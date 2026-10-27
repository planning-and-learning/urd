// The algorithms agree with std::set, pick the lookup their input allows, and type their results.

#include "check.hpp"
#include "urd/algorithms.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <random>
#include <set>
#include <unordered_set>
#include <vector>

namespace
{

using urd::props;

// What the standard containers guarantee by type, and what they do not.
static_assert(urd::properties_of_v<std::set<int>> == (props::sorted | props::unique | props::stable_refs));
static_assert(urd::properties_of_v<std::multiset<int>> == (props::sorted | props::stable_refs));
static_assert(urd::properties_of_v<std::unordered_set<int>> == (props::unique | props::stable_refs));
static_assert(urd::properties_of_v<std::vector<int>> == props::contiguous);
static_assert(urd::properties_of_v<std::set<int, std::greater<>>> == props::none);  // sorted, but descending

static_assert(urd::lookup_of<std::vector<int>>() == urd::lookup::linear_scan);
static_assert(urd::lookup_of<urd::SortedVector<int>>() == urd::lookup::binary_search);
static_assert(urd::lookup_of<urd::UniqueVector<int>>() == urd::lookup::linear_scan);
static_assert(urd::lookup_of<std::set<int>>() == urd::lookup::member);

static_assert(urd::mergeable_sets<std::set<int>, urd::SortedUniqueVector<int>>);
static_assert(!urd::mergeable_sets<std::vector<int>, urd::SortedUniqueVector<int>>);
static_assert(!urd::mergeable_sets<urd::SortedVector<int>, urd::SortedUniqueVector<int>>);

constexpr int RANGE = 30;

std::set<int> random_set(std::mt19937& random)
{
    auto pick = std::uniform_int_distribution<int>(0, RANGE);
    auto out = std::set<int>();
    const int count = pick(random);
    for (int i = 0; i < count; ++i)
        out.insert(pick(random));
    return out;
}

}  // namespace

int main()
{
    auto random = std::mt19937(7);
    for (int round = 0; round < 500; ++round)
    {
        const auto a = random_set(random);
        const auto b = random_set(random);
        const auto vector_b = urd::SortedUniqueVector<int>::assume(std::vector<int>(b.begin(), b.end()));

        auto want = std::vector<int>();
        std::ranges::set_intersection(a, b, std::back_inserter(want));
        const auto got = urd::intersect(a, vector_b);
        URD_CHECK(std::vector<int>(got.begin(), got.end()) == want);

        want.clear();
        std::ranges::set_union(a, b, std::back_inserter(want));
        const auto united = urd::unite(a, vector_b);
        URD_CHECK(std::vector<int>(united.begin(), united.end()) == want);

        for (int value = -1; value <= RANGE + 1; ++value)
        {
            const bool present = b.contains(value);
            URD_CHECK(urd::contains(vector_b, value) == present);
            URD_CHECK(urd::contains(std::vector<int>(b.begin(), b.end()), value) == present);
            URD_CHECK(urd::contains(b, value) == present);
        }
    }
}
