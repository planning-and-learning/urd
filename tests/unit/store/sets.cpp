// Sets and maps: random use against the standard's containers, set algebra, positions, ends, entries, Stores as keys, and orders by a projection. Each family
// header a Store here needs is included, and no other.

#include "names.hpp"
#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/flat_map.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/queues/addressable_heap.hpp"
#include "urd/repr/queues/bucket_queue.hpp"
#include "urd/repr/queues/queue.hpp"
#include "urd/repr/queues/ring_fifo.hpp"
#include "urd/repr/sequences/jagged.hpp"
#include "urd/repr/sequences/row_pool.hpp"
#include "urd/repr/sequences/segmented.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/flat_set.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/values/std_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/repr/wrappers/recycling.hpp"
#include "urd/store.hpp"
#include "urd/testing/allocations.hpp"

#include <algorithm>
#include <chrono>
#include <compare>
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

using urd::map;
using urd::multiset;
using urd::queue;
using urd::sequence;
using urd::set;
using urd::Store;
using SortedKey = Store<set<int>, urd::sorted, urd::frozen>;
using BitKey = Store<set<int>, urd::dense, urd::frozen, urd::sorted>;

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
        if constexpr (urd::has<S, urd::sorted>)
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

// GCC 13 at -O3 follows the first lookup into the map it has just seen built empty, and reports the key comparison
// that lookup never reaches (the table has no slot yet) as reading out of bounds.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
TEST(Store, KeysThatNameTheirIdentity)
{
    const auto facts = std::vector<Fact> { { 3, {} }, { 1, { 2 } }, { 1, { 2 } }, { 1, { 1 } } };
    const auto hashed = Store<set<Fact>>(facts.begin(), facts.end());
    EXPECT_EQ(hashed.size(), 3U);
    EXPECT_TRUE(hashed.contains(Fact { 1, { 2 } }));

    // Ordered member by member: predicate, then objects.
    const auto tree = Store<set<Fact>, urd::sorted>(facts.begin(), facts.end());
    const auto flat = Store<set<Fact>, urd::sorted, urd::frozen>(facts.begin(), facts.end());
    EXPECT_EQ(tree.begin()->objects, std::vector<int>({ 1 }));
    EXPECT_EQ(flat.begin()->objects, std::vector<int>({ 1 }));
    EXPECT_EQ(std::prev(flat.end())->predicate, 3);

    auto counts = Store<map<Fact, int>>();
    counts[Fact { 1, {} }] += 2;
    EXPECT_EQ(counts.at(Fact { 1, {} }), 2);
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

TEST(Store, BehavesLikeTheStandardContainers)
{
    for (unsigned seed = 0; seed < 5; ++seed)
    {
        behaves_like_std_set<Store<set<int>>>(seed);
        behaves_like_std_set<Store<set<int>, urd::sorted>>(seed);
        behaves_like_std_set<Store<set<int>, urd::small>>(seed);
        behaves_like_std_set<Store<set<int>, urd::sorted, urd::small>>(seed);
        behaves_like_std_multiset<Store<multiset<int>, urd::sorted>>(seed);
    }
}

TEST(Store, BuildsAndChangesAsAsked)
{
    // Built from elements, which the representation puts in the order asked for.
    const auto ids = Store<set<int>, urd::sorted> { 3, 1, 3, 2 };
    EXPECT_TRUE(std::vector<int>(ids.begin(), ids.end()) == std::vector<int>({ 1, 2, 3 }));
    EXPECT_TRUE(*ids.lower_bound(2) == 2 && ids.upper_bound(3) == ids.end() && *ids.rbegin() == 3);
    const auto first = Store<multiset<int>, urd::sorted, urd::nonempty> { 5, 4 };
    EXPECT_TRUE(first.front() == 4 && first[1] == 5 && first.back() == 5);

    const auto frozen = Store<set<int>, urd::sorted, urd::frozen> { 2, 3, 5 };
    EXPECT_TRUE(frozen.contains(5) && !frozen.contains(4));

    // A plain sequence reads and changes like std::vector.
    auto numbers = Store<sequence<int>> { 1, 2, 3, 2 };
    numbers[0] = 7;
    numbers.insert(numbers.begin() + 1, 9);
    EXPECT_TRUE(erase(numbers, 2) == 2);
    EXPECT_TRUE(numbers == (Store<sequence<int>> { 7, 9, 3 }) && numbers.at(2) == 3 && numbers < Store<sequence<int>> { 8 });
    EXPECT_TRUE(erase_if(numbers, [](int element) { return element > 8; }) == 1 && numbers.size() == 2);
}

// <=> and erase compare as == does (urd::EqualTo, urd::Less): a NaN equals a NaN and orders after every number.
TEST(Store, OrdersAndErasesAsItCompares)
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto with_nan = Store<sequence<double>> { 1.0, nan };
    EXPECT_TRUE(with_nan == (Store<sequence<double>> { 1.0, nan }));
    EXPECT_EQ(with_nan <=> (Store<sequence<double>> { 1.0, nan }), std::strong_ordering::equal);
    EXPECT_EQ(with_nan <=> (Store<sequence<double>> { 1.0, 2.0 }), std::strong_ordering::greater);
    auto numbers = Store<sequence<double>> { nan, 1.0, nan };
    EXPECT_EQ(erase(numbers, nan), 2U);
    EXPECT_TRUE(numbers == (Store<sequence<double>> { 1.0 }));
}

namespace
{

template<typename S>
class SetAlgebra : public testing::Test
{
};
using Sets = testing::Types<Store<set<int>>,
                            Store<set<int>, urd::sorted>,
                            Store<set<int>, urd::small>,
                            Store<set<int>, urd::stable_refs>,
                            Store<set<int>, urd::dense>,
                            Store<set<int>, urd::dense, urd::constant_clear>,
                            Store<set<int>, urd::insertion_ordered>,
                            Store<set<int>, urd::dense, urd::proportional_clear>>;
TYPED_TEST_SUITE(SetAlgebra, Sets);
// Inclusion, overlap, union, intersection and difference: the Stores whose container has them (a bitset's words).
template<typename S>
class DenseSetAlgebra : public testing::Test
{
};
using DenseSets = testing::Types<Store<set<int>, urd::dense>, Store<set<int>, urd::dense, urd::proportional_clear>>;
TYPED_TEST_SUITE(DenseSetAlgebra, DenseSets);

std::vector<int> sorted_copy(const auto& set)
{
    auto result = std::vector<int>(set.begin(), set.end());
    std::ranges::sort(result);
    return result;
}

}  // namespace

// Union, intersection, difference, inclusion and overlap give what std::set_* give on sorted copies.
TYPED_TEST(DenseSetAlgebra, AgreesWithTheStandardAlgorithms)
{
    auto random = std::mt19937(5);
    auto pick = std::uniform_int_distribution<int>(0, 40);
    for (int round = 0; round < 200; ++round)
    {
        auto lhs = urd::test::make_empty<TypeParam>();
        auto rhs = urd::test::make_empty<TypeParam>();
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
    auto names = Store<set<std::string>, urd::insertion_ordered>(urd::layer { 0 });
    names.insert("b");
    names.insert("a");
    EXPECT_EQ(names[0], "b");
    EXPECT_EQ(names.back(), "a");
    EXPECT_EQ(names.position(names.find("a")), 1U);
}

// A sorted Store's least and greatest elements are its first and last, O(1) in a tree or a sorted array.
TEST(Ends, SortedStoresHaveTheirLeastAndGreatest)
{
    const auto tree = Store<set<int>, urd::sorted> { 5, 1, 3 };
    EXPECT_EQ(tree.front(), 1);
    EXPECT_EQ(tree.back(), 5);
    const auto flat = Store<set<int>, urd::sorted, urd::frozen> { 5, 1, 3 };
    EXPECT_EQ(flat.front(), 1);
    const auto map = Store<urd::map<int, int>, urd::sorted> { { 2, 20 }, { 1, 10 } };
    EXPECT_EQ(map.front().second, 10);
    EXPECT_EQ(map.back().first, 2);
}
template<typename S>
concept has_front = requires(const S& store) { store.front(); };
static_assert(!has_front<Store<set<int>, urd::dense, urd::sorted>> && !has_front<Store<set<int>>>);

// An insertion-ordered map gives its keys and values as spans in insertion order, and an entry's position.
TEST(Entries, KeysAndValuesInInsertionOrder)
{
    auto costs = Store<map<int, double>, urd::dense, urd::insertion_ordered>();
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
TYPED_TEST(DenseSetAlgebra, UniteReportsWhetherItAdded)
{
    auto random = std::mt19937(43);
    for (int round = 0; round < 200; ++round)
    {
        auto lhs = urd::test::make_empty<TypeParam>();
        auto rhs = urd::test::make_empty<TypeParam>();
        for (int count = static_cast<int>(random() % 8); count > 0; --count)
            lhs.insert(static_cast<int>(random() % 20));
        for (int count = static_cast<int>(random() % 8); count > 0; --count)
            rhs.insert(static_cast<int>(random() % 20));
        const bool adds = !lhs.includes(rhs);
        const auto before = lhs.size();
        lhs |= rhs;
        EXPECT_EQ(lhs.size() != before, adds);  // a union only adds, so the size says whether it did
        EXPECT_TRUE(lhs.includes(rhs));
        const auto united = lhs.size();
        lhs |= rhs;
        EXPECT_EQ(lhs.size(), united);
    }
}

// As std::set::merge: the target gains what it lacked, the source keeps what the target had.
TYPED_TEST(SetAlgebra, MergeAgreesWithTheStandardSets)
{
    auto random = std::mt19937(47);
    for (int round = 0; round < 200; ++round)
    {
        auto lhs = urd::test::make_empty<TypeParam>();
        auto rhs = urd::test::make_empty<TypeParam>();
        auto lhs_expected = std::set<int>();
        auto rhs_expected = std::set<int>();
        for (int count = static_cast<int>(random() % 8); count > 0; --count)
        {
            const auto element = static_cast<int>(random() % 20);
            lhs.insert(element);
            lhs_expected.insert(element);
        }
        for (int count = static_cast<int>(random() % 8); count > 0; --count)
        {
            const auto element = static_cast<int>(random() % 20);
            rhs.insert(element);
            rhs_expected.insert(element);
        }
        lhs.merge(rhs);
        lhs_expected.merge(rhs_expected);
        EXPECT_EQ(sorted_copy(lhs), std::vector<int>(lhs_expected.begin(), lhs_expected.end()));
        EXPECT_EQ(sorted_copy(rhs), std::vector<int>(rhs_expected.begin(), rhs_expected.end()));
    }
}

// Stores as sorted keys: ordered lexicographically by their elements, equal by contents.
TEST(Less, StoresAsKeys)
{
    using Inner = Store<set<int>, urd::sorted>;
    const auto less = urd::Less<Inner> {};
    EXPECT_TRUE(less(Inner { 1, 2 }, Inner { 1, 3 }));
    EXPECT_TRUE(less(Inner { 1, 2 }, Inner { 1, 2, 3 }));
    EXPECT_FALSE(less(Inner { 2, 1 }, Inner { 1, 2 }));
    EXPECT_TRUE(urd::EqualTo<Inner> {}(Inner { 2, 1 }, Inner { 1, 2 }));

    auto by_contents = Store<set<Inner>, urd::sorted>();
    by_contents.insert(Inner { 3 });
    by_contents.insert(Inner { 1, 2 });
    by_contents.insert(Inner { 2, 1 });
    EXPECT_EQ(by_contents.size(), 2u);
    EXPECT_EQ(*by_contents.begin(), (Inner { 1, 2 }));
}

// Stores as hashed keys: found whatever order the key was built in, and whatever storage a bitset grew to.
TEST(Hash, StoresAsKeys)
{
    auto plans = Store<map<SortedKey, int>>();
    plans.insert({ SortedKey { 3, 1, 2 }, 7 });
    EXPECT_EQ(plans.at(SortedKey { 1, 2, 3 }), 7);
    EXPECT_FALSE(plans.contains(SortedKey { 1, 2 }));

    auto grown = Store<set<int>, urd::dense, urd::sorted> { 1, 2 };
    grown.insert(200);
    grown.erase(200);
    const auto small = Store<set<int>, urd::dense, urd::sorted> { 1, 2 };
    EXPECT_EQ(grown, small);
    using Key = Store<set<int>, urd::dense, urd::sorted>;
    EXPECT_EQ(urd::Hash<Key> {}(grown), urd::Hash<Key> {}(small));
    auto seen = Store<set<Key>>();
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

// The order is the projection and nothing else: two nodes with equal f are one element of a unique Store, as with
// any comparator; to keep both, the projection names what tells them apart.
TEST(OrderBy, EqualProjectionsAreOneElement)
{
    using ByF = Store<set<Node>, urd::sorted, urd::order_by<urd::by<&Node::f>>>;
    static_assert(std::same_as<ByF::representation, std::set<Node, urd::by<&Node::f>>>);
    const auto ids = [](const auto& nodes)
    {
        auto result = std::vector<int>();
        for (const auto& node : nodes)
            result.push_back(node.id);
        return result;
    };
    const auto nodes = ByF { Node { 1, 5, 0 }, Node { 2, 3, 0 }, Node { 3, 5, 0 }, Node { 4, 1, 0 } };
    EXPECT_EQ(ids(nodes), (std::vector<int> { 4, 2, 1 }));
    EXPECT_TRUE(nodes.contains(Node { 3, 5, 0 }));  // equivalent to node 1

    using ByFThenId = Store<set<Node>, urd::sorted, urd::order_by<urd::by<[](const Node& node) { return std::tuple(node.f, node.id); }>>>;
    const auto distinct = ByFThenId { Node { 1, 5, 0 }, Node { 2, 3, 0 }, Node { 3, 5, 0 }, Node { 4, 1, 0 } };
    EXPECT_EQ(ids(distinct), (std::vector<int> { 4, 2, 1, 3 }));
}

TEST(OrderBy, MapsOrderTheirKeysByTheProjection)
{
    auto costs = Store<map<Node, std::string>, urd::sorted, urd::order_by<urd::by<&Node::f>>>();
    costs.insert_or_assign(Node { 1, 9, 0 }, "late");
    costs.insert_or_assign(Node { 2, 2, 0 }, "early");
    EXPECT_EQ(costs.begin()->second, "early");
}

TEST(OrderBy, PriorityQueuePopsByTheProjection)
{
    using Open = Store<queue<Node>, urd::by_priority, urd::order_by<urd::by<[](const Node& node) { return std::tuple(node.f, -node.g); }>>>;
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

// A queue's elements need no order of their own where its comparator gives one.
TEST(OrderBy, QueueOfUnorderedElements)
{
    struct Task
    {
        int cost;
        std::string name;
    };
    auto tasks = Store<queue<Task>, urd::by_priority, urd::order_by<urd::by<&Task::cost>>>();
    tasks.push(Task { 7, "b" });
    tasks.push(Task { 2, "a" });
    EXPECT_EQ(tasks.top().name, "a");
}

}  // namespace
