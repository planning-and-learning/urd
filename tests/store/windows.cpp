// Dense sets over a window of keys (urd::key_range): their words begin at the window's first key. Against std::set
// under random inserts, erases and set algebra between sets over different windows, and the same algebra with a
// bitset given as words.

#include "urd/repr/values/dense_set.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <iterator>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace
{

using Set = urd::Store<urd::set<std::uint32_t>, urd::dense>;
using Model = std::set<std::uint32_t>;

std::vector<std::uint32_t> keys_of(const Set& set) { return { set.begin(), set.end() }; }
std::vector<std::uint32_t> keys_of(const Model& set) { return { set.begin(), set.end() }; }

/// A set over a random window, its model, and the window's first key, filled with random keys of the window.
std::tuple<Set, Model, std::uint32_t> drawn(std::mt19937& random)
{
    const auto first = static_cast<std::uint32_t>(random() % 200);
    const auto length = static_cast<std::uint32_t>(random() % 300);
    auto set = Set(urd::key_range<std::uint32_t> { first, first + length });
    auto model = Model();
    for (std::uint32_t each = 0; length > 0 && each < length / 2; ++each)
    {
        const auto key = first + static_cast<std::uint32_t>(random() % length);
        set.insert(key);
        model.insert(key);
    }
    return { std::move(set), std::move(model), first };
}

/// A set's keys as words from `first` on (bit j for key first + j): what a graph's row is.
std::vector<urd::word_t> words_of(const Model& model, std::uint32_t first, std::size_t bits)
{
    auto words = std::vector<urd::word_t>((bits + urd::word_bits - 1) / urd::word_bits, 0);
    for (const auto key : model)
        if (key >= first && key - first < bits)
            words[(key - first) / urd::word_bits] |= urd::word_t { 1 } << ((key - first) % urd::word_bits);
    return words;
}

TEST(Window, AgreesWithModel)
{
    auto random = std::mt19937(23);
    for (int round = 0; round < 400; ++round)
    {
        auto [lhs, lhs_model, lhs_first] = drawn(random);
        auto [rhs, rhs_model, rhs_first] = drawn(random);
        ASSERT_EQ(keys_of(lhs), keys_of(lhs_model));
        ASSERT_EQ(lhs.size(), lhs_model.size());
        for (std::uint32_t key = 0; key < 600; ++key)
            ASSERT_EQ(lhs.contains(key), lhs_model.contains(key));

        auto both = Model();
        std::ranges::set_intersection(lhs_model, rhs_model, std::inserter(both, both.end()));
        EXPECT_EQ(lhs.intersects(rhs), !both.empty());
        EXPECT_EQ(lhs.includes(rhs), both.size() == rhs_model.size());
        EXPECT_EQ(lhs == rhs, lhs_model == rhs_model);

        auto intersected = lhs;
        intersected &= rhs;
        EXPECT_EQ(keys_of(intersected), keys_of(both));
        auto minus = lhs;
        minus -= rhs;
        auto minus_model = Model();
        std::ranges::set_difference(lhs_model, rhs_model, std::inserter(minus_model, minus_model.end()));
        EXPECT_EQ(keys_of(minus), keys_of(minus_model));
        EXPECT_EQ(minus.size(), minus_model.size());

        // A union takes no key below the window's first.
        auto united = lhs;
        if (!rhs_model.empty() && *rhs_model.begin() < lhs_first)
            EXPECT_THROW(united |= rhs, std::out_of_range);
        else
        {
            united |= rhs;
            auto all = lhs_model;
            all.insert(rhs_model.begin(), rhs_model.end());
            EXPECT_EQ(keys_of(united), keys_of(all));
            EXPECT_EQ(united.size(), all.size());
        }
    }
}

/// The same algebra with a bitset given as words at an offset, as a graph's rows of bits are.
TEST(Window, WithWords)
{
    auto random = std::mt19937(29);
    for (int round = 0; round < 400; ++round)
    {
        auto [set, model, set_first] = drawn(random);
        auto [given_set, given, first] = drawn(random);
        const auto bits = static_cast<std::size_t>((given.empty() ? 0 : *given.rbegin() - first + 1) + (random() % 70));
        const auto words = words_of(given, first, bits);

        auto both = Model();
        std::ranges::set_intersection(model, given, std::inserter(both, both.end()));
        EXPECT_EQ(urd::detail::dense_words::count_common(urd::detail::store_access::representation(set), words, first, bits), both.size());
        auto visited = std::vector<std::uint32_t>();
        urd::detail::dense_words::for_each_common(urd::detail::store_access::representation(set),
                                                  words,
                                                  first,
                                                  bits,
                                                  [&](std::uint32_t key) { visited.push_back(key); });
        EXPECT_EQ(visited, keys_of(both));

        auto intersected = set;
        urd::detail::dense_words::intersect(urd::detail::store_access::representation(intersected), words, first, bits);
        EXPECT_EQ(keys_of(intersected), keys_of(both));
        EXPECT_EQ(intersected.size(), both.size());

        auto minus = set;
        urd::detail::dense_words::subtract(urd::detail::store_access::representation(minus), words, first, bits);
        auto minus_model = Model();
        std::ranges::set_difference(model, given, std::inserter(minus_model, minus_model.end()));
        EXPECT_EQ(keys_of(minus), keys_of(minus_model));
        EXPECT_EQ(minus.size(), minus_model.size());
    }
}

/// A key below the window does not fit; one past its room grows the set; its words do not begin at key 0.
TEST(Window, Edges)
{
    auto set = Set(urd::key_range<std::uint32_t> { 100, 110 });
    EXPECT_THROW(set.insert(99), std::out_of_range);
    EXPECT_FALSE(set.contains(5));
    set.insert(500);
    set.insert(100);
    EXPECT_EQ(keys_of(set), (std::vector<std::uint32_t> { 100, 500 }));
    EXPECT_EQ(*urd::detail::store_access::representation(set).lower_bound(0), 100U);
    EXPECT_EQ(*urd::detail::store_access::representation(set).lower_bound(101), 500U);
    EXPECT_THROW((void) set.significant_words(), std::logic_error);
    set.erase(100);
    EXPECT_EQ(set.size(), 1U);
}

}  // namespace
