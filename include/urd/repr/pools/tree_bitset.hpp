#pragma once

#include "urd/config.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/pools/pools.hpp"
#include "urd/repr/pools/pools_fwd.hpp"
#include "urd/repr/pools/tree_vector.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

// Sets of indices as bitsets, each a balanced tree of 32-bit blocks whose nodes are interned, so that equal subtrees
// of different sets are stored once and all-zero subtrees not at all. Blocks are
// fixed at the width of a child slot (stored inline); a row goes in as its elements, a delta as the elements added and
// removed.

namespace urd::repr
{

/// Toggles the bits in `mask` of the block at `position`.
struct BlockFlip
{
    std::uint32_t position = 0;
    std::uint32_t mask = 0;

    friend bool operator==(const BlockFlip&, const BlockFlip&) = default;
};

}  // namespace urd::repr

namespace urd
{

/// A tree bitset's block flip, for code outside urd.
using block_flip = repr::BlockFlip;

}  // namespace urd

namespace urd::repr
{

/// A pool of rows that are sets of indices (Row: a dense Store of keys), each a tree of 32-bit blocks with Arity
/// children per node. Block i always sits at position i, so changing a block only changes its path to the root; a
/// handle names the root and the number of blocks up to the last nonzero one, so equal sets have equal handles. With
/// ThreadSafe, threads may insert at once, and read rows already inserted while others insert.
template<typename Row, std::size_t Arity, bool ThreadSafe>
    requires(std::has_single_bit(Arity) && Arity >= 2)
class TreeBitsetSet
{
    using K = std::ranges::range_value_t<Row>;
    using Block = std::uint32_t;
    /// The blocks a word of a row's bits holds (urd/config.hpp): block i of a row is part i % blocks_per_word of word
    /// i / blocks_per_word, low parts first.
    static constexpr std::size_t blocks_per_word = word_bits / std::numeric_limits<Block>::digits;
    using Bits = BitsetSpan<Block>;
    using ConstBits = BitsetSpan<const Block>;
    using Children = std::array<std::uint32_t, Arity>;
    using Nodes = InterningTable<Children, ThreadSafe>;
    /// The slot of an all-zero subtree of more than one block. A zero block is the value 0.
    static constexpr std::uint32_t zero_subtree = std::numeric_limits<std::uint32_t>::max();
    /// Whether a row keeps its bits as words (urd::word_t: a dense set), which it hands out up to its last nonzero one.
    template<typename Elements>
    static constexpr bool keeps_words = requires(const Elements& row) {
        { row.significant_words() } -> std::convertible_to<std::span<const word_t>>;
    };

public:
    /// A row's blocks, and a change to one of them, as the block interface takes them.
    using block_type = Block;
    using flip_type = BlockFlip;
    using value_type = Row;
    using handle_type = TreeHandle;
    using size_type = std::size_t;

    static constexpr std::size_t arity = Arity;

    // The rows, as their elements.

    /// The handle of the row, and whether inserting it stored a node (find would have given none).
    template<std::ranges::input_range Elements>
    std::pair<handle_type, bool> insert(const Elements& elements)
    { return store_blocks(std::span<const Block>(blocks_of(elements))); }
    /// A row that keeps its bits (a dense set: significant_words()), by its words, read as blocks in place:
    /// allocates nothing.
    template<std::ranges::input_range Elements>
        requires keeps_words<Elements>
    std::pair<handle_type, bool> insert(const Elements& row)
    { return store_blocks(WordBlocks(row.significant_words())); }
    /// The handle insert(elements) would give, where inserting would store nothing: every node of the row is held
    /// already, as a row or as a part of a longer one (all-zero parts and one-block rows need none). Else none.
    template<std::ranges::input_range Elements>
    [[nodiscard]] handle_type find(const Elements& elements) const
    {
        const auto blocks = blocks_of(elements);
        return find_blocks(std::span<const Block>(blocks));
    }
    template<std::ranges::input_range Elements>
        requires keeps_words<Elements>
    [[nodiscard]] handle_type find(const Elements& row) const
    { return find_blocks(WordBlocks(row.significant_words())); }
    /// The row `base` with `added` put in and then `removed` taken out. Only the paths from the changed blocks, and
    /// from blocks at a changed length, to the root are rebuilt.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const K> added, std::span<const K> removed)
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
        return store_flips(base, flips);
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
        copy(handle, elements);
        return Row(elements.begin(), elements.end());
    }
    /// How many elements the row holds: the set bits, read along the tree; allocates nothing.
    [[nodiscard]] size_type row_size(handle_type handle) const
    {
        auto count = size_type { 0 };
        for_each_block(handle, [&count](std::uint32_t /*position*/, Block block) { count += static_cast<size_type>(std::popcount(block)); });
        return count;
    }
    /// The row's elements, in key order, into `out`, which must hold row_size(handle) of them; allocates nothing.
    void copy(handle_type handle, std::span<K> out) const
    {
        if (out.size() != row_size(handle))
            throw std::invalid_argument("urd::repr::TreeBitsetSet::read: the output must hold the row");
        auto next = out.begin();
        for_each_block(handle,
                       [&next](std::uint32_t position, Block block)
                       {
                           for (; block != 0; block &= block - 1)
                               *next++ = index_of<K>::from_index(std::size_t { position } * Bits::Digits + static_cast<std::size_t>(std::countr_zero(block)));
                       });
    }
    /// The row into `out`, a row that takes its bits as words (a dense set: assign_words), word by word into its
    /// storage, which is reused: allocates nothing where `out` holds as many words already.
    void copy(handle_type handle, Row& out) const
        requires requires(Row& row, void (*fill)(std::span<word_t>)) { row.assign_words(std::size_t {}, fill); }
    {
        out.assign_words(bit::ceil_div<std::size_t>(handle.size, blocks_per_word),
                         [this, handle](std::span<word_t> words)
                         {
                             for_each_block(handle,
                                            [&words](std::uint32_t position, Block block)
                                            { words[position / blocks_per_word] |= word_t { block } << (Bits::Digits * (position % blocks_per_word)); });
                         });
    }

    // The rows, as their blocks, as yggdrasil's TreeBitsetSet offers them.

    /// The handle of the row with these blocks, whose last block must not be zero.
    handle_type insert_blocks(std::span<const Block> blocks) { return store_blocks(blocks).first; }
    /// The row `base` with `flips` applied, sorted by unique position; trimmed to its last nonzero block.
    handle_type insert_flips(handle_type base, std::span<const BlockFlip> flips) { return store_flips(base, flips).first; }
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

    /// Words read as the blocks they hold (blocks_per_word each, low parts first), without a copy; up to the last nonzero
    /// block. What insert_recursive and find_recursive read, as they read a span.
    class WordBlocks
    {
    public:
        explicit WordBlocks(std::span<const word_t> words) : m_words(words), m_count(blocks_per_word * words.size())
        {
            while (m_count > 0 && (*this)[m_count - 1] == Block { 0 })
                --m_count;
        }
        [[nodiscard]] std::size_t size() const noexcept { return m_count; }
        [[nodiscard]] bool empty() const noexcept { return m_count == 0; }
        [[nodiscard]] Block operator[](std::size_t i) const noexcept
        {
            const auto at = m_first + i;
            return static_cast<Block>(m_words[at / blocks_per_word] >> (Bits::Digits * (at % blocks_per_word)));
        }
        [[nodiscard]] Block front() const noexcept { return (*this)[0]; }
        [[nodiscard]] Block back() const noexcept { return (*this)[m_count - 1]; }
        [[nodiscard]] WordBlocks subspan(std::size_t offset, std::size_t count) const noexcept
        {
            auto part = *this;
            part.m_first += offset;
            part.m_count = count;
            return part;
        }

    private:
        std::span<const word_t> m_words;
        std::size_t m_first = 0;
        std::size_t m_count = 0;
    };

    /// Calls f(position, block) for each nonzero block of the row, in order, along the tree; allocates nothing.
    template<typename F>
    void for_each_block(handle_type handle, F f) const
    {
        if (handle.size > 0)
            for_each_block(handle.root, 0, handle.size, f);
    }
    template<typename F>
    void for_each_block(std::uint32_t root, std::uint32_t offset, std::uint32_t size, F& f) const
    {
        if (size == 1)
        {
            if (root != Block { 0 })
                f(offset, Block { root });
            return;
        }
        if (root == zero_subtree)
            return;
        const auto& node = m_nodes[root];
        for (std::uint32_t k = 0, at = 0; at < size; ++k, at += tree::chunk<Arity>(size))
            for_each_block(node[k], offset + at, child_size(size, k), f);
    }

    /// The handle of the row with these blocks (a span, or WordBlocks), where every node is held; else none.
    template<typename Blocks>
    [[nodiscard]] handle_type find_blocks(const Blocks& blocks) const
    {
        if (blocks.empty())
            return handle_type {};
        const auto root = find_recursive(blocks);
        return root ? handle_type { *root, static_cast<std::uint32_t>(blocks.size()) } : handle_type::none();
    }

    /// The blocks of the row holding `elements`, up to its last nonzero block.
    template<std::ranges::input_range Elements>
    [[nodiscard]] static std::vector<Block> blocks_of(const Elements& elements)
    {
        auto bits = std::vector<std::size_t>();
        for (const auto& element : elements)
            bits.push_back(index_of<K>::to_index(K(element)));
        const auto width = bits.empty() ? std::size_t { 0 } : std::ranges::max(bits) + 1;
        auto blocks = std::vector<Block>(Bits::num_blocks(width));
        auto row = Bits(blocks.data(), width);
        for (const auto bit : bits)
            row.set(bit);
        return blocks;
    }

    /// insert_blocks and insert_flips, with whether they stored a node. `stored` is per call: threads never share it.
    template<typename Blocks>
    std::pair<handle_type, bool> store_blocks(const Blocks& blocks)
    {
        if (!blocks.empty() && blocks.back() == Block { 0 })
            throw std::invalid_argument("urd::repr::TreeBitsetSet::insert_blocks: the last block must not be zero");
        const auto size = static_cast<std::uint32_t>(blocks.size());
        if (blocks.empty())
            return { handle_type {}, false };
        auto stored = false;
        const auto root = insert_recursive(blocks, stored);
        return { handle_type { root, size }, stored };
    }
    std::pair<handle_type, bool> store_flips(handle_type base, std::span<const BlockFlip> flips)
    {
        assert(std::ranges::is_sorted(flips, {}, &BlockFlip::position));
        assert(std::ranges::adjacent_find(flips, {}, &BlockFlip::position) == flips.end());

        const auto upper = flips.empty() ? base.size : std::max(base.size, flips.back().position + 1);
        if (upper == 0)
            return { handle_type {}, false };

        auto stored = false;
        auto spine = FreshSpine();
        auto cursor = BaseCursor(m_nodes, base);
        const auto root = insert_delta_recursive(cursor, false, zero(upper), 0, upper, flips, stored, &spine);
        const auto size = trimmed_size(root, upper);
        if (size == upper)
            return { handle_type { root, size }, stored };
        if (size == 0)
            return { handle_type {}, false };

        // Cleared trailing blocks: rebuild the right spine for the shorter size, reusing all other subtrees. The first
        // pass stored the untrimmed row's spine, which the row does not keep: what this one stores says whether the row
        // is new, or else whether its root is a node the first pass stored (a fresh node of the row has fresh
        // ancestors up to the root, and the root covers offset 0).
        auto trimmed = BaseCursor(m_nodes, handle_type { root, upper });
        auto rebuilt = false;
        const auto shorter = insert_delta_recursive(trimmed, false, zero(size), 0, size, {}, rebuilt, nullptr);
        return { handle_type { shorter, size }, rebuilt || (size > 1 && spine.holds(shorter)) };
    }

    /// Stores the node of a subtree covering `size` blocks, unless all its children are zero.
    std::uint32_t insert_node(const Children& children, std::uint32_t size, bool& stored)
    {
        const auto count = tree::num_children<Arity>(size);
        for (std::uint32_t k = 0; k < count; ++k)
            if (children[k] != zero(child_size(size, k)))
            {
                const auto [found, inserted] = m_nodes.insert(children);
                stored = stored || inserted;
                return static_cast<std::uint32_t>(found - m_nodes.begin());
            }
        return zero_subtree;
    }

    /// insert_recursive's root for `blocks`, looked up instead of stored: none where a node is missing.
    template<typename Blocks>
    [[nodiscard]] std::optional<std::uint32_t> find_recursive(const Blocks& blocks) const
    {
        assert(!blocks.empty());
        if (blocks.size() == 1)
            return blocks.front();
        const auto size = static_cast<std::uint32_t>(blocks.size());
        auto children = unused_children();
        for (std::uint32_t k = 0, offset = 0; offset < size; ++k, offset += tree::chunk<Arity>(size))
        {
            const auto child = find_recursive(blocks.subspan(offset, child_size(size, k)));
            if (!child)
                return std::nullopt;
            children[k] = *child;
        }
        const auto count = tree::num_children<Arity>(size);
        for (std::uint32_t k = 0; k < count; ++k)
            if (children[k] != zero(child_size(size, k)))
            {
                const auto found = m_nodes.position(children);
                return found ? std::optional(static_cast<std::uint32_t>(*found)) : std::nullopt;
            }
        return zero_subtree;
    }

    template<typename Blocks>
    std::uint32_t insert_recursive(const Blocks& blocks, bool& stored)
    {
        assert(!blocks.empty());
        if (blocks.size() == 1)
            return blocks.front();
        const auto size = static_cast<std::uint32_t>(blocks.size());
        auto children = unused_children();
        for (std::uint32_t k = 0, offset = 0; offset < size; ++k, offset += tree::chunk<Arity>(size))
            children[k] = insert_recursive(blocks.subspan(offset, child_size(size, k)), stored);
        return insert_node(children, size, stored);
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

    /// The nodes a delta stored at offset 0, one per level at most: the only ones a trimmed row's root can be.
    struct FreshSpine
    {
        std::array<std::uint32_t, std::numeric_limits<std::uint32_t>::digits + 2> nodes {};
        std::size_t count = 0;

        void add(std::uint32_t node)
        {
            assert(count < nodes.size());
            nodes[count++] = node;
        }
        [[nodiscard]] bool holds(std::uint32_t node) const
        {
            const auto used = std::span(nodes).first(count);
            return std::ranges::find(used, node) != used.end();
        }
    };

    /// Builds the subtree for [offset, offset + size) of the new row. `base_root` covers the same range in the base
    /// where `base_known`; otherwise it is looked up through `base`.
    std::uint32_t insert_delta_recursive(BaseCursor& base,
                                         bool base_known,
                                         std::uint32_t base_root,
                                         std::uint32_t offset,
                                         std::uint32_t size,
                                         std::span<const BlockFlip> flips,
                                         bool& stored,
                                         FreshSpine* spine)
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
            children[k] =
                insert_delta_recursive(base, base_known, children[k], child_offset, child_blocks, flips.first(static_cast<std::size_t>(split)), stored, spine);
            flips = flips.subspan(static_cast<std::size_t>(split));
        }
        auto fresh = false;
        const auto node = insert_node(children, size, fresh);
        stored = stored || fresh;
        if (fresh && spine != nullptr && offset == 0)
            spine->add(node);
        return node;
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

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept { return Members<&TreeBitsetSet::m_nodes> {}; }
};

}  // namespace urd::repr

namespace urd
{

template<>
struct include<"urd/repr/pools/tree_bitset.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<bool ThreadSafe, typename S>
struct representation_of<tree_pool_decl<true, ThreadSafe>, S>
{
    static_assert(std::same_as<typename S::handle_type, urd::index<typename S::element_type>>,
                  "urd::Store: a tree pool hands out its own handles (a root and a size), not the index urd::pool<V, H> names");
    using type = repr::TreeBitsetSet<typename S::element_type, 4, ThreadSafe>;
};

}  // namespace urd::detail
