// A Store resolves the same request to the same type in any order, to the representation its properties and
// workload select, offers what was asked for, and behaves like the reference container under random use.

#include "urd/store.hpp"

#include "urd/compose/family.hpp"
#include "urd/concepts.hpp"

#include <concepts>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <iterator>
#include <map>
#include <numeric>
#include <random>
#include <ranges>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
// A bitset that clears only the words it set: sorted, cleared in time proportional to its elements.
static_assert(std::same_as<Store<key<int>, urd::dense, urd::proportional_clear>::representation, urd::repr::DenseSet<int, true>>);
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

// Scopes and layers: a scoped map is a flat hash table with an undo log; a layered Store a flat hash table over a
// parent.
static_assert(std::same_as<Store<key<int>, value<int>, urd::scoped>::representation, urd::repr::ScopedMap<int, int>>);
static_assert(std::same_as<Store<key<int>, urd::layered>::representation, urd::repr::Layered<FlatHashSet<int>>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::layered>::representation, urd::repr::Layered<FlatHashMapOf<int, int>>>);

static_assert(std::same_as<Store<key<int>, urd::change_log>::representation, urd::repr::ChangeLogged<FlatHashSet<int>>>);

// Selection trusts the traits, so every registered container whose traits are declared for an element type must offer
// what they promise: checked here for the whole registry, for elements, maps and rows.
template<typename R>
constexpr bool conforms_where_declared()
{
    // Only a declared container is instantiated: an undeclared one may not even be valid for the element type.
    if constexpr (urd::container_traits<R>::declared)
        return urd::conforming<R>;
    else
        return true;
}
template<typename... Rs>
constexpr bool all_conform(urd::detail::type_list<Rs...> /*candidates*/)
{ return (conforms_where_declared<Rs>() && ...); }
template<typename... Ps>
using element_candidates = typename urd::detail::instantiated<typename urd::detail::to_spec<Ps...>::type, urd::detail::element_preference>::type;
template<typename... Ps>
using map_candidates = typename urd::detail::instantiated<typename urd::detail::to_spec<Ps...>::type, urd::detail::map_preference>::type;
// Called parenthesized: argument-dependent lookup would instantiate every candidate to find associated names.
static_assert((all_conform) (element_candidates<key<int>> {}));
static_assert((all_conform) (element_candidates<value<std::string>> {}));
static_assert((all_conform) (element_candidates<value<Store<value<int>, urd::frozen>>> {}));
static_assert((all_conform) (element_candidates<value<Store<key<int>, urd::dense, urd::frozen>>> {}));
static_assert((all_conform) (map_candidates<key<int>, value<int>> {}));
static_assert((all_conform) (map_candidates<key<std::string>, value<double>> {}));

// An interner shared by threads: insertion order, elements that never move, inserts from threads at once.
static_assert(std::same_as<Store<key<int>, urd::positional, urd::monotone, urd::concurrent>::representation, urd::repr::ConcurrentIndexedSet<int>>);

// Maps in insertion order: hashed keys, or dense ones placed by index with O(1) clear; a dense set in insertion order.
static_assert(std::same_as<Store<key<int>, value<double>, urd::insertion_ordered>::representation, urd::repr::OrderedMap<urd::repr::IndexedSet<int>, double>>);
static_assert(std::same_as<Store<key<int>, value<double>, urd::dense, urd::insertion_ordered>::representation,
                           urd::repr::OrderedMap<urd::repr::SparseSet<int, true>, double>>);
static_assert(urd::has<Store<key<int>, value<double>, urd::dense, urd::insertion_ordered>::representation, urd::constant_clear>);
static_assert(std::same_as<Store<key<int>, urd::dense, urd::insertion_ordered>::representation, urd::repr::SparseSet<int, true>>);

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

// A frozen graph: a multimap from sources to targets that answers by value too, so successors are a vertex's values
// and predecessors its keys, each in the order the edges were given.
static_assert(
    std::same_as<Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>::representation, urd::repr::BidirectionalCsr<int, int>>);

// Built from pairs in any order, the keys of each value are those paired with it, in the order given.
TEST(Graph, KeysOfAValueAreItsPairsInOrder)
{
    auto random = std::mt19937(37);
    for (int round = 0; round < 200; ++round)
    {
        auto pairs = std::vector<std::pair<int, int>>();
        for (int count = static_cast<int>(random() % 40); count > 0; --count)
            pairs.emplace_back(static_cast<int>(random() % 12), static_cast<int>(random() % 12));
        const auto graph = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>(pairs.begin(), pairs.end());
        for (int target = 0; target < 14; ++target)
        {
            auto expected = std::vector<int>();
            for (const auto& [source, each] : pairs)
                if (each == target)
                    expected.push_back(source);
            ASSERT_TRUE(std::ranges::equal(graph.keys(target), expected)) << "round " << round << ", value " << target;
        }
    }
}

TEST(Graph, GivesSuccessorsAndPredecessors)
{
    const auto edges = std::vector<std::pair<int, int>> { { 2, 0 }, { 0, 1 }, { 2, 1 }, { 1, 2 }, { 0, 2 } };
    const auto graph = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>(edges.begin(), edges.end());
    EXPECT_EQ(graph.size(), 5U);
    EXPECT_TRUE(std::ranges::equal(graph.values(0), std::vector<int> { 1, 2 }));
    EXPECT_TRUE(std::ranges::equal(graph.values(2), std::vector<int> { 0, 1 }));
    EXPECT_TRUE(std::ranges::equal(graph.keys(1), std::vector<int> { 0, 2 }));
    EXPECT_TRUE(std::ranges::equal(graph.keys(2), std::vector<int> { 1, 0 }));
    EXPECT_TRUE(graph.values(3).empty() && graph.keys(3).empty());
}

// Scopes undo, latest first, every change made in them: random changes and scopes against a stack of copies.
TEST(Scoped, PopUndoesTheScopesChanges)
{
    using Symbols = Store<key<int>, value<int>, urd::scoped>;
    auto random = std::mt19937(3);
    for (int round = 0; round < 50; ++round)
    {
        auto symbols = Symbols();
        auto saved = std::vector<std::map<int, int>>();
        const auto contents = [&symbols] { return std::map<int, int>(symbols.begin(), symbols.end()); };
        for (int step = 0; step < 300; ++step)
        {
            const int key = static_cast<int>(random() % 16);
            const int value = static_cast<int>(random() % 100);
            switch (random() % 7)
            {
                case 0:
                    symbols.push_scope();
                    saved.push_back(contents());
                    break;
                case 1:
                    if (!saved.empty())
                    {
                        symbols.pop_scope();
                        ASSERT_EQ(contents(), saved.back()) << "step " << step;
                        saved.pop_back();
                    }
                    break;
                case 2:
                    symbols.erase(key);
                    break;
                case 3:
                    symbols.insert_or_assign(key, value);
                    break;
                case 4:
                    symbols[key] = value;
                    break;
                case 5:
                    symbols.try_emplace(key, value);
                    break;
                default:
                    if (symbols.contains(key))
                        symbols.at(key) += 1;
                    break;
            }
            ASSERT_EQ(symbols.depth(), saved.size());
        }
    }
}

// A layer finds its parent's elements, holds only its own, and shadows the parent's values with its own.
TEST(Layered, AnswersFromItselfThenItsParent)
{
    using Names = Store<key<std::string>, value<int>, urd::layered>;
    const auto parent = Names { { "on", 0 }, { "clear", 1 } };
    auto child = Names { { "holding", 2 } };
    child.layer_over(&parent);
    EXPECT_TRUE(child.contains("on") && child.contains("holding") && !child.contains("empty"));
    EXPECT_EQ(child.size(), 1U);
    EXPECT_EQ(std::as_const(child).at("clear"), 1);
    EXPECT_FALSE(child.try_emplace("on", 7).second);
    EXPECT_EQ(std::as_const(child).at("on"), 0);
    child.insert_or_assign("on", 5);
    EXPECT_EQ(child.at("on"), 5);
    EXPECT_EQ(parent.at("on"), 0);
    EXPECT_EQ(child.erase("clear"), 0U);
    EXPECT_TRUE(child.contains("clear"));
    // at() on a layer that may change hands out its own value to change: the parent's is copied up first.
    child.at("clear") = 3;
    EXPECT_EQ(child.size(), 3U);
    EXPECT_EQ(parent.at("clear"), 1);

    using Seen = Store<key<int>, urd::layered>;
    const auto before = Seen { 1, 2 };
    auto after = Seen { 3 };
    after.layer_over(&before);
    EXPECT_FALSE(after.insert(2).second);
    EXPECT_TRUE(after.insert(4).second);
    EXPECT_EQ(std::set<int>(after.begin(), after.end()), (std::set<int> { 3, 4 }));
    after.layer_over(nullptr);
    EXPECT_FALSE(after.contains(1));
}

// A two-level dense map is two dense Stores nested: cleared at the outer level in O(1), refilled fresh.
TEST(Nested, TwoLevelDenseMap)
{
    using Rows = Store<key<int>, value<double>, urd::dense>;
    auto relations = Store<key<int>, value<Rows>, urd::dense, urd::constant_clear>();
    relations[2][5] = 1.5;
    relations[2][7] = 2.5;
    relations[0][1] = 0.5;
    EXPECT_EQ(relations.at(2).size(), 2U);
    relations.clear();
    EXPECT_TRUE(relations.empty());
    relations[2][1] = 3.0;
    EXPECT_EQ(relations.at(2).size(), 1U);
    EXPECT_FALSE(relations.at(2).contains(5));
}

// A partition: union-find over indices, by request.
static_assert(std::same_as<Store<key<int>, urd::partition>::representation, urd::repr::DisjointSets<int>>);

// A partition answers what a labeling recomputed after each merge answers.
TEST(Partition, AgreesWithRelabeling)
{
    auto random = std::mt19937(17);
    for (int round = 0; round < 50; ++round)
    {
        constexpr std::size_t size = 24;
        auto partition = Store<key<int>, urd::partition>();
        partition.insert(static_cast<int>(size) - 1);
        auto label = std::vector<std::size_t>(size);
        std::iota(label.begin(), label.end(), std::size_t { 0 });
        for (int step = 0; step < 40; ++step)
        {
            const auto lhs = static_cast<int>(random() % size);
            const auto rhs = static_cast<int>(random() % size);
            const auto old = label[static_cast<std::size_t>(rhs)];
            const auto joined = label[static_cast<std::size_t>(lhs)];
            EXPECT_EQ(partition.merge(lhs, rhs), old != joined);
            std::ranges::replace(label, old, joined);
            for (int each = 0; each < static_cast<int>(size); ++each)
            {
                ASSERT_EQ(partition.same(lhs, each), label[static_cast<std::size_t>(lhs)] == label[static_cast<std::size_t>(each)]);
                ASSERT_EQ(partition.class_size(each), static_cast<std::size_t>(std::ranges::count(label, label[static_cast<std::size_t>(each)])));
                ASSERT_TRUE(partition.same(partition.find(each), each));
            }
            auto distinct = label;
            std::ranges::sort(distinct);
            ASSERT_EQ(partition.classes(), static_cast<std::size_t>(std::ranges::distance(distinct.begin(), std::ranges::unique(distinct).begin())));
        }
    }
}

namespace
{

template<typename T>
using Interned = Store<key<T>, urd::positional, urd::monotone>;

}  // namespace

// A family holds one Store per type and reaches each by its type.
TEST(Family, ReachesEachMemberByType)
{
    auto repositories = urd::compose::Family<Interned, int, std::string>();
    repositories.get<int>().insert(7);
    repositories.get<std::string>().insert("on");
    repositories.get<std::string>().insert("clear");
    EXPECT_EQ(repositories.get<int>().size(), 1U);
    EXPECT_EQ(std::as_const(repositories).get<std::string>()[1], "clear");
    std::size_t total = 0;
    repositories.for_each([&total](const auto& member) { total += member.size(); });
    EXPECT_EQ(total, 3U);
}

// A change log holds exactly the difference between the contents and those when it was last cleared.
TEST(ChangeLog, HoldsTheDifferenceSinceItWasCleared)
{
    auto random = std::mt19937(29);
    auto facts = Store<key<int>, urd::change_log>();
    auto reference = std::set<int>();
    for (int step = 0; step < 2000; ++step)
    {
        const int element = static_cast<int>(random() % 20);
        switch (random() % 10)
        {
            case 0:
                facts.clear_changes();
                reference = std::set<int>(facts.begin(), facts.end());
                break;
            case 1:
                if (random() % 8 == 0)
                    facts.clear();
                break;
            case 2:
            case 3:
            case 4:
                facts.erase(element);
                break;
            default:
                facts.insert(element);
                break;
        }
        const auto now = std::set<int>(facts.begin(), facts.end());
        auto added = std::vector<int>();
        auto removed = std::vector<int>();
        std::ranges::set_difference(now, reference, std::back_inserter(added));
        std::ranges::set_difference(reference, now, std::back_inserter(removed));
        ASSERT_EQ(std::set<int>(facts.added().begin(), facts.added().end()), std::set<int>(added.begin(), added.end())) << "step " << step;
        ASSERT_EQ(facts.added().size(), added.size());
        ASSERT_EQ(std::set<int>(facts.removed().begin(), facts.removed().end()), std::set<int>(removed.begin(), removed.end())) << "step " << step;
        ASSERT_EQ(facts.removed().size(), removed.size());
    }
}

// An interner's table hashes through its own elements: copies and moves each keep theirs, and a moved-from interner
// starts empty and works.
TEST(Interner, CopiesAndMovesKeepTheirOwnIndex)
{
    using Symbols = Store<key<std::string>, urd::positional>;
    auto symbols = Symbols { "on", "clear" };
    auto copy = symbols;
    copy.insert("holding");
    EXPECT_FALSE(symbols.contains("holding"));
    auto moved = std::move(symbols);
    EXPECT_EQ(moved.position(moved.find("clear")), 1U);
    symbols.clear();  // a moved-from Store is valid; clearing it is how code reuses one
    symbols.insert("empty");
    EXPECT_EQ(symbols.size(), 1U);
    EXPECT_EQ(symbols.position(symbols.find("empty")), 0U);
    EXPECT_FALSE(moved.contains("empty"));
    copy = moved;
    EXPECT_FALSE(copy.contains("holding"));
    EXPECT_EQ(copy.position(copy.find("clear")), 1U);
}

// Integer elements of the same width as the positions the table holds are never taken for positions.
TEST(Interner, IntegerElementsAreNotPositions)
{
    auto ids = Store<key<std::uint32_t>, urd::positional>();
    for (const std::uint32_t id : { 2U, 0U, 1U, 0U, 7U })
        ids.insert(id);
    EXPECT_EQ(ids.size(), 4U);
    EXPECT_EQ(ids.position(ids.find(0U)), 1U);
    EXPECT_EQ(ids.position(ids.find(1U)), 2U);
    EXPECT_EQ(ids.position(ids.find(3U)), ids.size());
}

// Threads interning overlapping elements at once: each element gets one position, the same for every thread, and
// positions run 0..n-1 without gaps.
TEST(Interner, ThreadsAgreeOnPositions)
{
    auto symbols = Store<key<int>, urd::positional, urd::monotone, urd::concurrent>();
    constexpr int threads = 4;
    constexpr int each = 5000;
    auto seen = std::vector<std::vector<std::size_t>>(threads, std::vector<std::size_t>(each));
    {
        auto workers = std::vector<std::jthread>();
        for (int thread = 0; thread < threads; ++thread)
            workers.emplace_back(
                [&symbols, &seen, thread]
                {
                    // Every thread inserts the same elements, each starting at another point.
                    for (int step = 0; step < each; ++step)
                    {
                        const int element = (step + thread * each / threads) % each;
                        seen[static_cast<std::size_t>(thread)][static_cast<std::size_t>(element)] = symbols.position(symbols.insert(element).first);
                    }
                });
    }
    EXPECT_EQ(symbols.size(), static_cast<std::size_t>(each));
    for (int thread = 1; thread < threads; ++thread)
        EXPECT_EQ(seen[static_cast<std::size_t>(thread)], seen[0]);
    auto positions = seen[0];
    std::ranges::sort(positions);
    for (std::size_t position = 0; position < positions.size(); ++position)
        ASSERT_EQ(positions[position], position);
    for (int element = 0; element < each; ++element)
        ASSERT_EQ(symbols[seen[0][static_cast<std::size_t>(element)]], element);
}

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

namespace
{

/// A closed interval, merged by hull.
struct Interval
{
    double lower = 0;
    double upper = 0;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Interval::lower, &Interval::upper> {}; }
};
constexpr auto hull = [](const Interval& lhs, const Interval& rhs) { return Interval { std::min(lhs.lower, rhs.lower), std::max(lhs.upper, rhs.upper) }; };

template<typename S>
class Merge : public testing::Test
{
};
using MergeMaps = testing::Types<Store<key<int>, value<int>>,
                                 Store<key<int>, value<int>, urd::sorted>,
                                 Store<key<int>, value<int>, urd::dense>,
                                 Store<key<int>, value<int>, urd::dense, urd::constant_clear>,
                                 Store<key<int>, value<int>, urd::insertion_ordered>,
                                 Store<key<int>, value<int>, urd::dense, urd::insertion_ordered>,
                                 Store<key<int>, value<int>, urd::scoped>>;
TYPED_TEST_SUITE(Merge, MergeMaps);

}  // namespace

// merge inserts where a key has no value, combines where it has one, and says whether the map changed: against a
// std::map merged by hand.
TYPED_TEST(Merge, CombinesAndReportsChange)
{
    auto random = std::mt19937(41);
    auto map = TypeParam();
    auto expected = std::map<int, int>();
    const auto lower = [](int lhs, int rhs) { return std::min(lhs, rhs); };
    for (int step = 0; step < 2000; ++step)
    {
        const int key = static_cast<int>(random() % 30);
        const int value = static_cast<int>(random() % 100);
        const auto [found, inserted] = expected.try_emplace(key, value);
        const bool changed = inserted || value < found->second;
        found->second = std::min(found->second, value);
        ASSERT_EQ(map.merge(key, value, lower), changed) << "step " << step;
        ASSERT_EQ(map.at(key), expected.at(key));
    }
    EXPECT_EQ(map.size(), expected.size());
}

// An interval widened by hull only where it grows.
TEST(Merge, HullOfIntervals)
{
    auto values = Store<key<int>, value<Interval>, urd::dense, urd::insertion_ordered>();
    EXPECT_TRUE(values.merge(3, Interval { 1, 2 }, hull));
    EXPECT_FALSE(values.merge(3, Interval { 1.5, 2 }, hull));
    EXPECT_TRUE(values.merge(3, Interval { 0, 1 }, hull));
    EXPECT_EQ(values.at(3).lower, 0);
    EXPECT_EQ(values.at(3).upper, 2);
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

// update hands each value to a function with whether the key had one; read reads it or gives a default. Against a
// std::map updated by hand, over the map kinds of the merge suite.
TYPED_TEST(Merge, UpdateAndRead)
{
    auto random = std::mt19937(47);
    auto map = TypeParam();
    auto expected = std::map<int, int>();
    for (int step = 0; step < 2000; ++step)
    {
        const int key = static_cast<int>(random() % 30);
        const bool had = expected.contains(key);
        const auto reported = map.update(key,
                                         [](int& count, bool held)
                                         {
                                             count = held ? count + 1 : 1;
                                             return held;
                                         });
        ASSERT_EQ(reported, had) << "step " << step;
        expected[key] = had ? expected[key] + 1 : 1;
        const int probe = static_cast<int>(random() % 32);
        ASSERT_EQ(map.read(probe, [](int count) { return count; }, -1), expected.contains(probe) ? expected[probe] : -1);
    }
}

// A map cleared by generation hands over the value it kept, storage and all: a buffer refilled per round allocates
// once.
TEST(Merge, GenerationsReuseValues)
{
    auto buffers = Store<key<int>, value<std::vector<int>>, urd::dense, urd::constant_clear>();
    buffers.update(3, [](std::vector<int>& buffer, bool) { buffer.assign(1000, 7); });
    buffers.clear();
    EXPECT_FALSE(buffers.contains(3));
    buffers.update(3,
                   [](std::vector<int>& buffer, bool held)
                   {
                       EXPECT_FALSE(held);
                       EXPECT_GE(buffer.capacity(), 1000U);
                       buffer.clear();
                   });
    EXPECT_TRUE(buffers.contains(3));
    EXPECT_TRUE(buffers.at(3).empty());
}

// Threads updating and reading the same keys of a thread-safe generation map at once: every update counts.
TEST(Merge, ThreadsUpdateTogether)
{
    auto counts = Store<key<int>, value<int>, urd::dense, urd::constant_clear, urd::concurrent>();
    static_assert(std::same_as<decltype(counts)::representation, urd::repr::IndexMap<int, int, true, true>>);
    constexpr int threads = 4;
    constexpr int rounds = 20000;
    constexpr int keys = 50;
    {
        auto workers = std::vector<std::jthread>();
        for (int thread = 0; thread < threads; ++thread)
            workers.emplace_back(
                [&counts, thread]
                {
                    for (int round = 0; round < rounds; ++round)
                    {
                        counts.update((round + thread) % keys, [](int& count, bool held) { count = held ? count + 1 : 1; });
                        (void) counts.read(round % keys, [](int count) { return count; }, 0);
                    }
                });
    }
    int total = 0;
    for (int key = 0; key < keys; ++key)
        total += counts.at(key);
    EXPECT_EQ(total, threads * rounds);
}
