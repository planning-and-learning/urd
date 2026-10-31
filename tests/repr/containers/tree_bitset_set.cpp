// Copied from yggdrasil (tests/unit/containers/tree_bitset_set.cpp); names moved from ygg to urd::repr, the block-level
// operations named insert_blocks, insert_flips and read_blocks (insert and read take a row's elements here), blocks
// fixed at 32 bits, and yggdrasil's default arity 2 spelled out. The concurrent tests come with the concurrent node
// tables; the boost::dynamic_bitset adapters are not copied.

#include "urd/repr/dense_set.hpp"
#include "urd/repr/tree_bitset.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace urd::repr::tests
{
namespace
{

using Block = std::uint32_t;
using Row = DenseSet<std::uint32_t>;

std::vector<Block> to_blocks(const std::map<std::uint32_t, Block>& words)
{
    auto blocks = std::vector<Block>(words.empty() ? 0 : words.rbegin()->first + 1);
    for (const auto& [position, block] : words)
        blocks[position] = block;
    return blocks;
}

template<typename Set>
std::vector<Block> read_all(const Set& set, typename Set::handle_type index)
{
    auto blocks = std::vector<Block>(index.size);
    set.read_blocks(index, blocks);
    return blocks;
}

TEST(CommonTreeBitsetSetTest, RoundTripsAndSkipsZeroBlocks)
{
    auto set = TreeBitsetSet<Row, 2> {};

    const auto empty = set.insert_blocks(std::span<const Block> {});
    EXPECT_EQ(empty, TreeHandle {});
    EXPECT_TRUE(set.empty());
    set.read_blocks(empty, std::span<Block> {});

    constexpr auto single = std::array<Block, 1> { 0b1011 };
    const auto single_index = set.insert_blocks(single);
    EXPECT_EQ(single_index.size, 1);
    EXPECT_EQ(read_all(set, single_index), std::vector<Block>(single.begin(), single.end()));
    EXPECT_EQ(single_index.root, single.front());  // Blocks are stored inline.
    EXPECT_EQ(set.num_nodes(), 0);

    // Leading zero blocks and zero subtrees are not stored.
    constexpr auto sparse = std::array<Block, 8> { 0, 0, 0, 0, 0, 0, 0, 7 };
    const auto sparse_index = set.insert_blocks(sparse);
    EXPECT_EQ(read_all(set, sparse_index), std::vector<Block>(sparse.begin(), sparse.end()));
    EXPECT_EQ(set.num_nodes(), 3);

    auto far = std::vector<Block>(std::size_t { 1 } << 20);
    far.front() = 1;
    far.back() = Block { 1 } << 31;
    const auto far_index = set.insert_blocks(far);
    EXPECT_EQ(read_all(set, far_index), far);

    EXPECT_EQ(set.insert_blocks(single), single_index);
    EXPECT_EQ(set.insert_blocks(sparse), sparse_index);
    EXPECT_THROW(set.insert_blocks(std::array<Block, 2> { 1, 0 }), std::invalid_argument);
    EXPECT_THROW(set.read_blocks(sparse_index, std::span<Block>(far).first(3)), std::invalid_argument);

    set.clear();
    EXPECT_TRUE(set.empty());
}

TEST(CommonTreeBitsetSetTest, DistinguishesAllOnesBlocksFromZeroSubtrees)
{
    using Flip = BlockFlip;
    auto set = TreeBitsetSet<Row, 2> {};

    // An all-ones block has the bit pattern of the zero-subtree sentinel, but leaves are read by position.
    constexpr auto ones = ~Block { 0 };
    for (const auto& blocks :
         { std::vector<Block> { ones }, std::vector<Block> { ones, ones }, std::vector<Block> { 0, ones }, std::vector<Block> { ones, 0, 0, ones } })
    {
        const auto index = set.insert_blocks(blocks);
        EXPECT_EQ(read_all(set, index), blocks);
    }

    const auto base = set.insert_blocks(std::vector<Block> { 1, 2, 3 });
    const auto flipped = set.insert_flips(base, std::array<Flip, 2> { Flip { 0, ones ^ 1 }, Flip { 2, 3 } });
    EXPECT_EQ(flipped, set.insert_blocks(std::vector<Block> { ones, 2 }));
    EXPECT_EQ(set.insert_flips(flipped, std::array<Flip, 1> { Flip { 0, ones } }), set.insert_blocks(std::vector<Block> { 0, 2 }));
}

TEST(CommonTreeBitsetSetTest, DeltaInsertMatchesFullInsert)
{
    using Flip = BlockFlip;
    auto set = TreeBitsetSet<Row, 2> {};

    const auto base = set.insert_blocks(std::array<Block, 5> { 1, 2, 3, 4, 5 });

    // Same size: only the path to block 2 is new.
    const auto nodes_before = set.num_nodes();
    const auto changed = set.insert_flips(base, std::array<Flip, 1> { Flip { 2, 0b11 } });
    EXPECT_EQ(changed, set.insert_blocks(std::array<Block, 5> { 1, 2, 0, 4, 5 }));
    EXPECT_LE(set.num_nodes() - nodes_before, 3);

    // Growing and shrinking.
    const auto grown = set.insert_flips(base, std::array<Flip, 1> { Flip { 9, 1 } });
    EXPECT_EQ(grown, set.insert_blocks(std::array<Block, 10> { 1, 2, 3, 4, 5, 0, 0, 0, 0, 1 }));
    const auto shrunk = set.insert_flips(grown, std::array<Flip, 2> { Flip { 4, 5 }, Flip { 9, 1 } });
    EXPECT_EQ(shrunk, set.insert_blocks(std::array<Block, 4> { 1, 2, 3, 4 }));
    EXPECT_EQ(set.insert_flips(shrunk, std::array<Flip, 1> { Flip { 4, 5 } }), base);

    // No flips leaves the index unchanged; clearing everything gives the empty index.
    EXPECT_EQ(set.insert_flips(base, std::span<const Flip> {}), base);
    EXPECT_EQ(set.insert_flips(base, std::array<Flip, 5> { Flip { 0, 1 }, Flip { 1, 2 }, Flip { 2, 3 }, Flip { 3, 4 }, Flip { 4, 5 } }), TreeHandle {});
    EXPECT_EQ(set.insert_flips(TreeHandle {}, std::array<Flip, 1> { Flip { 3, 8 } }), set.insert_blocks(std::array<Block, 4> { 0, 0, 0, 8 }));
}

TEST(CommonTreeBitsetSetTest, DeltaInsertAcrossPowerOfTwoSizes)
{
    using Flip = BlockFlip;
    auto set = TreeBitsetSet<Row, 2> {};

    // Sizes just below, at, and above powers of two, where the shapes of base and result differ most.
    for (const std::uint32_t from : { 1u, 2u, 7u, 64u, 65u, 511u, 512u, 513u, 1000u })
    {
        for (const std::uint32_t to : { 1u, 3u, 63u, 64u, 65u, 257u, 512u, 513u, 1500u })
        {
            auto base_blocks = std::vector<Block>(from);
            for (std::uint32_t i = 0; i < from; ++i)
                base_blocks[i] = static_cast<Block>(i % 3 == 0 ? i + 1 : 0);
            base_blocks.back() = 1;
            const auto base = set.insert_blocks(base_blocks);

            // Set the new last block, clear the old last block if it lies beyond, and change one block in between.
            auto blocks = base_blocks;
            blocks.resize(std::max(from, to), Block { 0 });
            auto flips = std::vector<Flip> {};
            const auto middle = std::min(from, to) / 2;
            flips.push_back(Flip { middle, Block { 0b100 } });
            if (to - 1 != middle && blocks[to - 1] == 0)
                flips.push_back(Flip { to - 1, Block { 1 } });
            for (std::uint32_t i = to; i < from; ++i)
                if (blocks[i] != 0)
                    flips.push_back(Flip { i, blocks[i] });
            std::ranges::sort(flips, {}, &Flip::position);
            for (const auto& flip : flips)
                blocks[flip.position] ^= flip.mask;
            while (!blocks.empty() && blocks.back() == 0)
                blocks.pop_back();

            EXPECT_EQ(set.insert_flips(base, flips), set.insert_blocks(blocks)) << "from " << from << " to " << to;
        }
    }
}

TEST(CommonTreeBitsetSetTest, RandomDeltaChainsMatchFullInsert)
{
    using Flip = BlockFlip;

    auto rng = std::mt19937_64 { 42 };
    auto set = TreeBitsetSet<Row, 2> {};
    auto reference = TreeBitsetSet<Row, 2> {};

    for (const std::uint32_t universe : { 1u, 3u, 64u, 200u, 5000u })
    {
        for (std::size_t chain = 0; chain < 20; ++chain)
        {
            auto words = std::map<std::uint32_t, Block> {};
            auto index = TreeHandle {};
            auto position = std::uniform_int_distribution<std::uint32_t>(0, universe - 1);
            auto num_flips = std::uniform_int_distribution<std::size_t>(0, 6);

            for (std::size_t step = 0; step < 50; ++step)
            {
                auto flips = std::map<std::uint32_t, Block> {};
                for (std::size_t k = num_flips(rng); k > 0; --k)
                    flips[position(rng)] ^= Block { 1 } << (rng() % 32);

                auto flip_list = std::vector<Flip> {};
                for (const auto& [p, mask] : flips)
                {
                    if (mask == 0)
                        continue;
                    flip_list.push_back(Flip { p, mask });
                    words[p] ^= mask;
                    if (words[p] == 0)
                        words.erase(p);
                }

                const auto blocks = to_blocks(words);
                const auto nodes_before = set.num_nodes();
                const auto height = blocks.empty() ? std::size_t { 0 } : static_cast<std::size_t>(std::bit_width(blocks.size()));
                index = set.insert_flips(index, flip_list);

                // Changing a block and growing or shrinking each rebuild at most one path.
                EXPECT_LE(set.num_nodes() - nodes_before, (flip_list.size() + 2) * (height + 1));
                EXPECT_EQ(read_all(set, index), blocks);
                EXPECT_EQ(index, set.insert_blocks(blocks));
                EXPECT_EQ(read_all(reference, reference.insert_blocks(blocks)), blocks);
            }
        }
    }
}

/// Sizes 1, Arity +- 1, Arity^2 +- 1 and Arity^3 +- 1 have unused child slots on the right edge.
template<std::size_t Arity>
std::vector<std::uint32_t> arity_sizes()
{
    auto sizes = std::vector<std::uint32_t> { 1, 2 };
    for (std::uint32_t power = Arity; power <= Arity * Arity * Arity; power *= Arity)
        for (const auto size : { power - 1, power, power + 1 })
            sizes.push_back(size);
    return sizes;
}

template<typename TestBlock, std::size_t Arity>
void expect_arity_round_trips_and_canonical_trees()
{
    SCOPED_TRACE(testing::Message() << "Arity " << Arity << ", " << sizeof(TestBlock) << "-byte blocks");
    auto rng = std::mt19937_64 { Arity };
    auto set = TreeBitsetSet<Row, Arity> {};
    for (const auto size : arity_sizes<Arity>())
    {
        // Sparse blocks, so that zero subtrees occur at every level.
        auto blocks = std::vector<TestBlock>(size);
        for (auto& block : blocks)
            block = (rng() % 3 == 0) ? TestBlock(rng() | 1) : TestBlock { 0 };
        blocks.back() = TestBlock { 1 };

        const auto index = set.insert_blocks(blocks);
        auto output = std::vector<TestBlock>(size);
        set.read_blocks(index, output);
        EXPECT_EQ(output, blocks);
        EXPECT_EQ(set.insert_blocks(blocks), index);

        // Building the same bitset from the empty bitset by flips gives the same tree.
        auto flips = std::vector<BlockFlip> {};
        for (std::uint32_t i = 0; i < size; ++i)
            if (blocks[i] != TestBlock { 0 })
                flips.push_back(BlockFlip { i, blocks[i] });
        EXPECT_EQ(set.insert_flips(TreeHandle {}, flips), index);
    }
}

TEST(CommonTreeBitsetSetTest, RoundTripsAndCanonicalTreesForArities)
{
    expect_arity_round_trips_and_canonical_trees<Block, 2>();
    expect_arity_round_trips_and_canonical_trees<Block, 4>();
    expect_arity_round_trips_and_canonical_trees<Block, 8>();
}

template<typename TestBlock, std::size_t Arity>
void expect_random_delta_chains_match_full_insert()
{
    SCOPED_TRACE(testing::Message() << "Arity " << Arity << ", " << sizeof(TestBlock) << "-byte blocks");
    using Flip = BlockFlip;

    auto rng = std::mt19937_64 { 7 * Arity };
    auto set = TreeBitsetSet<Row, Arity> {};

    for (const std::uint32_t universe : { 1u, 3u, static_cast<std::uint32_t>(Arity + 1), 200u, 5000u })
    {
        for (std::size_t chain = 0; chain < 10; ++chain)
        {
            auto words = std::map<std::uint32_t, TestBlock> {};
            auto index = TreeHandle {};
            auto position = std::uniform_int_distribution<std::uint32_t>(0, universe - 1);
            auto num_flips = std::uniform_int_distribution<std::size_t>(0, 6);

            for (std::size_t step = 0; step < 40; ++step)
            {
                auto flips = std::map<std::uint32_t, TestBlock> {};
                for (std::size_t k = num_flips(rng); k > 0; --k)
                    flips[position(rng)] ^= TestBlock { 1 } << (rng() % std::numeric_limits<TestBlock>::digits);

                auto flip_list = std::vector<Flip> {};
                for (const auto& [p, mask] : flips)
                {
                    if (mask == 0)
                        continue;
                    flip_list.push_back(Flip { p, mask });
                    words[p] ^= mask;
                    if (words[p] == 0)
                        words.erase(p);
                }

                auto blocks = std::vector<TestBlock>(words.empty() ? 0 : words.rbegin()->first + 1);
                for (const auto& [p, block] : words)
                    blocks[p] = block;

                const auto nodes_before = set.num_nodes();
                const auto bits = static_cast<std::size_t>(std::countr_zero(Arity));
                const auto levels = blocks.empty() ? std::size_t { 0 } : (static_cast<std::size_t>(std::bit_width(blocks.size())) + bits - 1) / bits + 1;
                index = set.insert_flips(index, flip_list);

                // Changing a block and growing or shrinking each rebuild at most one path.
                EXPECT_LE(set.num_nodes() - nodes_before, (flip_list.size() + 2) * (levels + 1));
                auto output = std::vector<TestBlock>(index.size);
                set.read_blocks(index, output);
                EXPECT_EQ(output, blocks);
                EXPECT_EQ(index, set.insert_blocks(blocks));
            }
        }
    }
}

TEST(CommonTreeBitsetSetTest, RandomDeltaChainsMatchFullInsertForArities)
{
    expect_random_delta_chains_match_full_insert<Block, 2>();
    expect_random_delta_chains_match_full_insert<Block, 4>();
    expect_random_delta_chains_match_full_insert<Block, 8>();
}

}  // namespace
}  // namespace urd::repr::tests
