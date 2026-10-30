// urd::Hash, urd::EqualTo and urd::Less keep the laws the containers rely on, for every type they cover:
// equal values hash equally, Less is a strict weak order, and two values are equivalent under Less exactly
// when EqualTo calls them equal. One typed test per law, over random values from a small domain, so that
// equal values occur.

#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/semantics/less.hpp"
#include "urd/store.hpp"
#include "urd/use/all.hpp"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

enum class Colour : std::uint8_t
{
    red,
    green,
    blue,
};

/// A record that names its identity: Hash, EqualTo and Less follow from it, with nothing in namespace std.
struct Atom
{
    int predicate = 0;
    std::vector<int> objects;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Atom::predicate, &Atom::objects> {}; }
};

constexpr int domain = 4;  // small, so that random values repeat

template<typename T>
T random_value(std::mt19937& random)
{
    auto pick = std::uniform_int_distribution<int>(0, domain - 1);
    if constexpr (std::same_as<T, int>)
        return pick(random);
    else if constexpr (std::same_as<T, double>)
    {
        const int drawn = pick(random);
        return drawn == 0 ? std::numeric_limits<double>::quiet_NaN() : drawn == 1 ? -0.0 : static_cast<double>(drawn) / 2;
    }
    else if constexpr (std::same_as<T, Colour>)
        return static_cast<Colour>(pick(random) % 3);
    else if constexpr (std::same_as<T, std::string>)
        return std::string(static_cast<std::size_t>(pick(random)), 'a');
    else if constexpr (std::same_as<T, std::vector<int>>)
    {
        auto result = std::vector<int>(static_cast<std::size_t>(pick(random) % 3));
        for (auto& element : result)
            element = pick(random);
        return result;
    }
    else if constexpr (std::same_as<T, std::optional<int>>)
        return pick(random) == 0 ? std::optional<int>() : std::optional<int>(pick(random));
    else if constexpr (std::same_as<T, std::pair<int, std::string>>)
        return { random_value<int>(random), random_value<std::string>(random) };
    else if constexpr (std::same_as<T, std::tuple<int, double>>)
        return { random_value<int>(random), random_value<double>(random) };
    else if constexpr (std::same_as<T, std::array<int, 2>>)
        return { random_value<int>(random), random_value<int>(random) };
    else if constexpr (urd::is_store<T>)
    {
        const auto elements = random_value<std::vector<int>>(random);
        return T(elements.begin(), elements.end());
    }
    else
        return Atom { random_value<int>(random), random_value<std::vector<int>>(random) };
}

using Types = testing::Types<int,
                             double,
                             Colour,
                             std::string,
                             std::vector<int>,
                             std::optional<int>,
                             std::pair<int, std::string>,
                             std::tuple<int, double>,
                             std::array<int, 2>,
                             Atom,
                             // Stores whose contents fix their order: a sorted set, a bitset.
                             urd::Store<urd::key<int>, urd::sorted, urd::frozen>,
                             urd::Store<urd::key<int>, urd::dense, urd::sorted>>;

template<typename T>
class Semantics : public testing::Test
{
};
TYPED_TEST_SUITE(Semantics, Types);

constexpr int samples = 2000;

}  // namespace

TYPED_TEST(Semantics, EqualValuesHashEqually)
{
    auto random = std::mt19937(1);
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto lhs = random_value<TypeParam>(random);
        const auto rhs = random_value<TypeParam>(random);
        if (urd::EqualTo<TypeParam> {}(lhs, rhs))
        {
            EXPECT_EQ(urd::Hash<TypeParam> {}(lhs), urd::Hash<TypeParam> {}(rhs));
        }
    }
}

TYPED_TEST(Semantics, EqualToIsAnEquivalence)
{
    auto random = std::mt19937(2);
    const auto equal = urd::EqualTo<TypeParam> {};
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto a = random_value<TypeParam>(random);
        const auto b = random_value<TypeParam>(random);
        const auto c = random_value<TypeParam>(random);
        EXPECT_TRUE(equal(a, a));
        EXPECT_EQ(equal(a, b), equal(b, a));
        if (equal(a, b) && equal(b, c))
        {
            EXPECT_TRUE(equal(a, c));
        }
    }
}

TYPED_TEST(Semantics, LessIsAStrictWeakOrderAgreeingWithEqualTo)
{
    auto random = std::mt19937(3);
    const auto less = urd::Less<TypeParam> {};
    const auto equal = urd::EqualTo<TypeParam> {};
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto a = random_value<TypeParam>(random);
        const auto b = random_value<TypeParam>(random);
        const auto c = random_value<TypeParam>(random);
        EXPECT_FALSE(less(a, a));
        if (less(a, b))
        {
            EXPECT_FALSE(less(b, a));
        }
        if (less(a, b) && less(b, c))
        {
            EXPECT_TRUE(less(a, c));
        }
        EXPECT_EQ(!less(a, b) && !less(b, a), equal(a, b));
    }
}
