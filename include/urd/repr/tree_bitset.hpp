#pragma once

#include "urd/elements.hpp"
#include "urd/interface.hpp"
#include "urd/repr/concurrent_indexed.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/pools.hpp"
#include "urd/repr/tree_vector.hpp"
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
#include <vector>

// Sets of indices as bitsets, each a balanced tree of 32-bit blocks whose nodes are interned, so that equal subtrees
// of different sets are stored once and all-zero subtrees not at all. Copied from yggdrasil's TreeBitsetSet
// (containers/tree_bitset_set.hpp) with its blocks fixed at the width of a child slot (stored inline), and changed
// to fit urd's pools: a row goes in as its elements, a delta as the elements added and removed.

namespace urd::repr
{

/// Toggles the bits in `mask` of the block at `position`.
struct BlockFlip
{
    std::uint32_t position = 0;
    std::uint32_t mask = 0;

    friend bool operator==(const BlockFlip&, const BlockFlip&) = default;
};

/// A pool of rows that are sets of indices (Row: a dense Store of keys), each a tree of 32-bit blocks with Arity
/// children per node. Block i always sits at position i, so changing a block only changes its path to the root; a
/// handle names the root and the number of blocks up to the last nonzero one, so equal sets have equal handles. With
/// ThreadSafe, threads may insert at once, and read rows already inserted while others insert.
template<typename Row, std::size_t Arity = 4, bool ThreadSafe = false>
    requires(std::has_single_bit(Arity) && Arity >= 2)
class TreeBitsetSet
{
    using K = std::ranges::range_value_t<Row>;
    using Block = std::uint32_t;
    using Bits = BitsetSpan<Block>;
    using ConstBits = BitsetSpan<const Block>;
    using Children = std::array<std::uint32_t, Arity>;
    using Nodes = InterningTable<Children, ThreadSafe>;
    /// The slot of an all-zero subtree of more than one block. A zero block is the value 0.
    static constexpr std::uint32_t zero_subtree = std::numeric_limits<std::uint32_t>::max();

public:
    using value_type = Row;
    using handle_type = TreeHandle;
    using size_type = std::size_t;

    static constexpr std::size_t arity = Arity;

    // The rows, as their elements.

    template<std::ranges::input_range Elements>
    handle_type insert(const Elements& elements)
    {
        auto bits = std::vector<std::size_t>();
        for (const auto& element : elements)
            bits.push_back(index_of<K>::to_index(K(element)));
        const auto width = bits.empty() ? std::size_t { 0 } : std::ranges::max(bits) + 1;
        auto blocks = std::vector<Block>(Bits::num_blocks(width));
        auto row = Bits(blocks.data(), width);
        for (const auto bit : bits)
            row.set(bit);
        return insert_blocks(blocks);
    }
    /// The row `base` with `added` put in and then `removed` taken out. Only the paths from the changed blocks, and
    /// from blocks at a changed length, to the root are rebuilt.
    handle_type insert(handle_type base, std::span<const K> added, std::span<const K> removed)
    {
        // Each element once, by its index: whether it ends up in the row decides whether its bit flips.
        auto touched = std::vector<std::size_t>();
        for (const auto& element : added)
            touched.push_back(index_of<K>::to_index(element));
        for (const auto& element : removed)
            touched.push_back(index_of<K>::to_index(element));
        std::ranges::sort(touched);
        touched.erase(std::ranges::unique(touched).begin(), touched.end());
        const auto listed = [](std::span<const K> elements, std::size_t bit)
        { return std::ranges::any_of(elements, [bit](const K& each) { return index_of<K>::to_index(each) == bit; }); };
        auto flips = std::vector<BlockFlip>();
        for (const auto bit : touched)
        {
            const bool was = contains(base, index_of<K>::from_index(bit));
            const bool is = !listed(removed, bit) && (listed(added, bit) || was);
            if (is != was)
                flips.push_back({ static_cast<std::uint32_t>(Bits::block_index(bit)), Block { 1 } << Bits::block_pos(bit) });
        }
        normalize(flips);
        return insert_flips(base, flips);
    }
    /// Whether the row of `handle` holds `element`, read along one path, not by reading the row.
    [[nodiscard]] bool contains(handle_type handle, const K& element) const
    {
        if (!index_of<K>::is_index(element))
            return false;
        const auto bit = index_of<K>::to_index(element);
        auto position = static_cast<std::uint32_t>(Bits::block_index(bit));
        if (position >= handle.size)
            return false;
        auto block = handle.root;
        auto size = handle.size;
        while (size > 1)
        {
            if (block == zero_subtree)
                return false;
            const auto step = tree::chunk<Arity>(size);
            const auto k = position / step;
            block = m_nodes[block][k];
            position -= k * step;
            size = child_size(size, k);
        }
        return ConstBits(&block, Bits::Digits).test(Bits::block_pos(bit));
    }

    [[nodiscard]] Row get(handle_type handle) const
    {
        auto elements = std::vector<K>(row_size(handle));
        read(handle, elements);
        return Row(elements.begin(), elements.end());
    }
    /// How many elements the row holds: the set bits.
    [[nodiscard]] size_type row_size(handle_type handle) const
    {
        const auto blocks = read_all(handle);
        return ConstBits(blocks.data(), blocks.size() * Bits::Digits).count();
    }
    /// The row's elements, in key order, into `out`, which must hold row_size(handle) of them.
    void read(handle_type handle, std::span<K> out) const
    {
        const auto blocks = read_all(handle);
        const auto row = ConstBits(blocks.data(), blocks.size() * Bits::Digits);
        if (out.size() != row.count())
            throw std::invalid_argument("urd::repr::TreeBitsetSet::read: the output must hold the row");
        std::ranges::transform(set_bit_indices(row), out.begin(), [](std::size_t bit) { return index_of<K>::from_index(bit); });
    }

    // The rows, as their blocks, as yggdrasil's TreeBitsetSet offers them.

    /// The handle of the row with these blocks, whose last block must not be zero.
    handle_type insert_blocks(std::span<const Block> blocks)
    {
        if (!blocks.empty() && blocks.back() == Block { 0 })
            throw std::invalid_argument("urd::repr::TreeBitsetSet::insert_blocks: the last block must not be zero");
        const auto size = static_cast<std::uint32_t>(blocks.size());
        return blocks.empty() ? handle_type {} : handle_type { insert_recursive(blocks), size };
    }
    /// The row `base` with `flips` applied, sorted by unique position; trimmed to its last nonzero block.
    handle_type insert_flips(handle_type base, std::span<const BlockFlip> flips)
    {
        assert(std::ranges::is_sorted(flips, {}, &BlockFlip::position));
        assert(std::ranges::adjacent_find(flips, {}, &BlockFlip::position) == flips.end());

        const auto upper = flips.empty() ? base.size : std::max(base.size, flips.back().position + 1);
        if (upper == 0)
            return handle_type {};

        auto cursor = BaseCursor(m_nodes, base);
        const auto root = insert_delta_recursive(cursor, false, zero(upper), 0, upper, flips);
        const auto size = trimmed_size(root, upper);
        if (size == upper)
            return handle_type { root, size };
        if (size == 0)
            return handle_type {};

        // Cleared trailing blocks: rebuild the right spine for the shorter size, reusing all other subtrees.
        auto trimmed = BaseCursor(m_nodes, handle_type { root, upper });
        return handle_type { insert_delta_recursive(trimmed, false, zero(size), 0, size, {}), size };
    }
    /// The row's blocks into `blocks`, which must hold handle.size of them.
    void read_blocks(handle_type handle, std::span<Block> blocks) const
    {
        if (blocks.size() != handle.size)
            throw std::invalid_argument("urd::repr::TreeBitsetSet::read_blocks: the output must hold the row's blocks");
        if (!blocks.empty())
            read_recursive(handle.root, blocks);
    }

    /// Sorts flips by position and combines those of one block, dropping those that cancel out.
    static void normalize(std::vector<BlockFlip>& flips)
    {
        std::ranges::sort(flips, {}, &BlockFlip::position);
        auto last = std::size_t { 0 };
        for (const auto& flip : flips)
        {
            if (last > 0 && flips[last - 1].position == flip.position)
                flips[last - 1].mask ^= flip.mask;
            else
                flips[last++] = flip;
        }
        flips.resize(last);
        std::erase_if(flips, [](const BlockFlip& flip) { return flip.mask == 0; });
    }

    [[nodiscard]] size_type memory_usage() const { return m_nodes.memory_usage(); }
    [[nodiscard]] size_type num_nodes() const { return m_nodes.size(); }
    [[nodiscard]] bool empty() const { return m_nodes.empty(); }
    void clear() { m_nodes.clear(); }

private:
    static constexpr std::uint32_t zero(std::uint32_t size) { return size == 1 ? 0 : zero_subtree; }
    static constexpr std::uint32_t child_size(std::uint32_t size, std::uint32_t k)
    { return std::min(tree::chunk<Arity>(size), size - k * tree::chunk<Arity>(size)); }
    static Children unused_children()
    {
        auto children = Children {};
        children.fill(tree::unused_child);
        return children;
    }

    [[nodiscard]] std::vector<Block> read_all(handle_type handle) const
    {
        auto blocks = std::vector<Block>(handle.size);
        read_blocks(handle, blocks);
        return blocks;
    }

    /// Stores the node of a subtree covering `size` blocks, unless all its children are zero.
    std::uint32_t insert_node(const Children& children, std::uint32_t size)
    {
        const auto count = tree::num_children<Arity>(size);
        for (std::uint32_t k = 0; k < count; ++k)
            if (children[k] != zero(child_size(size, k)))
            {
                const auto found = m_nodes.insert(children).first;
                return static_cast<std::uint32_t>(found - m_nodes.begin());
            }
        return zero_subtree;
    }

    std::uint32_t insert_recursive(std::span<const Block> blocks)
    {
        assert(!blocks.empty());
        if (blocks.size() == 1)
            return blocks.front();
        const auto size = static_cast<std::uint32_t>(blocks.size());
        auto children = unused_children();
        for (std::uint32_t k = 0, offset = 0; offset < size; ++k, offset += tree::chunk<Arity>(size))
            children[k] = insert_recursive(blocks.subspan(offset, child_size(size, k)));
        return insert_node(children, size);
    }

    /// Finds subtrees of a base row for the ranges that insert_delta_recursive visits. These arrive in depth-first
    /// order, so the cursor keeps its last descent path and only climbs as far as the next range requires.
    class BaseCursor
    {
    public:
        BaseCursor(const Nodes& nodes, handle_type base) : m_nodes(&nodes), m_base_size(base.size) { m_path[0] = Frame { base.root, 0, base.size }; }

        /// The subtree of the base covering exactly [offset, offset + size), or nothing if the base has none. Ranges
        /// inside all-zero subtrees, or beyond the base, are zero.
        std::optional<std::uint32_t> find(std::uint32_t offset, std::uint32_t size)
        {
            if (offset >= m_base_size)
                return zero(size);
            if (offset + size > m_base_size)
                return std::nullopt;
            while (!m_path[m_depth].contains(offset, size))  // the root frame contains every range inside the base
                --m_depth;
            while (true)
            {
                const auto frame = m_path[m_depth];
                if (frame.size == size)
                    return frame.root;
                if (frame.root == zero_subtree)
                    return zero(size);
                const auto& node = (*m_nodes)[frame.root];
                const auto k = (offset - frame.offset) / tree::chunk<Arity>(frame.size);
                const auto child_offset = frame.offset + k * tree::chunk<Arity>(frame.size);
                const auto child_blocks = child_size(frame.size, k);
                if (offset + size > child_offset + child_blocks)
                    return std::nullopt;
                m_path[++m_depth] = Frame { node[k], child_offset, child_blocks };
            }
        }

    private:
        struct Frame
        {
            std::uint32_t root = 0;
            std::uint32_t offset = 0;
            std::uint32_t size = 0;

            [[nodiscard]] bool contains(std::uint32_t other_offset, std::uint32_t other_size) const
            { return offset <= other_offset && other_offset + other_size <= offset + size; }
        };

        const Nodes* m_nodes;
        std::uint32_t m_base_size;
        std::array<Frame, std::numeric_limits<std::uint32_t>::digits + 2> m_path {};  // bounded by the tree's height
        std::size_t m_depth = 0;
    };

    /// Builds the subtree for [offset, offset + size) of the new row. `base_root` covers the same range in the base
    /// where `base_known`; otherwise it is looked up through `base`.
    std::uint32_t insert_delta_recursive(BaseCursor& base,
                                         bool base_known,
                                         std::uint32_t base_root,
                                         std::uint32_t offset,
                                         std::uint32_t size,
                                         std::span<const BlockFlip> flips)
    {
        assert(size > 0);
        if (!base_known)
            if (const auto root = base.find(offset, size))
            {
                base_known = true;
                base_root = *root;
            }
        if (flips.empty() && base_known)
            return base_root;
        if (size == 1)
        {
            assert(base_known);
            return flips.empty() ? base_root : base_root ^ flips.front().mask;
        }

        const auto count = tree::num_children<Arity>(size);
        auto children = unused_children();
        if (base_known && base_root != zero_subtree)
            children = m_nodes[base_root];
        else
            for (std::uint32_t k = 0; k < count; ++k)
                children[k] = zero(child_size(size, k));

        for (std::uint32_t k = 0, child_offset = offset; k < count; ++k, child_offset += tree::chunk<Arity>(size))
        {
            const auto child_blocks = child_size(size, k);
            const auto split = std::ranges::lower_bound(flips, child_offset + child_blocks, {}, &BlockFlip::position) - flips.begin();
            children[k] = insert_delta_recursive(base, base_known, children[k], child_offset, child_blocks, flips.first(static_cast<std::size_t>(split)));
            flips = flips.subspan(static_cast<std::size_t>(split));
        }
        return insert_node(children, size);
    }

    /// The number of blocks up to and including the last nonzero block of the subtree.
    [[nodiscard]] std::uint32_t trimmed_size(std::uint32_t root, std::uint32_t size) const
    {
        auto offset = std::uint32_t { 0 };
        while (root != zero(size))
        {
            if (size == 1)
                return offset + 1;
            // The last nonzero child, which exists since insert_node never stores all-zero children.
            const auto& node = m_nodes[root];
            auto k = tree::num_children<Arity>(size) - 1;
            while (node[k] == zero(child_size(size, k)))
                --k;
            root = node[k];
            offset += k * tree::chunk<Arity>(size);
            size = child_size(size, k);
        }
        return offset;
    }

    void read_recursive(std::uint32_t root, std::span<Block> blocks) const
    {
        assert(!blocks.empty());
        if (blocks.size() == 1)
        {
            blocks.front() = root;
            return;
        }
        if (root == zero_subtree)
        {
            std::ranges::fill(blocks, Block { 0 });
            return;
        }
        const auto& node = m_nodes[root];
        const auto size = static_cast<std::uint32_t>(blocks.size());
        for (std::uint32_t k = 0, offset = 0; offset < size; ++k, offset += tree::chunk<Arity>(size))
            read_recursive(node[k], blocks.subspan(offset, child_size(size, k)));
    }

    Nodes m_nodes;
};

}  // namespace urd::repr

namespace urd
{

/// Rows that are sets of indices, sharing equal subtrees; clear keeps the table's storage.
template<typename Row, std::size_t Arity, bool ThreadSafe>
    requires indexable<std::ranges::range_value_t<Row>> && has<traits_t<Row>, unique, dense>
struct container_traits<repr::TreeBitsetSet<Row, Arity, ThreadSafe>> :
    properties_of<handles, unique, structural_sharing, shrinkable, retains, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

template<typename Row, std::size_t Arity, bool ThreadSafe>
struct element_traits<repr::TreeBitsetSet<Row, Arity, ThreadSafe>> : container_traits<Row>
{
};

}  // namespace urd
