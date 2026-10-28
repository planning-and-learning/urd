// A Store resolves the same request to the same type in any order, to the representation its properties and
// workload select, offers what was asked for, and behaves like the reference container under random use.

#include "urd/store.hpp"

#include "urd/concepts.hpp"
#include "urd/graph.hpp"

#include <concepts>
#include <functional>
#include <gtest/gtest.h>
#include <iterator>
#include <map>
#include <random>
#include <ranges>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{

using urd::key;
using urd::Store;
using urd::value;

// The order of the properties does not matter.
static_assert(std::same_as<Store<key<int>, urd::sorted>, Store<urd::sorted, key<int>>>);
static_assert(std::same_as<Store<value<int>, urd::unique, urd::sorted>, Store<urd::sorted, value<int>, urd::unique>>);

// Implied properties: a set's keys are unique, values without keys are a sequence, frozen keeps references.
static_assert(urd::has<Store<key<int>>, urd::unique>);
static_assert(urd::has<Store<value<int>>, urd::positional, urd::functional, urd::dense>);
static_assert(urd::has<Store<key<int>, urd::frozen>, urd::stable_refs>);

// What each request resolves to (pinned, so a change in the selection shows up here). Where no order is asked
// for, an open-addressing hash table, which docs/BENCHMARKS.md finds fastest at every size.
// The registry's names: each container with urd's hash, equality and order.
using urd::detail::FlatHashMapOf;
using urd::detail::FlatHashSet;
using urd::detail::FlatMapOf;
using urd::detail::FlatMultiSet;
using urd::detail::FlatSet;
using urd::detail::HashMapOf;
using urd::detail::HashMultiMapOf;
using urd::detail::HashSet;
using urd::detail::TreeMapOf;
using urd::detail::TreeMultiMapOf;
using urd::detail::TreeSet;
static_assert(std::same_as<Store<key<int>>::representation, FlatHashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted>::representation, TreeSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::frozen>::representation, FlatSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::small>::representation, FlatHashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::stable_refs>::representation, HashSet<int>>);
// Few elements: contiguous storage first, node-based containers last, a hash table before a tree among them.
static_assert(std::same_as<Store<key<int>, urd::small, urd::stable_refs>::representation, HashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::small, urd::sorted, urd::stable_refs>::representation, TreeSet<int>>);
static_assert(std::same_as<Store<value<int>>::representation, std::vector<int>>);
static_assert(std::same_as<Store<value<int>, urd::sorted>::representation, FlatMultiSet<int>>);
// Elements that must not move: segments, unless nothing changes anyway.
static_assert(std::same_as<Store<value<int>, urd::stable_refs, urd::monotone>::representation, urd::repr::SegmentedVector<int>>);
static_assert(std::same_as<Store<value<int>, urd::monotone, urd::concurrent>::representation,
                           urd::repr::SegmentedVector<int, urd::repr::default_first_segment_size, true>>);
static_assert(std::same_as<Store<value<int>, urd::stable_refs, urd::frozen>::representation, std::vector<int>>);
// Maps: by key, as the sets are by element.
static_assert(std::same_as<Store<key<int>, value<int>>::representation, FlatHashMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::sorted>::representation, TreeMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::small>::representation, FlatHashMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::sorted, urd::frozen>::representation, FlatMapOf<int, int>>);
// A multimap is postings: a key's values together, one probe away. Stable references need the node-based table.
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi>::representation, urd::repr::PostingMap<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi, urd::stable_refs>::representation, HashMultiMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi, urd::sorted>::representation, TreeMultiMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::stable_refs>::representation, HashMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::retains>::representation, FlatHashMapOf<int, int>>);
// A map of rows holds the inner Stores; flattening by dense key comes with dense maps.
static_assert(std::same_as<Store<key<int>, value<Store<value<int>, urd::frozen>>>::representation, FlatHashMapOf<int, Store<value<int>, urd::frozen>>>);
// A map's values change by key, never through its iterators; only a map with unique keys has at and try_emplace.
template<typename S>
concept can_assign_by_key = requires(S store) { store[1] = 2; };
template<typename S>
concept can_try_emplace = requires(S store) { store.try_emplace(1, 2); };
static_assert(can_assign_by_key<Store<key<int>, value<int>>> && !can_assign_by_key<Store<key<int>, value<int>, urd::frozen>>);
static_assert(can_try_emplace<Store<key<int>, value<int>>> && !can_try_emplace<Store<key<int>, value<int>, urd::multi>>);
static_assert(std::same_as<decltype(*Store<key<int>, value<int>>().begin()), const std::pair<const int, int>&>);

// Dense keys (indices): stored by position, a bitset where nothing else is asked, a sparse set where clear must be
// O(1).
static_assert(std::same_as<Store<key<int>, urd::dense>::representation, urd::repr::DenseSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::dense, urd::sorted>::representation, urd::repr::DenseSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::dense, urd::constant_clear>::representation, urd::repr::SparseSet<int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::dense>::representation, urd::repr::IndexMap<int, int, false>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::dense, urd::constant_clear>::representation, urd::repr::IndexMap<int, int, true>>);

// Insertion order and positions: an indexed set. A set that is positional keeps insertion order unless it asks to be
// sorted, so that an element's position is its index (an interner).
static_assert(std::same_as<Store<key<int>, urd::insertion_ordered>::representation, urd::repr::IndexedSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::positional>::representation, urd::repr::IndexedSet<int>>);
static_assert(urd::has<Store<key<int>, urd::positional>, urd::insertion_ordered, urd::unique>);
static_assert(std::same_as<Store<key<int>, urd::positional, urd::sorted>::representation, FlatSet<int>>);
static_assert(!urd::has<Store<key<int>, urd::insertion_ordered>, urd::positional>);

// A frozen multimap by dense key: compressed sparse rows, each key's values a span.
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen>::representation, urd::repr::CsrMultiMap<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::sorted>::representation, urd::repr::CsrMultiMap<int, int>>);

// Rows: a Store of Stores.
static_assert(std::same_as<Store<value<Store<value<int>>>>::representation, std::vector<Store<value<int>>>>);
static_assert(std::same_as<Store<value<Store<value<int>, urd::frozen>>>::representation, urd::repr::Jagged<Store<value<int>, urd::frozen>>>);
static_assert(std::same_as<Store<value<Store<value<Store<value<int>, urd::frozen>>, urd::frozen>>>::representation,
                           urd::repr::Jagged<Store<value<Store<value<int>, urd::frozen>>, urd::frozen>>>);
static_assert(std::same_as<Store<value<Store<value<Store<value<int>, urd::frozen>>, urd::frozen>>>::representation::value_type::representation,
                           urd::repr::Jagged<Store<value<int>, urd::frozen>>>);
static_assert(std::same_as<Store<value<Store<value<int>, urd::retains>>, urd::retains>::representation, urd::repr::RowPool<Store<value<int>, urd::retains>>>);

// Storage kept for reuse, or returned: the containers that do so.
static_assert(std::same_as<Store<key<int>, urd::retains>::representation, FlatHashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::retains>::representation, FlatSet<int>>);
static_assert(std::same_as<Store<value<int>, urd::retains>::representation, std::vector<int>>);
static_assert(std::same_as<Store<key<int>, urd::reclaims>::representation, TreeSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::reclaims>::representation, TreeSet<int>>);

// Only what was asked for: a set that reclaims is a tree, which is sorted, but it was not asked to be sorted, so it
// is not.
static_assert(!urd::Sorted<Store<key<int>, urd::reclaims>>);
static_assert(urd::Sorted<Store<key<int>, urd::sorted, urd::small>>);
static_assert(urd::Unique<Store<key<int>, urd::small>>);

// A Store adds no space to its representation: the facets it does not have are empty bases.
static_assert(sizeof(Store<value<int>>) == sizeof(Store<value<int>>::representation));
static_assert(sizeof(Store<key<int>, value<int>, urd::sorted>) == sizeof(Store<key<int>, value<int>, urd::sorted>::representation));

// The interface follows from the properties, in the standard's names.
template<typename S>
concept can_insert = requires(S store, int element) { store.insert(element); };
template<typename S>
concept can_push_back = requires(S store, int element) { store.push_back(element); };
template<typename S>
concept can_erase = requires(S store, int element) { store.erase(store.begin()); };
template<typename S>
concept can_search = requires(const S store, int element) { store.lower_bound(element); };
template<typename S>
concept can_assign = requires(S store, int element) { store[0] = element; };
static_assert(can_insert<Store<key<int>>> && can_erase<Store<key<int>>> && !can_push_back<Store<key<int>>>);
static_assert(!can_insert<Store<key<int>, urd::frozen>> && !can_erase<Store<key<int>, urd::frozen>>);
static_assert(can_push_back<Store<value<int>, urd::monotone>> && !can_erase<Store<value<int>, urd::monotone>>);
static_assert(!can_erase<Store<key<int>, urd::nonempty>>);
// A sorted Store is searched by order; a sorted sequence keeps elements by value, as std::flat_multiset.
static_assert(can_search<Store<key<int>, urd::sorted>> && !can_search<Store<key<int>>>);
static_assert(can_insert<Store<value<int>, urd::sorted>> && !can_push_back<Store<value<int>, urd::sorted>>);
// Elements change in place only where no property depends on their values and the Store may change.
static_assert(can_assign<Store<value<int>>>);
static_assert(!can_assign<Store<value<int>, urd::sorted>> && !can_assign<Store<value<int>, urd::monotone>>);
static_assert(!can_assign<Store<key<int>, urd::positional>>);
// A unique set's insert says whether it inserted; a multiset's does not.
static_assert(std::same_as<decltype(Store<key<int>>().insert(1).second), bool>);
static_assert(std::same_as<decltype(Store<value<int>, urd::sorted>().insert(1)), Store<value<int>, urd::sorted>::iterator>);
// Every Store offers what its properties require of a container.
static_assert(urd::conforming<Store<key<int>>> && urd::conforming<Store<key<int>, urd::sorted>>);
static_assert(urd::conforming<Store<value<int>>> && urd::conforming<Store<value<int>, urd::sorted, urd::nonempty>>);
static_assert(urd::conforming<Store<value<int>, urd::stable_refs, urd::monotone>> && urd::conforming<Store<key<int>, urd::frozen>>);

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

}  // namespace

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
                            Store<key<int>, urd::insertion_ordered>>;
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
    }
}

// An interner: each new element gets the next index, which stays its index; a known one keeps its index.
TEST(Interner, GivesEachElementItsIndex)
{
    auto symbols = Store<key<std::string>, urd::positional, urd::monotone>();
    const auto index = [&symbols](const std::string& symbol) { return symbols.position(symbols.insert(symbol).first); };
    EXPECT_EQ(index("on"), 0U);
    EXPECT_EQ(index("clear"), 1U);
    EXPECT_EQ(index("on"), 0U);
    EXPECT_EQ(symbols[1], "clear");
    EXPECT_EQ(symbols.position(symbols.find("clear")), 1U);
    EXPECT_EQ(symbols.position(symbols.find("holding")), symbols.size());
    EXPECT_EQ(std::vector<std::string>(symbols.rbegin(), symbols.rend()), (std::vector<std::string> { "clear", "on" }));
}

// The values of a key, in the order given: a span of a CSR multimap, a view of any other multimap's equal_range.
TEST(MultiValues, GivesEachKeysValuesInOrder)
{
    const auto pairs = std::vector<std::pair<int, int>> { { 2, 20 }, { 0, 1 }, { 2, 21 }, { 0, 2 }, { 2, 22 } };
    const auto csr = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen>(pairs.begin(), pairs.end());
    const auto tree = Store<key<int>, value<int>, urd::multi, urd::sorted>(pairs.begin(), pairs.end());
    const auto postings = Store<key<int>, value<int>, urd::multi>(pairs.begin(), pairs.end());
    const auto nodes = Store<key<int>, value<int>, urd::multi, urd::stable_refs>(pairs.begin(), pairs.end());
    EXPECT_TRUE(std::ranges::equal(postings.values(2), std::vector<int> { 20, 21, 22 }));
    // std::unordered_multimap keeps equal keys together, in no promised order.
    auto node_values = std::vector<int>(nodes.values(2).begin(), nodes.values(2).end());
    std::ranges::sort(node_values);
    EXPECT_EQ(node_values, (std::vector<int> { 20, 21, 22 }));
    EXPECT_TRUE(std::ranges::equal(csr.values(2), std::vector<int> { 20, 21, 22 }));
    EXPECT_TRUE(std::ranges::equal(tree.values(2), std::vector<int> { 20, 21, 22 }));
    EXPECT_TRUE(std::ranges::equal(csr.values(0), std::vector<int> { 1, 2 }));
    EXPECT_TRUE(csr.values(1).empty());
    EXPECT_TRUE(csr.values(7).empty());
    EXPECT_EQ(csr.size(), 5U);
}

// Built from pairs in any order, a CSR multimap holds what std::multimap holds, in the same order: frozen
// containers get no random operations, so their construction is compared here.
TEST(MultiValues, CsrBuildsWhatAMultimapHolds)
{
    auto random = std::mt19937(11);
    for (int round = 0; round < 200; ++round)
    {
        auto pairs = std::vector<std::pair<int, int>>();
        for (int count = static_cast<int>(random() % 40); count > 0; --count)
            pairs.emplace_back(static_cast<int>(random() % 12), static_cast<int>(random() % 100));
        const auto csr = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::sorted>(pairs.begin(), pairs.end());
        const auto expected = std::multimap<int, int>(pairs.begin(), pairs.end());
        ASSERT_TRUE(std::ranges::equal(csr, expected, [](const auto& lhs, const auto& rhs) { return lhs.first == rhs.first && lhs.second == rhs.second; }));
        for (int each = -1; each < 14; ++each)
        {
            EXPECT_EQ(csr.count(each), expected.count(each));
            EXPECT_EQ(std::ranges::distance(csr.lower_bound(each), csr.end()), std::ranges::distance(expected.lower_bound(each), expected.end()));
        }
        auto backwards = std::vector<int>();
        for (auto it = csr.end(); it != csr.begin();)
            backwards.push_back((--it)->first);
        EXPECT_TRUE(std::ranges::equal(backwards, expected | std::views::keys | std::views::reverse));
    }
}

// A graph is two CSR Stores: successors and predecessors, each in the order the edges were given.
TEST(Graph, GivesSuccessorsAndPredecessors)
{
    const auto edges = std::vector<std::pair<int, int>> { { 2, 0 }, { 0, 1 }, { 2, 1 }, { 1, 2 }, { 0, 2 } };
    const auto graph = urd::Graph<int>(4, edges);
    EXPECT_EQ(graph.num_vertices(), 4U);
    EXPECT_EQ(graph.num_edges(), 5U);
    EXPECT_TRUE(std::ranges::equal(graph.successors(0), std::vector<int> { 1, 2 }));
    EXPECT_TRUE(std::ranges::equal(graph.successors(2), std::vector<int> { 0, 1 }));
    EXPECT_TRUE(std::ranges::equal(graph.predecessors(1), std::vector<int> { 0, 2 }));
    EXPECT_TRUE(std::ranges::equal(graph.predecessors(2), std::vector<int> { 1, 0 }));
    EXPECT_TRUE(graph.successors(3).empty() && graph.predecessors(3).empty());
}
