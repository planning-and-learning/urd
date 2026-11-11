// Sequences addressed by an index type: sequence<V, P>. Each header a Store here needs is included.

#include "urd/elements/index.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/store.hpp"

#include <concepts>
#include <cstddef>
#include <gtest/gtest.h>
#include <stdexcept>

namespace
{

struct Vertex
{
};
using Id = urd::index<Vertex>;
using Levels = urd::Store<urd::sequence<int, Id>>;

static_assert(std::same_as<Levels::position_type, Id>);
template<typename S>
concept takes_a_number = requires(const S& store) { store[std::size_t { 0 }]; };
static_assert(!takes_a_number<Levels>, "an index-addressed sequence takes its index, not a number");
static_assert(takes_a_number<urd::Store<urd::sequence<int>>>);
static_assert(std::same_as<urd::Store<urd::sequence<int>>::position_type, std::size_t>);

TEST(IndexedSequence, IsAddressedByItsIndex)
{
    auto levels = Levels {};
    levels.push_back(4);
    levels.push_back(7);
    levels.resize(3, 9);
    EXPECT_EQ(levels.size(), 3U);
    EXPECT_EQ(levels[Id(1)], 7);
    levels[Id(0)] = 5;
    EXPECT_EQ(levels.at(Id(0)), 5);
    EXPECT_EQ(levels[Id(2)], 9);
    EXPECT_EQ(levels.position(levels.begin() + 1), Id(1));
    EXPECT_EQ(levels.position(levels.end()), Id(3));
    EXPECT_THROW((void) levels.at(Id(3)), std::out_of_range);
    EXPECT_THROW((void) levels.at(Id()), std::out_of_range);
    levels.clear();
    EXPECT_TRUE(levels.empty());
}

}  // namespace
