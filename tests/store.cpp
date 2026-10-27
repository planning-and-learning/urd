// A Store resolves the same request to the same type in any order, to the representation its properties and
// workload select, offers what was asked for, and behaves like the reference container under random use.

#include "urd/store.hpp"

#include "check.hpp"
#include "urd/concepts.hpp"

#include <concepts>
#include <functional>
#include <iterator>
#include <map>
#include <random>
#include <set>
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
template<typename T>
using FlatHashSet = boost::unordered_flat_set<T, std::hash<T>>;
template<typename K, typename V>
using FlatHashMap = boost::unordered_flat_map<K, V, std::hash<K>>;
static_assert(std::same_as<Store<key<int>>::representation, FlatHashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted>::representation, std::set<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::frozen>::representation, boost::container::flat_set<int>>);
static_assert(std::same_as<Store<key<int>, urd::small>::representation, FlatHashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::stable_refs>::representation, std::unordered_set<int>>);
static_assert(std::same_as<Store<value<int>>::representation, std::vector<int>>);
static_assert(std::same_as<Store<value<int>, urd::sorted>::representation, boost::container::flat_multiset<int>>);
// Elements that must not move: segments, unless nothing changes anyway.
static_assert(std::same_as<Store<value<int>, urd::stable_refs, urd::monotone>::representation, urd::repr::SegmentedVector<int>>);
static_assert(std::same_as<Store<value<int>, urd::monotone, urd::concurrent>::representation,
                           urd::repr::SegmentedVector<int, urd::repr::default_first_segment_size, true>>);
static_assert(std::same_as<Store<value<int>, urd::stable_refs, urd::frozen>::representation, std::vector<int>>);
// Maps: by key, as the sets are by element.
static_assert(std::same_as<Store<key<int>, value<int>>::representation, FlatHashMap<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::sorted>::representation, std::map<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::small>::representation, FlatHashMap<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::sorted, urd::frozen>::representation, boost::container::flat_map<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi>::representation, std::unordered_multimap<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi, urd::sorted>::representation, std::multimap<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::stable_refs>::representation, std::unordered_map<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::retains>::representation, FlatHashMap<int, int>>);
// A map of rows holds the inner Stores; flattening by dense key comes with dense maps.
static_assert(std::same_as<Store<key<int>, value<Store<value<int>, urd::frozen>>>::representation, FlatHashMap<int, Store<value<int>, urd::frozen>>>);
// A map's values change by key, never through its iterators; only a map with unique keys has at and try_emplace.
template<typename S>
concept can_assign_by_key = requires(S store) { store[1] = 2; };
template<typename S>
concept can_try_emplace = requires(S store) { store.try_emplace(1, 2); };
static_assert(can_assign_by_key<Store<key<int>, value<int>>> && !can_assign_by_key<Store<key<int>, value<int>, urd::frozen>>);
static_assert(can_try_emplace<Store<key<int>, value<int>>> && !can_try_emplace<Store<key<int>, value<int>, urd::multi>>);
static_assert(std::same_as<decltype(*Store<key<int>, value<int>>().begin()), const std::pair<const int, int>&>);

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
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::retains>::representation, boost::container::flat_set<int>>);
static_assert(std::same_as<Store<value<int>, urd::retains>::representation, std::vector<int>>);
static_assert(std::same_as<Store<key<int>, urd::reclaims>::representation, std::set<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::reclaims>::representation, std::set<int>>);

// Only what was asked for: a small set is stored sorted, but it was not asked to be sorted, so it is not.
static_assert(!urd::Sorted<Store<key<int>, urd::small>>);
static_assert(urd::Sorted<Store<key<int>, urd::sorted, urd::small>>);
static_assert(urd::Unique<Store<key<int>, urd::small>>);

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
            URD_CHECK(store.erase(element) == reference.erase(element));
        else
            URD_CHECK(store.insert(element).second == reference.insert(element).second);
        const int probe = pick(random);
        URD_CHECK(store.contains(probe) == reference.contains(probe));
        URD_CHECK(store.size() == reference.size());
        auto contents = std::vector<int>(store.begin(), store.end());
        std::ranges::sort(contents);
        URD_CHECK(contents == std::vector<int>(reference.begin(), reference.end()));
        if constexpr (urd::Sorted<S>)
            URD_CHECK(std::ranges::is_sorted(store));
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
            URD_CHECK(store.erase(element) == reference.erase(element));
        else
            URD_CHECK(*store.insert(element) == *reference.insert(element));
        URD_CHECK(std::vector<int>(store.begin(), store.end()) == std::vector<int>(reference.begin(), reference.end()));
        const int probe = pick(random);
        URD_CHECK(store.count(probe) == reference.count(probe));
        URD_CHECK(std::distance(store.begin(), store.lower_bound(probe)) == std::distance(reference.begin(), reference.lower_bound(probe)));
    }
}

}  // namespace

int main()
{
    for (unsigned seed = 0; seed < 5; ++seed)
    {
        behaves_like_std_set<Store<key<int>>>(seed);
        behaves_like_std_set<Store<key<int>, urd::sorted>>(seed);
        behaves_like_std_set<Store<key<int>, urd::small>>(seed);
        behaves_like_std_set<Store<key<int>, urd::sorted, urd::small>>(seed);
        behaves_like_std_multiset<Store<value<int>, urd::sorted>>(seed);
    }

    // Built from elements, or from elements claimed to have the properties.
    const auto ids = Store<key<int>, urd::sorted> { 3, 1, 3, 2 };
    URD_CHECK(std::vector<int>(ids.begin(), ids.end()) == std::vector<int>({ 1, 2, 3 }));
    URD_CHECK(*ids.lower_bound(2) == 2 && ids.upper_bound(3) == ids.end() && *ids.rbegin() == 3);
    const auto first = Store<value<int>, urd::sorted, urd::nonempty>::assume({ 4, 5 });
    URD_CHECK(first.front() == 4 && first[1] == 5 && first.back() == 5);

    const auto frozen = Store<key<int>, urd::sorted, urd::frozen> { 2, 3, 5 };
    URD_CHECK(frozen.contains(5) && !frozen.contains(4));

    // A plain sequence reads and changes like std::vector.
    auto numbers = Store<value<int>> { 1, 2, 3, 2 };
    numbers[0] = 7;
    numbers.insert(numbers.begin() + 1, 9);
    URD_CHECK(erase(numbers, 2) == 2);
    URD_CHECK(numbers == (Store<value<int>> { 7, 9, 3 }) && numbers.at(2) == 3 && numbers < Store<value<int>> { 8 });
    URD_CHECK(erase_if(numbers, [](int element) { return element > 8; }) == 1 && numbers.size() == 2);
}
