// The tests of BitsetSpan, SetBitIndices, for_each_bit and their urd::Hash (moved from yggdrasil's
// tests/unit/containers/dynamic_bitset.cpp), and of urd::Hash for boost::dynamic_bitset where Boost has it.

#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/repr/containers/bitset_span.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <vector>

#if __has_include(<boost/dynamic_bitset.hpp>)
#include "urd/elements/hash_dynamic_bitset.hpp"
#endif

namespace urd::repr::tests
{

TEST(BitsetSpan, ReadsAndWritesBits)
{
    auto lhs_blocks = std::vector<std::uint64_t>(urd::repr::BitsetSpan<std::uint64_t>::num_blocks(70), 0);
    auto rhs_blocks = std::vector<std::uint64_t>(urd::repr::BitsetSpan<std::uint64_t>::num_blocks(70), 0);

    auto lhs = urd::repr::BitsetSpan<std::uint64_t>(lhs_blocks.data(), 70);
    auto rhs = urd::repr::BitsetSpan<std::uint64_t>(rhs_blocks.data(), 70);

    EXPECT_EQ(lhs.size(), 70);
    EXPECT_FALSE(lhs.empty());
    EXPECT_TRUE(lhs.none());
    EXPECT_FALSE(lhs.any());
    EXPECT_FALSE(lhs.all());

    lhs[1] = true;
    lhs.set(69);
    lhs.set(68, false);
    rhs.set(1);
    rhs.set(2);
    rhs.set(69);

    EXPECT_TRUE(lhs[1]);
    EXPECT_FALSE(lhs[2]);
    EXPECT_TRUE(lhs.any());
    EXPECT_FALSE(lhs.none());
    EXPECT_FALSE(lhs.all());

    auto lhs_raw_blocks = lhs.blocks();
    lhs_raw_blocks.back() |= ~urd::repr::BitsetSpan<std::uint64_t>::last_mask(lhs.size());
    EXPECT_FALSE(lhs.trailing_bits_zero());
    lhs.clear_trailing_bits();
    EXPECT_TRUE(lhs.trailing_bits_zero());

    EXPECT_TRUE(lhs.intersects(rhs));
    EXPECT_TRUE(lhs.is_subset_of(rhs));
    EXPECT_TRUE(lhs.is_proper_subset_of(rhs));
    EXPECT_TRUE(rhs.is_superset_of(lhs));
    EXPECT_TRUE(rhs.is_proper_superset_of(lhs));
    EXPECT_FALSE(rhs.is_subset_of(lhs));

    lhs[2].flip();
    EXPECT_TRUE(lhs == rhs);
    EXPECT_TRUE(lhs.is_subset_of(rhs));
    EXPECT_FALSE(lhs.is_proper_subset_of(rhs));

    lhs.flip();
    EXPECT_FALSE(lhs.intersects(rhs));
    EXPECT_TRUE(lhs.trailing_bits_zero());

    lhs ^= rhs;
    EXPECT_TRUE(lhs.all());
}

TEST(BitsetSpan, AtChecksBounds)
{
    auto blocks = std::vector<std::uint64_t>(urd::repr::BitsetSpan<std::uint64_t>::num_blocks(8), 0);
    auto bitset = urd::repr::BitsetSpan<std::uint64_t>(blocks.data(), 8);
    using Bitset = decltype(bitset);
    using ConstBitset = urd::repr::BitsetSpan<const std::uint64_t>;
    static_assert(std::convertible_to<Bitset, ConstBitset>);

    bitset.at(3) = true;
    EXPECT_TRUE(bitset.at(3));

    bitset.at(3).flip();
    EXPECT_FALSE(bitset.at(3));

    const auto const_bitset = ConstBitset(bitset);
    EXPECT_FALSE(const_bitset.at(3));
    EXPECT_EQ(bitset.data(), blocks.data());
    EXPECT_EQ(const_bitset.data(), blocks.data());

    EXPECT_THROW(bitset.at(8), std::out_of_range);
    EXPECT_THROW((void) const_bitset.at(8), std::out_of_range);
}

TEST(BitsetSpan, AtRejectsInvalidNonEmptySpan)
{
    auto bitset = urd::repr::BitsetSpan<std::uint64_t>(nullptr, 1);
    const auto const_bitset = urd::repr::BitsetSpan<const std::uint64_t>(nullptr, 1);

    EXPECT_THROW(bitset.at(0), std::logic_error);
    EXPECT_THROW((void) const_bitset.at(0), std::logic_error);

    auto empty_bitset = urd::repr::BitsetSpan<std::uint64_t>(nullptr, 0);
    const auto empty_const_bitset = urd::repr::BitsetSpan<const std::uint64_t>(nullptr, 0);

    EXPECT_THROW(empty_bitset.at(0), std::out_of_range);
    EXPECT_THROW((void) empty_const_bitset.at(0), std::out_of_range);
}

TEST(BitsetSpan, FindNextHandlesSentinelAndEndPositions)
{
    auto blocks = std::vector<std::uint64_t>(urd::repr::BitsetSpan<std::uint64_t>::num_blocks(70), 0);
    auto bitset = urd::repr::BitsetSpan<std::uint64_t>(blocks.data(), 70);

    EXPECT_EQ(bitset.find_first(), urd::repr::BitsetSpan<std::uint64_t>::npos);
    EXPECT_EQ(bitset.find_next(urd::repr::BitsetSpan<std::uint64_t>::npos), urd::repr::BitsetSpan<std::uint64_t>::npos);
    EXPECT_EQ(bitset.find_next_zero(urd::repr::BitsetSpan<std::uint64_t>::npos), urd::repr::BitsetSpan<std::uint64_t>::npos);

    bitset.set(69);
    EXPECT_EQ(bitset.find_first(), 69);
    EXPECT_EQ(bitset.find_next(68), 69);
    EXPECT_EQ(bitset.find_next(69), urd::repr::BitsetSpan<std::uint64_t>::npos);
    EXPECT_EQ(bitset.find_next_zero(68), urd::repr::BitsetSpan<std::uint64_t>::npos);

    bitset.reset();
    bitset.set();
    EXPECT_EQ(bitset.find_first_zero(), urd::repr::BitsetSpan<std::uint64_t>::npos);
    EXPECT_EQ(bitset.find_next_zero(69), urd::repr::BitsetSpan<std::uint64_t>::npos);
}

TEST(BitsetSpan, RejectsMismatchedSpanSizes)
{
    auto lhs_blocks = std::vector<std::uint64_t>(urd::repr::BitsetSpan<std::uint64_t>::num_blocks(8), 0);
    auto rhs_blocks = std::vector<std::uint64_t>(urd::repr::BitsetSpan<std::uint64_t>::num_blocks(9), 0);

    auto lhs = urd::repr::BitsetSpan<std::uint64_t>(lhs_blocks.data(), 8);
    auto rhs = urd::repr::BitsetSpan<std::uint64_t>(rhs_blocks.data(), 9);

    EXPECT_FALSE(lhs == rhs);
    EXPECT_THROW((void) lhs.intersects(rhs), std::invalid_argument);
    EXPECT_THROW((void) lhs.count_intersection(rhs), std::invalid_argument);
    EXPECT_THROW((void) lhs.is_subset_of(rhs), std::invalid_argument);
    EXPECT_THROW((void) lhs.is_proper_subset_of(rhs), std::invalid_argument);
    EXPECT_THROW((void) lhs.is_superset_of(rhs), std::invalid_argument);
    EXPECT_THROW((void) lhs.is_proper_superset_of(rhs), std::invalid_argument);
    EXPECT_THROW(lhs.copy_from(rhs), std::invalid_argument);
    EXPECT_THROW(lhs.diff_from(rhs), std::invalid_argument);
    EXPECT_THROW(lhs &= rhs, std::invalid_argument);
    EXPECT_THROW(lhs |= rhs, std::invalid_argument);
    EXPECT_THROW(lhs ^= rhs, std::invalid_argument);
    EXPECT_THROW(lhs -= rhs, std::invalid_argument);
    EXPECT_THROW(urd::repr::for_each_bit([](size_t) {}, [](std::uint64_t left, std::uint64_t right) { return left & right; }, lhs, rhs), std::invalid_argument);
}

TEST(BitsetSpan, HashesAndComparesSpans)
{
    auto lhs_blocks = std::vector<std::uint64_t>(urd::repr::BitsetSpan<std::uint64_t>::num_blocks(8), 0);
    auto rhs_blocks = std::vector<std::uint64_t>(urd::repr::BitsetSpan<std::uint64_t>::num_blocks(8), 0);

    auto lhs = urd::repr::BitsetSpan<std::uint64_t>(lhs_blocks.data(), 8);
    auto rhs = urd::repr::BitsetSpan<std::uint64_t>(rhs_blocks.data(), 8);

    lhs.set(1);
    rhs.set(1);

    EXPECT_TRUE(urd::EqualTo<urd::repr::BitsetSpan<std::uint64_t>> {}(lhs, rhs));
    EXPECT_EQ(urd::Hash<urd::repr::BitsetSpan<std::uint64_t>> {}(lhs), urd::Hash<urd::repr::BitsetSpan<std::uint64_t>> {}(rhs));

    rhs.set(2);

    EXPECT_FALSE(urd::EqualTo<urd::repr::BitsetSpan<std::uint64_t>> {}(lhs, rhs));
    EXPECT_NE(urd::Hash<urd::repr::BitsetSpan<std::uint64_t>> {}(lhs), urd::Hash<urd::repr::BitsetSpan<std::uint64_t>> {}(rhs));
}

static_assert(std::ranges::forward_range<urd::repr::SetBitIndices<std::uint64_t>>);
static_assert(std::ranges::borrowed_range<urd::repr::SetBitIndices<std::uint64_t>>);
static_assert(std::same_as<std::ranges::range_value_t<urd::repr::SetBitIndices<std::uint64_t>>, size_t>);

TEST(BitsetSpan, CountsIntersectionsAcrossWordBoundaries)
{
    for (const auto size : { 0U, 1U, 63U, 64U, 65U, 130U })
    {
        auto lhs_blocks = std::vector<std::uint64_t>(urd::repr::BitsetSpan<std::uint64_t>::num_blocks(size));
        auto rhs_blocks = lhs_blocks;
        auto lhs = urd::repr::BitsetSpan<std::uint64_t>(lhs_blocks.data(), size);
        auto rhs = urd::repr::BitsetSpan<std::uint64_t>(rhs_blocks.data(), size);
        EXPECT_EQ(lhs.count_intersection(rhs), 0);
        lhs.set();
        rhs.set();
        EXPECT_EQ(lhs.count_intersection(urd::repr::BitsetSpan<const std::uint64_t>(rhs)), size);
        lhs.reset();
        rhs.reset();
        size_t expected = 0;
        for (size_t i = 0; i < size; ++i)
        {
            lhs.set(i, i % 2 == 0);
            rhs.set(i, i % 3 == 0);
            expected += i % 6 == 0;
        }
        EXPECT_EQ(urd::repr::BitsetSpan<const std::uint64_t>(lhs).count_intersection(rhs), expected);
        rhs.copy_from(lhs);
        rhs.flip();
        EXPECT_EQ(lhs.count_intersection(rhs), 0);
    }
}

TEST(BitsetSpan, SetIndicesBorrowStorageNotWrappers)
{
    auto blocks = std::array<std::uint64_t, 3> {};
    auto bits = urd::repr::BitsetSpan<std::uint64_t>(blocks.data(), 130);
    for (const auto position : { 0U, 63U, 64U, 65U, 129U })
        bits.set(position);
    const auto expected = std::array<size_t, 5> { 0, 63, 64, 65, 129 };
    EXPECT_TRUE(std::ranges::equal(urd::repr::set_bit_indices(bits), expected));
    EXPECT_TRUE(std::ranges::equal(urd::repr::set_bit_indices(urd::repr::BitsetSpan<const std::uint64_t>(bits)), expected));

    auto iterator = urd::repr::set_bit_indices(urd::repr::BitsetSpan<const std::uint64_t>(blocks.data(), 130)).begin();
    const auto end = urd::repr::set_bit_indices(bits).end();
    auto copy = iterator;
    EXPECT_EQ(*copy, 0);
    EXPECT_EQ(*iterator++, 0);
    EXPECT_EQ(*iterator, 63);
    EXPECT_EQ(*copy, 0);
    ++copy;
    EXPECT_EQ(copy, iterator);
    while (iterator != end)
        ++iterator;
    EXPECT_EQ(iterator, end);
    EXPECT_TRUE(urd::repr::SetBitIndices<std::uint64_t>().empty());
    EXPECT_TRUE(urd::repr::set_bit_indices(urd::repr::BitsetSpan<const std::uint64_t>(nullptr, 0)).empty());
    auto other_blocks = blocks;
    EXPECT_NE(urd::repr::set_bit_indices(bits).begin(), urd::repr::set_bit_indices(urd::repr::BitsetSpan<const std::uint64_t>(other_blocks.data(), 130)).begin());
}

TEST(BitsetSpan, SetIndicesSkipEmptyWordsAndPreserveEndIdentity)
{
    const auto check = []<typename Block>()
    {
        using Bits = urd::repr::BitsetSpan<Block>;
        using Iterator = typename urd::repr::SetBitIndices<Block>::Iterator;
        constexpr size_t size = Bits::Digits * 4 + 3;
        auto blocks = std::vector<Block>(Bits::num_blocks(size));
        auto bits = Bits(blocks.data(), size);
        const auto expected = std::array<size_t, 4> { 0, Bits::Digits - 1, Bits::Digits * 3 + 2, size - 1 };
        for (const auto position : expected)
            bits.set(position);
        EXPECT_TRUE(std::ranges::equal(urd::repr::set_bit_indices(bits), expected));
        auto iterator = urd::repr::set_bit_indices(bits).begin();
        for (const auto position : expected)
        {
            auto copy = iterator;
            EXPECT_EQ(*iterator, position);
            ++iterator;
            EXPECT_EQ(*copy, position);
            ++copy;
            EXPECT_EQ(copy, iterator);
        }
        EXPECT_EQ(iterator, std::default_sentinel);
        EXPECT_EQ(iterator, Iterator(bits, Bits::npos));
        EXPECT_EQ(Iterator(), std::default_sentinel);

        bits.set();
        EXPECT_TRUE(std::ranges::equal(urd::repr::set_bit_indices(bits), std::views::iota(size_t { 0 }, size)));
        bits.reset();
        bits.set(size - 1);
        EXPECT_TRUE(std::ranges::equal(urd::repr::set_bit_indices(bits), std::array<size_t, 1> { size - 1 }));
        bits.reset();
        EXPECT_TRUE(urd::repr::set_bit_indices(bits).empty());
    };
    check.template operator()<std::uint8_t>();
    check.template operator()<std::uint32_t>();
    check.template operator()<std::uint64_t>();
}

#if __has_include(<boost/dynamic_bitset.hpp>)
TEST(BitsetSpan, HashesADynamicBitsetByItsSizeAndBlocks)
{
    auto bits = boost::dynamic_bitset<std::uint64_t>(70);
    bits.set(1);
    bits.set(69);
    auto expected = urd::hash_t { 70 };
    boost::to_block_range(bits, boost::make_function_output_iterator([&expected](const std::uint64_t block) { urd::hash_combine(expected, block); }));
    EXPECT_EQ(urd::Hash<boost::dynamic_bitset<std::uint64_t>> {}(bits), expected);

    auto blocks = std::vector<std::uint64_t>(bits.num_blocks());
    boost::to_block_range(bits, blocks.begin());
    EXPECT_EQ(urd::Hash<boost::dynamic_bitset<std::uint64_t>> {}(bits), urd::Hash<BitsetSpan<std::uint64_t>> {}(BitsetSpan<std::uint64_t>(blocks.data(), 70)));
}
#endif

}  // namespace urd::repr::tests
