// Sets and maps: random use against the standard's containers, set algebra, positions, ends, entries, Stores as keys, and orders by a projection. Each family
// header a Store here needs is included, and no other.

#include "../allocations.hpp"
#include "urd/compose/family.hpp"
#include "urd/concepts.hpp"
#include "urd/store.hpp"
#include "urd/use/contiguous.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/nodes.hpp"
#include "urd/use/queues.hpp"
#include "urd/use/sequences.hpp"

#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

using urd::key;
using urd::Store;
using urd::value;
using SortedKey = Store<key<int>, urd::sorted, urd::frozen>;
using BitKey = Store<key<int>, urd::dense, urd::frozen, urd::sorted>;

constexpr int RANGE = 25;
constexpr int STEPS = 2000;

/// Random inserts, erases and lookups on a set Store, against std::set.
template<typename S>
void behaves_like_std_set(unsigned seed)
{
    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, RANGE);
    auto store = S();
    auto reference = std::set<int>();
    for (int step = 0; step < STEPS; ++step)
    {
        const int element = pick(random);
        if (pick(random) % 3 == 0)
        {
            EXPECT_TRUE(store.erase(element) == reference.erase(element));
        }
        else
        {
            EXPECT_TRUE(store.insert(element).second == reference.insert(element).second);
        }
        const int probe = pick(random);
        EXPECT_TRUE(store.contains(probe) == reference.contains(probe));
        EXPECT_TRUE(store.size() == reference.size());
        auto contents = std::vector<int>(store.begin(), store.end());
        std::ranges::sort(contents);
        EXPECT_TRUE(contents == std::vector<int>(reference.begin(), reference.end()));
        if constexpr (urd::Sorted<S>)
        {
            EXPECT_TRUE(std::ranges::is_sorted(store));
        }
    }
}

/// Random inserts and erases on a sorted sequence Store, against std::multiset.
template<typename S>
void behaves_like_std_multiset(unsigned seed)
{
    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, RANGE);
    auto store = S();
    auto reference = std::multiset<int>();
    for (int step = 0; step < STEPS; ++step)
    {
        const int element = pick(random);
        if (pick(random) % 3 == 0)
        {
            EXPECT_TRUE(store.erase(element) == reference.erase(element));
        }
        else
        {
            EXPECT_TRUE(*store.insert(element) == *reference.insert(element));
        }
        EXPECT_TRUE(std::vector<int>(store.begin(), store.end()) == std::vector<int>(reference.begin(), reference.end()));
        const int probe = pick(random);
        EXPECT_TRUE(store.count(probe) == reference.count(probe));
        EXPECT_TRUE(std::distance(store.begin(), store.lower_bound(probe)) == std::distance(reference.begin(), reference.lower_bound(probe)));
    }
}

namespace
{

/// A key type that names its identity, and nothing else: no operators, nothing in namespace std.
struct Fact
{
    int predicate = 0;
    std::vector<int> objects;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Fact::predicate, &Fact::objects> {}; }
};

}  // namespace

TEST(Store, KeysThatNameTheirIdentity)
{
    const auto facts = std::vector<Fact> { { 3, {} }, { 1, { 2 } }, { 1, { 2 } }, { 1, { 1 } } };
    const auto hashed = Store<key<Fact>>(facts.begin(), facts.end());
    EXPECT_EQ(hashed.size(), 3U);
    EXPECT_TRUE(hashed.contains(Fact { 1, { 2 } }));

    // Ordered member by member: predicate, then objects.
    const auto tree = Store<key<Fact>, urd::sorted>(facts.begin(), facts.end());
    const auto flat = Store<key<Fact>, urd::sorted, urd::frozen>(facts.begin(), facts.end());
    EXPECT_EQ(tree.begin()->objects, std::vector<int>({ 1 }));
    EXPECT_EQ(flat.begin()->objects, std::vector<int>({ 1 }));
    EXPECT_EQ(std::prev(flat.end())->predicate, 3);

    auto counts = Store<key<Fact>, value<int>>();
    counts[Fact { 1, {} }] += 2;
    EXPECT_EQ(counts.at(Fact { 1, {} }), 2);
}

TEST(Store, BehavesLikeTheStandardContainers)
{
    for (unsigned seed = 0; seed < 5; ++seed)
    {
        behaves_like_std_set<Store<key<int>>>(seed);
        behaves_like_std_set<Store<key<int>, urd::sorted>>(seed);
        behaves_like_std_set<Store<key<int>, urd::small>>(seed);
        behaves_like_std_set<Store<key<int>, urd::sorted, urd::small>>(seed);
        behaves_like_std_multiset<Store<value<int>, urd::sorted>>(seed);
    }
}

TEST(Store, BuildsAndChangesAsAsked)
{
    // Built from elements, which the representation puts in the order asked for.
    const auto ids = Store<key<int>, urd::sorted> { 3, 1, 3, 2 };
    EXPECT_TRUE(std::vector<int>(ids.begin(), ids.end()) == std::vector<int>({ 1, 2, 3 }));
    EXPECT_TRUE(*ids.lower_bound(2) == 2 && ids.upper_bound(3) == ids.end() && *ids.rbegin() == 3);
    const auto first = Store<value<int>, urd::sorted, urd::nonempty> { 5, 4 };
    EXPECT_TRUE(first.front() == 4 && first[1] == 5 && first.back() == 5);

    const auto frozen = Store<key<int>, urd::sorted, urd::frozen> { 2, 3, 5 };
    EXPECT_TRUE(frozen.contains(5) && !frozen.contains(4));

    // A plain sequence reads and changes like std::vector.
    auto numbers = Store<value<int>> { 1, 2, 3, 2 };
    numbers[0] = 7;
    numbers.insert(numbers.begin() + 1, 9);
    EXPECT_TRUE(erase(numbers, 2) == 2);
    EXPECT_TRUE(numbers == (Store<value<int>> { 7, 9, 3 }) && numbers.at(2) == 3 && numbers < Store<value<int>> { 8 });
    EXPECT_TRUE(erase_if(numbers, [](int element) { return element > 8; }) == 1 && numbers.size() == 2);
}

namespace
{

template<typename S>
class SetAlgebra : public testing::Test
{
};
using Sets = testing::Types<Store<key<int>>,
                            Store<key<int>, urd::sorted>,
                            Store<key<int>, urd::small>,
                            Store<key<int>, urd::stable_refs>,
                            Store<key<int>, urd::dense>,
                            Store<key<int>, urd::dense, urd::constant_clear>,
                            Store<key<int>, urd::insertion_ordered>,
                            Store<key<int>, urd::dense, urd::proportional_clear>>;
TYPED_TEST_SUITE(SetAlgebra, Sets);

std::vector<int> sorted_copy(const auto& set)
{
    auto result = std::vector<int>(set.begin(), set.end());
    std::ranges::sort(result);
    return result;
}

}  // namespace

// Union, intersection, difference, inclusion and overlap give what std::set_* give on sorted copies.
TYPED_TEST(SetAlgebra, AgreesWithTheStandardAlgorithms)
{
    auto random = std::mt19937(5);
    auto pick = std::uniform_int_distribution<int>(0, 40);
    for (int round = 0; round < 200; ++round)
    {
        auto lhs = TypeParam();
        auto rhs = TypeParam();
        for (int count = pick(random) % 12; count > 0; --count)
            lhs.insert(pick(random));
        for (int count = pick(random) % 12; count > 0; --count)
            rhs.insert(pick(random));
        const auto a = sorted_copy(lhs);
        const auto b = sorted_copy(rhs);

        auto expected = std::vector<int>();
        std::ranges::set_union(a, b, std::back_inserter(expected));
        EXPECT_EQ(sorted_copy(TypeParam(lhs) |= rhs), expected);
        expected.clear();
        std::ranges::set_intersection(a, b, std::back_inserter(expected));
        EXPECT_EQ(sorted_copy(TypeParam(lhs) &= rhs), expected);
        EXPECT_EQ(lhs.intersects(rhs), !expected.empty());
        expected.clear();
        std::ranges::set_difference(a, b, std::back_inserter(expected));
        EXPECT_EQ(sorted_copy(TypeParam(lhs) -= rhs), expected);
        EXPECT_EQ(lhs.includes(rhs), std::ranges::includes(a, b));
        EXPECT_EQ((TypeParam(lhs) |= rhs).size(), sorted_copy(TypeParam(lhs) |= rhs).size());
        // After the algebra, clear leaves nothing behind (a bitset that tracks its words must track theirs too).
        for (auto result : { TypeParam(lhs) |= rhs, TypeParam(lhs) &= rhs, TypeParam(lhs) -= rhs })
        {
            result.clear();
            EXPECT_TRUE(result.empty() && sorted_copy(result).empty());
            result.insert(3);
            EXPECT_EQ(sorted_copy(result), std::vector<int> { 3 });
        }
    }
}

// An insertion-ordered set keeps its elements in one array, so it has positions without asking for positional.
TEST(Positions, InsertionOrderedSetsHaveThem)
{
    auto names = Store<key<std::string>, urd::insertion_ordered>();
    names.insert("b");
    names.insert("a");
    EXPECT_EQ(names[0], "b");
    EXPECT_EQ(names.back(), "a");
    EXPECT_EQ(names.position(names.find("a")), 1U);
}

// A sorted Store's least and greatest elements are its first and last, O(1) in a tree or a sorted array.
TEST(Ends, SortedStoresHaveTheirLeastAndGreatest)
{
    const auto tree = Store<key<int>, urd::sorted> { 5, 1, 3 };
    EXPECT_EQ(tree.front(), 1);
    EXPECT_EQ(tree.back(), 5);
    const auto flat = Store<key<int>, urd::sorted, urd::frozen> { 5, 1, 3 };
    EXPECT_EQ(flat.front(), 1);
    const auto map = Store<key<int>, value<int>, urd::sorted> { { 2, 20 }, { 1, 10 } };
    EXPECT_EQ(map.front().second, 10);
    EXPECT_EQ(map.back().first, 2);
}
template<typename S>
concept has_front = requires(const S& store) { store.front(); };
static_assert(!has_front<Store<key<int>, urd::dense, urd::sorted>> && !has_front<Store<key<int>>>);

// An insertion-ordered map gives its keys and values as spans in insertion order, and an entry's position.
TEST(Entries, KeysAndValuesInInsertionOrder)
{
    auto costs = Store<key<int>, value<double>, urd::dense, urd::insertion_ordered>();
    costs.try_emplace(7, 1.5);
    costs.try_emplace(2, 0.5);
    costs.insert_or_assign(7, 2.5);
    costs[9] = 4.0;
    EXPECT_TRUE(std::ranges::equal(costs.keys(), std::vector<int> { 7, 2, 9 }));
    EXPECT_TRUE(std::ranges::equal(costs.values(), std::vector<double> { 2.5, 0.5, 4.0 }));
    EXPECT_EQ(costs.position(costs.find(2)), 1U);
    EXPECT_EQ(costs.position(costs.find(5)), costs.size());
    costs.erase(7);
    EXPECT_TRUE(std::ranges::equal(costs.keys(), std::vector<int> { 2, 9 }));
    EXPECT_EQ(costs.at(9), 4.0);
}

// unite says whether the union added anything.
TYPED_TEST(SetAlgebra, UniteReportsWhetherItAdded)
{
    auto random = std::mt19937(43);
    for (int round = 0; round < 200; ++round)
    {
        auto lhs = TypeParam();
        auto rhs = TypeParam();
        for (int count = static_cast<int>(random() % 8); count > 0; --count)
            lhs.insert(static_cast<int>(random() % 20));
        for (int count = static_cast<int>(random() % 8); count > 0; --count)
            rhs.insert(static_cast<int>(random() % 20));
        const bool adds = !lhs.includes(rhs);
        EXPECT_EQ(lhs.unite(rhs), adds);
        EXPECT_TRUE(lhs.includes(rhs));
        EXPECT_FALSE(lhs.unite(rhs));
    }
}

// Stores as sorted keys: ordered lexicographically by their elements, equal by contents.
TEST(Less, StoresAsKeys)
{
    using Inner = Store<key<int>, urd::sorted>;
    const auto less = urd::Less<Inner> {};
    EXPECT_TRUE(less(Inner { 1, 2 }, Inner { 1, 3 }));
    EXPECT_TRUE(less(Inner { 1, 2 }, Inner { 1, 2, 3 }));
    EXPECT_FALSE(less(Inner { 2, 1 }, Inner { 1, 2 }));
    EXPECT_TRUE(urd::EqualTo<Inner> {}(Inner { 2, 1 }, Inner { 1, 2 }));

    auto by_contents = Store<key<Inner>, urd::sorted>();
    by_contents.insert(Inner { 3 });
    by_contents.insert(Inner { 1, 2 });
    by_contents.insert(Inner { 2, 1 });
    EXPECT_EQ(by_contents.size(), 2u);
    EXPECT_EQ(*by_contents.begin(), (Inner { 1, 2 }));
}

// Stores as hashed keys: found whatever order the key was built in, and whatever storage a bitset grew to.
TEST(Hash, StoresAsKeys)
{
    auto plans = Store<key<SortedKey>, value<int>>();
    plans.insert({ SortedKey { 3, 1, 2 }, 7 });
    EXPECT_EQ(plans.at(SortedKey { 1, 2, 3 }), 7);
    EXPECT_FALSE(plans.contains(SortedKey { 1, 2 }));

    auto grown = Store<key<int>, urd::dense, urd::sorted> { 1, 2 };
    grown.insert(200);
    grown.erase(200);
    const auto small = Store<key<int>, urd::dense, urd::sorted> { 1, 2 };
    EXPECT_EQ(grown, small);
    using Key = Store<key<int>, urd::dense, urd::sorted>;
    EXPECT_EQ(urd::Hash<Key> {}(grown), urd::Hash<Key> {}(small));
    auto seen = Store<key<Key>>();
    seen.insert(grown);
    EXPECT_TRUE(seen.contains(small));
}

// Ordered by a projection: a node's cost, ties by the node itself; a priority queue by (f, larger g first).
namespace
{
struct Node
{
    int id;
    int f;
    int g;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Node::id> {}; }
};
}  // namespace

TEST(OrderBy, SortsByTheProjectionAndBreaksTiesByTheElement)
{
    using ByF = Store<key<Node>, urd::sorted, urd::order_by<&Node::f>>;
    static_assert(std::same_as<ByF::representation, std::set<Node, urd::ProjectedLess<Node, &Node::f>>>);
    auto nodes = ByF { Node { 1, 5, 0 }, Node { 2, 3, 0 }, Node { 3, 5, 0 }, Node { 4, 1, 0 } };
    auto ids = std::vector<int>();
    for (const auto& node : nodes)
        ids.push_back(node.id);
    EXPECT_EQ(ids, (std::vector<int> { 4, 2, 1, 3 }));
    EXPECT_EQ(nodes.size(), 4U);
    EXPECT_TRUE(nodes.contains(Node { 3, 5, 0 }));
}

TEST(OrderBy, MapsOrderTheirKeysByTheProjection)
{
    auto costs = Store<key<Node>, value<std::string>, urd::sorted, urd::order_by<&Node::f>>();
    costs.insert_or_assign(Node { 1, 9, 0 }, "late");
    costs.insert_or_assign(Node { 2, 2, 0 }, "early");
    EXPECT_EQ(costs.begin()->second, "early");
}

TEST(OrderBy, PriorityQueuePopsByTheProjection)
{
    using Open = Store<value<Node>, urd::by_priority, urd::order_by<[](const Node& node) { return std::tuple(node.f, -node.g); }>>;
    auto open = Open();
    open.push(Node { 1, 4, 1 });
    open.push(Node { 2, 4, 3 });
    open.push(Node { 3, 2, 0 });
    auto ids = std::vector<int>();
    while (!open.empty())
    {
        ids.push_back(open.top().id);
        open.pop();
    }
    EXPECT_EQ(ids, (std::vector<int> { 3, 2, 1 }));
}

// A queue's elements need no order of their own where a projection gives one.
TEST(OrderBy, QueueOfUnorderedElements)
{
    struct Task
    {
        int cost;
        std::string name;
    };
    auto tasks = Store<value<Task>, urd::by_priority, urd::order_by<&Task::cost>>();
    tasks.push(Task { 7, "b" });
    tasks.push(Task { 2, "a" });
    EXPECT_EQ(tasks.top().name, "a");
}

// Entries identified by one thing and ordered by another: offers per interval, kept cheapest first. merge inserts or
// keeps the cheaper; every change puts the entry back in order; equal costs go by the interval.
namespace
{
struct Span
{
    int lower;
    int upper;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Span::lower, &Span::upper> {}; }
};
struct Offer
{
    double cost;
    int source;
    friend bool operator==(const Offer&, const Offer&) = default;
};
constexpr auto cheapest_first = [](const Span& span, const Offer& offer) { return std::tuple(offer.cost, span.lower, span.upper); };
using Offers = Store<key<Span>, value<Offer>, urd::sorted, urd::small, urd::order_by<cheapest_first>>;
template<typename S>
concept changes_values_by_reference = requires(S offers, Span span) { offers.at(span).cost = 0; } || requires(S offers, Span span) { offers[span]; };
}  // namespace

TEST(OrderByValue, KeepsEntriesInTheOrderTheirValuesGive)
{
    static_assert(std::same_as<Offers::representation, urd::repr::FlatEntryMap<Span, Offer, urd::EntryLess<Span, Offer, cheapest_first>>>);
    static_assert(!changes_values_by_reference<Offers> && changes_values_by_reference<Store<key<Span>, value<Offer>, urd::sorted>>);
    const auto cheaper = [](const Offer& held, const Offer& next) { return next.cost < held.cost ? next : held; };
    auto offers = Offers();
    EXPECT_TRUE(offers.merge(Span { 0, 4 }, Offer { 5.0, 1 }, cheaper));
    EXPECT_TRUE(offers.merge(Span { 2, 3 }, Offer { 2.0, 2 }, cheaper));
    EXPECT_TRUE(offers.merge(Span { 1, 1 }, Offer { 2.0, 3 }, cheaper));
    EXPECT_FALSE(offers.merge(Span { 0, 4 }, Offer { 7.0, 4 }, cheaper));  // not cheaper: unchanged
    const auto sources = [&offers]
    {
        auto order = std::vector<int>();
        for (const auto& [span, offer] : offers)
            order.push_back(offer.source);
        return order;
    };
    EXPECT_EQ(sources(), (std::vector<int> { 3, 2, 1 }));                 // equal costs by span: {1, 1} before {2, 3}
    EXPECT_TRUE(offers.merge(Span { 0, 4 }, Offer { 1.0, 5 }, cheaper));  // cheaper: moves to the front
    EXPECT_EQ(sources(), (std::vector<int> { 5, 3, 2 }));
    offers.update(Span { 1, 1 }, [](Offer& offer, bool) { offer.cost = 9.0; });
    EXPECT_EQ(sources(), (std::vector<int> { 5, 2, 3 }));
    EXPECT_EQ(std::prev(offers.end())->first.lower, 1);  // the most expensive last
    EXPECT_EQ(offers.at(Span { 2, 3 }).source, 2);       // found by its span
    EXPECT_EQ(offers.read(Span { 7, 7 }, [](const Offer& offer) { return offer.source; }, -1), -1);
    offers.insert_or_assign(Span { 2, 3 }, Offer { 0.5, 6 });
    EXPECT_EQ(sources(), (std::vector<int> { 6, 5, 3 }));
}

// Entries whose projections tie come in key order, whatever order they were inserted in.
TEST(OrderByValue, TiesGoByKey)
{
    auto ranked = Store<key<int>, value<int>, urd::sorted, urd::small, urd::order_by<[](int, int score) { return score; }>>();
    for (const int key : { 5, 1, 3, 4, 2 })
        ranked.insert_or_assign(key, key % 2);
    auto keys = std::vector<int>();
    for (const auto& [key, score] : ranked)
        keys.push_back(key);
    EXPECT_EQ(keys, (std::vector<int> { 2, 4, 1, 3, 5 }));
}

}  // namespace
