#pragma once

#include "urd/elements.hpp"
#include "urd/interface.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/pools.hpp"
#include "urd/repr/pools_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
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

    /// No row's handle: an empty row is {0, 0}, and every other row has a size.
    [[nodiscard]] static constexpr TreeHandle none() noexcept { return { tree::unused_child, 0 }; }
    [[nodiscard]] constexpr bool is_none() const noexcept { return root == tree::unused_child && size == 0; }

    friend bool operator==(const TreeHandle&, const TreeHandle&) = default;
    static constexpr auto identifying_members() noexcept { return Members<&TreeHandle::root, &TreeHandle::size> {}; }
};

/// A pool of sequence rows (Row: a Store of values, or a vector), each a balanced tree with Arity children per node,
/// whose leaves are the row's values and whose nodes are interned: equal subtrees are stored once, across all rows.
/// Values with the size of a child index and a unique representation are stored inline in the child slots, so they
/// need no leaf table. A row written in a few positions shares all but those positions' paths to the root. With
/// ThreadSafe, threads may insert at once, and read rows already inserted while others insert.
template<typename Row, std::size_t Arity, bool ThreadSafe>
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

    /// The handle of the row, and whether inserting it stored a node (find would have given none).
    template<std::ranges::input_range Elements>
    std::pair<handle_type, bool> insert(const Elements& elements)
    {
        // A sequence row is its elements as given: nothing to put in order first.
        const auto values = std::vector<T>(std::ranges::begin(elements), std::ranges::end(elements));
        const auto size = static_cast<std::uint32_t>(values.size());
        if (values.empty())
            return { handle_type {}, false };
        auto stored = false;
        const auto root = insert_recursive(values, stored);
        return { handle_type { root, size }, stored };
    }
    /// The handle insert(elements) would give, where inserting would store nothing: every node of the row is held
    /// already, as a row or as a part of a longer one (an empty or one-element row needs none). Else none.
    template<std::ranges::input_range Elements>
    [[nodiscard]] handle_type find(const Elements& elements) const
    {
        const auto values = std::vector<T>(std::ranges::begin(elements), std::ranges::end(elements));
        if (values.empty())
            return handle_type {};
        const auto root = find_recursive(values);
        return root ? handle_type { *root, static_cast<std::uint32_t>(values.size()) } : handle_type::none();
    }

    /// The row `base` with `writes` applied, which are sorted by unique position within it. Only the paths from
    /// the written positions to the root are rebuilt.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const write_type> writes)
    {
        assert(std::ranges::is_sorted(writes, {}, &write_type::position));
        assert(std::ranges::adjacent_find(writes, {}, &write_type::position) == writes.end());
        assert(writes.empty() || writes.back().position < base.size);
        if (base.size == 0)
            return { base, false };
        auto stored = false;
        const auto root = insert_writes_recursive(base.root, 0, base.size, writes, stored);
        return { handle_type { root, base.size }, stored };
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
    // The inserts below set `stored` where they store a leaf or a node: per call, so threads never share it.
    std::uint32_t insert_leaf(const T& value, bool& stored)
    {
        if constexpr (inline_leaves)
            return std::bit_cast<std::uint32_t>(value);
        else
            return stored_at(m_leaves, m_leaves.insert(value), stored);
    }
    std::uint32_t insert_node(const Children& children, bool& stored) { return stored_at(m_nodes, m_nodes.insert(children), stored); }
    template<typename Set>
    static std::uint32_t stored_at(const Set& set, std::pair<typename Set::const_iterator, bool> inserted, bool& stored)
    {
        stored = stored || inserted.second;
        return position_of(set, inserted.first);
    }
    [[nodiscard]] std::optional<std::uint32_t> find_leaf(const T& value) const
    {
        if constexpr (inline_leaves)
            return std::bit_cast<std::uint32_t>(value);
        else
            return narrow(m_leaves.position(value));
    }
    [[nodiscard]] static std::optional<std::uint32_t> narrow(std::optional<std::size_t> position)
    { return position ? std::optional(static_cast<std::uint32_t>(*position)) : std::nullopt; }
    /// The position of `it` in `set`, taken after the insert that gave `it` (which may have moved the elements).
    template<typename Set>
    [[nodiscard]] static std::uint32_t position_of(const Set& set, typename Set::const_iterator it)
    { return static_cast<std::uint32_t>(it - set.begin()); }

    std::uint32_t insert_recursive(std::span<const T> values, bool& stored)
    {
        assert(!values.empty());
        if (values.size() == 1)
            return insert_leaf(values.front(), stored);

        const auto size = static_cast<std::uint32_t>(values.size());
        const auto step = tree::chunk<Arity>(size);
        auto children = Children {};
        children.fill(tree::unused_child);
        for (std::uint32_t k = 0, offset = 0; offset < size; ++k, offset += step)
            children[k] = insert_recursive(values.subspan(offset, std::min(step, size - offset)), stored);
        return insert_node(children, stored);
    }

    /// insert_recursive's root for `values`, looked up instead of stored: none where a node is missing.
    [[nodiscard]] std::optional<std::uint32_t> find_recursive(std::span<const T> values) const
    {
        assert(!values.empty());
        if (values.size() == 1)
            return find_leaf(values.front());

        const auto size = static_cast<std::uint32_t>(values.size());
        const auto step = tree::chunk<Arity>(size);
        auto children = Children {};
        children.fill(tree::unused_child);
        for (std::uint32_t k = 0, offset = 0; offset < size; ++k, offset += step)
        {
            const auto child = find_recursive(values.subspan(offset, std::min(step, size - offset)));
            if (!child)
                return std::nullopt;
            children[k] = *child;
        }
        return narrow(m_nodes.position(children));
    }

    /// Rebuilds the subtree `base_root` covering [offset, offset + size) with `writes` applied.
    std::uint32_t insert_writes_recursive(std::uint32_t base_root, std::uint32_t offset, std::uint32_t size, std::span<const write_type> writes, bool& stored)
    {
        assert(size > 0);
        if (writes.empty())
            return base_root;
        if (size == 1)
            return insert_leaf(writes.front().value, stored);

        auto children = m_nodes[base_root];
        const auto step = tree::chunk<Arity>(size);
        for (std::uint32_t k = 0, child_offset = offset; child_offset < offset + size; ++k, child_offset += step)
        {
            const auto child_size = std::min(step, offset + size - child_offset);
            const auto split = std::ranges::lower_bound(writes, std::size_t { child_offset } + child_size, {}, &write_type::position) - writes.begin();
            children[k] = insert_writes_recursive(children[k], child_offset, child_size, writes.first(static_cast<std::size_t>(split)), stored);
            writes = writes.subspan(static_cast<std::size_t>(split));
        }
        return insert_node(children, stored);
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

}  // namespace urd

namespace urd
{

template<>
struct include<"urd/repr/tree_vector.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<bool ThreadSafe, typename S>
struct representation_of<tree_pool_decl<false, ThreadSafe>, S>
{
    using type = repr::TreeVectorSet<typename S::element_type, 4, ThreadSafe>;
};

}  // namespace urd::detail
