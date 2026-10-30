#pragma once

#include "urd/elements.hpp"
#include "urd/interface.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/pools.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

// Rows as balanced trees whose nodes are interned, so equal subtrees of different rows are stored once. Copied
// from yggdrasil's TreeVectorSet (containers/tree_vector_set.hpp) and changed to fit urd's pools: nodes and leaves
// are interned in IndexedSets, rows go in as any range and come back as the pool's row type.

namespace urd::repr
{

namespace tree
{

/// The children of a subtree covering `size` >= 2 positions each cover `chunk<Arity>(size)` positions, except that
/// the last may cover fewer: the smallest power of Arity whose Arity-fold covers `size`. Positions of a block never
/// depend on later positions, so changing a position only changes its path to the root. For Arity 2 this is
/// bit_floor(size - 1).
template<std::size_t Arity>
    requires(std::has_single_bit(Arity) && Arity >= 2)
constexpr std::uint32_t chunk(std::uint32_t size) noexcept
{
    assert(size >= 2);
    constexpr auto bits = std::countr_zero(Arity);
    const auto levels = (std::bit_width(size - 1) + bits - 1) / bits;
    return std::uint32_t { 1 } << (bits * (levels - 1));
}

/// The number of children used by a subtree covering `size` >= 2 positions, in [2, Arity].
template<std::size_t Arity>
constexpr std::uint32_t num_children(std::uint32_t size) noexcept
{
    const auto step = chunk<Arity>(size);
    return (size + step - 1) / step;
}

/// The value of child slots beyond the end of a subtree, which only occur in its last node per level. Fixed so
/// that equal rows are equal trees; never read.
inline constexpr std::uint32_t unused_child = std::numeric_limits<std::uint32_t>::max();

/// Stands in for the leaf table where leaves are stored inline in their parents' child slots.
struct NoLeaves
{
    static constexpr std::size_t size() noexcept { return 0; }
    static constexpr std::size_t memory_usage() noexcept { return 0; }
    static void clear() noexcept {}
};

}  // namespace tree

/// The handle of a row in a tree pool: its root and its length.
struct TreeHandle
{
    std::uint32_t root = 0;
    std::uint32_t size = 0;

    friend bool operator==(const TreeHandle&, const TreeHandle&) = default;
    static constexpr auto identifying_members() noexcept { return Members<&TreeHandle::root, &TreeHandle::size> {}; }
};

/// A pool of sequence rows (Row: a Store of values, or a vector), each a balanced tree with Arity children per node,
/// whose leaves are the row's values and whose nodes are interned: equal subtrees are stored once, across all rows.
/// Values with the size of a child index and a unique representation are stored inline in the child slots, so they
/// need no leaf table. A row written in a few positions shares all but those positions' paths to the root. With
/// ThreadSafe, threads may insert at once, and read rows already inserted while others insert.
template<typename Row, std::size_t Arity = 4, bool ThreadSafe = false>
    requires(std::has_single_bit(Arity) && Arity >= 2)
class TreeVectorSet
{
    using T = std::ranges::range_value_t<Row>;
    using Children = std::array<std::uint32_t, Arity>;
    // Unique object representations make bitwise identity agree with equality (rules out float, padding).
    static constexpr bool inline_leaves = std::has_unique_object_representations_v<T> && sizeof(T) == sizeof(std::uint32_t);
    using Leaves = std::conditional_t<inline_leaves, tree::NoLeaves, InterningTable<T, ThreadSafe>>;

public:
    using value_type = Row;
    using handle_type = TreeHandle;
    using write_type = Write<T>;
    using size_type = std::size_t;

    static constexpr std::size_t arity = Arity;

    template<std::ranges::input_range Elements>
    handle_type insert(const Elements& elements)
    {
        // A sequence row is its elements as given: nothing to put in order first.
        const auto values = std::vector<T>(std::ranges::begin(elements), std::ranges::end(elements));
        const auto size = static_cast<std::uint32_t>(values.size());
        return values.empty() ? handle_type {} : handle_type { insert_recursive(values), size };
    }

    /// The row `base` with `writes` applied, which are sorted by unique position within it. Only the paths from
    /// the written positions to the root are rebuilt.
    handle_type insert(handle_type base, std::span<const write_type> writes)
    {
        assert(std::ranges::is_sorted(writes, {}, &write_type::position));
        assert(std::ranges::adjacent_find(writes, {}, &write_type::position) == writes.end());
        assert(writes.empty() || writes.back().position < base.size);
        return base.size == 0 ? base : handle_type { insert_writes_recursive(base.root, 0, base.size, writes), base.size };
    }

    [[nodiscard]] Row get(handle_type handle) const
    {
        auto values = std::vector<T>(handle.size);
        read(handle, values);
        return Row(values.begin(), values.end());
    }
    [[nodiscard]] size_type row_size(handle_type handle) const { return handle.size; }
    /// The row's values into `out`, which must hold row_size(handle) of them.
    void read(handle_type handle, std::span<T> out) const
    {
        if (out.size() != handle.size)
            throw std::invalid_argument("urd::repr::TreeVectorSet::read: the output must hold the row");
        if (!out.empty())
            read_recursive(handle.root, out);
    }

    [[nodiscard]] size_type memory_usage() const { return m_leaves.memory_usage() + m_nodes.memory_usage(); }
    [[nodiscard]] size_type num_leaves() const { return m_leaves.size(); }
    [[nodiscard]] size_type num_nodes() const { return m_nodes.size(); }
    [[nodiscard]] bool empty() const { return m_leaves.size() == 0 && m_nodes.empty(); }
    void clear()
    {
        m_nodes.clear();
        m_leaves.clear();
    }

private:
    std::uint32_t insert_leaf(const T& value)
    {
        if constexpr (inline_leaves)
            return std::bit_cast<std::uint32_t>(value);
        else
            return position_of(m_leaves, m_leaves.insert(value).first);
    }
    std::uint32_t insert_node(const Children& children) { return position_of(m_nodes, m_nodes.insert(children).first); }
    /// The position of `it` in `set`, taken after the insert that gave `it` (which may have moved the elements).
    template<typename Set>
    [[nodiscard]] static std::uint32_t position_of(const Set& set, typename Set::const_iterator it)
    {
        return static_cast<std::uint32_t>(it - set.begin());
    }

    std::uint32_t insert_recursive(std::span<const T> values)
    {
        assert(!values.empty());
        if (values.size() == 1)
            return insert_leaf(values.front());

        const auto size = static_cast<std::uint32_t>(values.size());
        const auto step = tree::chunk<Arity>(size);
        auto children = Children {};
        children.fill(tree::unused_child);
        for (std::uint32_t k = 0, offset = 0; offset < size; ++k, offset += step)
            children[k] = insert_recursive(values.subspan(offset, std::min(step, size - offset)));
        return insert_node(children);
    }

    /// Rebuilds the subtree `base_root` covering [offset, offset + size) with `writes` applied.
    std::uint32_t insert_writes_recursive(std::uint32_t base_root, std::uint32_t offset, std::uint32_t size, std::span<const write_type> writes)
    {
        assert(size > 0);
        if (writes.empty())
            return base_root;
        if (size == 1)
            return insert_leaf(writes.front().value);

        auto children = m_nodes[base_root];
        const auto step = tree::chunk<Arity>(size);
        for (std::uint32_t k = 0, child_offset = offset; child_offset < offset + size; ++k, child_offset += step)
        {
            const auto child_size = std::min(step, offset + size - child_offset);
            const auto split = std::ranges::lower_bound(writes, std::size_t { child_offset } + child_size, {}, &write_type::position) - writes.begin();
            children[k] = insert_writes_recursive(children[k], child_offset, child_size, writes.first(static_cast<std::size_t>(split)));
            writes = writes.subspan(static_cast<std::size_t>(split));
        }
        return insert_node(children);
    }

    void read_recursive(std::uint32_t root, std::span<T> values) const
    {
        assert(!values.empty());
        if (values.size() == 1)
        {
            if constexpr (inline_leaves)
                values.front() = std::bit_cast<T>(root);
            else
                values.front() = m_leaves[root];
            return;
        }

        const auto& node = m_nodes[root];
        const auto size = static_cast<std::uint32_t>(values.size());
        const auto step = tree::chunk<Arity>(size);
        for (std::uint32_t k = 0, offset = 0; offset < size; ++k, offset += step)
            read_recursive(node[k], values.subspan(offset, std::min(step, size - offset)));
    }

    [[no_unique_address]] Leaves m_leaves;
    InterningTable<Children, ThreadSafe> m_nodes;
};

}  // namespace urd::repr

namespace urd
{

/// Rows of hashable values, sharing equal subtrees; clear keeps the tables' storage.
template<typename Row, std::size_t Arity, bool ThreadSafe>
    requires hashable<detail::row_element_t<Row>> && (!by_value<traits_t<Row>>)
struct container_traits<repr::TreeVectorSet<Row, Arity, ThreadSafe>> :
    properties_of<handles, unique, structural_sharing, shrinkable, retains, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

template<typename Row, std::size_t Arity, bool ThreadSafe>
struct element_traits<repr::TreeVectorSet<Row, Arity, ThreadSafe>> : container_traits<Row>
{
};

}  // namespace urd
