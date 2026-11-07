// The tests of the packed integer access in repr/containers/bit.hpp (moved from yggdrasil's tests/unit/core/core.cpp).

#include "urd/repr/containers/bit.hpp"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace urd::repr::tests
{

TEST(Bit, CeilDivRoundsUpWithoutOverflow)
{
    EXPECT_EQ(urd::repr::bit::ceil_div(0u, 2u), 0u);
    EXPECT_EQ(urd::repr::bit::ceil_div(5u, 2u), 3u);
    EXPECT_EQ(urd::repr::bit::ceil_div(std::numeric_limits<unsigned>::max(), 2u), std::numeric_limits<unsigned>::max() / 2u + 1u);
}

TEST(Bit, IntReferenceReadsAndWritesItsBits)
{
    auto blocks = std::array<std::uint8_t, 2> {};
    urd::repr::bit::write_int<std::uint8_t>(blocks, std::uint8_t { 0b111111 }, 6, 6);
    EXPECT_EQ(urd::repr::bit::read_int<std::uint8_t>(blocks, 6, 6), std::uint8_t { 0b111111 });

    auto reference = urd::repr::bit::int_reference<std::uint8_t>(blocks, 0, 3);
    reference = std::uint8_t { 5 };
    EXPECT_EQ(static_cast<std::uint8_t>(reference), std::uint8_t { 5 });
    EXPECT_THROW((reference = std::uint8_t { 8 }), std::out_of_range);
}

TEST(Bit, AtomicIntReferencePreservesAdjacentBits)
{
    auto blocks = std::array<std::uint8_t, 2> { 0b00111111, 0b11110000 };
    auto reference = urd::repr::bit::atomic_int_reference<std::uint8_t>(blocks, 6, 6);

    reference = std::uint8_t { 0b101011 };

    EXPECT_EQ(static_cast<std::uint8_t>(reference), std::uint8_t { 0b101011 });
    EXPECT_EQ(blocks, (std::array<std::uint8_t, 2> { 0b11111111, 0b11111010 }));
    EXPECT_THROW((reference = std::uint8_t { 0b1000000 }), std::out_of_range);
}

TEST(Bit, AtomicPackedAccessMatchesNonAtomicAccess)
{
    for (std::uint8_t offset = 0; offset < std::numeric_limits<std::uint8_t>::digits; ++offset)
    {
        for (std::uint8_t len = 0; len <= std::numeric_limits<std::uint8_t>::digits; ++len)
        {
            auto expected = std::array<std::uint8_t, 2> { 0b10100101, 0b01011010 };
            auto actual = expected;
            constexpr auto value = std::uint8_t { 0b11010011 };

            urd::repr::bit::write_int<std::uint8_t>(expected, value, offset, len);
            urd::repr::bit::atomic_write_int<std::uint8_t>(actual, value, offset, len);

            EXPECT_EQ(actual, expected);
            EXPECT_EQ(urd::repr::bit::atomic_read_int<std::uint8_t>(actual, offset, len), urd::repr::bit::read_int<std::uint8_t>(expected, offset, len));
        }
    }
}

#ifndef NDEBUG
TEST(Bit, AtomicPackedAccessRejectsInvalidRanges)
{
    auto blocks = std::array<std::uint8_t, 2> {};
    constexpr auto digits = std::numeric_limits<std::uint8_t>::digits;

    EXPECT_DEATH((void) urd::repr::bit::atomic_read_int<std::uint8_t>(blocks, digits, 1), "Offset");
    EXPECT_DEATH(urd::repr::bit::atomic_write_int<std::uint8_t>(blocks, std::uint8_t { 0 }, 0, digits + 1), "Width");
    EXPECT_DEATH((void) urd::repr::bit::atomic_int_reference<std::uint8_t>(blocks, digits, 1), "Offset");
}
#endif

}  // namespace urd::repr::tests
