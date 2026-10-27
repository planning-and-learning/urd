// A Store resolves the same request to the same type in any order, to the representation its properties and
// workload select, offers what was asked for, and behaves like the reference container under random use.

#include "check.hpp"
#include "urd/concepts.hpp"
#include "urd/store.hpp"

#include <concepts>
#include <random>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>

namespace
{

using urd::key;
using urd::props;
using urd::Store;
using urd::value;

// The order of the properties does not matter.
static_assert(std::same_as<Store<key<int>, urd::sorted>, Store<urd::sorted, key<int>>>);
static_assert(std::same_as<Store<value<int>, urd::unique, urd::sorted>, Store<urd::sorted, value<int>, urd::unique>>);

// Implied properties: a set's keys are unique, values without keys are a sequence, frozen keeps references.
static_assert(urd::includes(Store<key<int>>::properties, props::unique));
static_assert(urd::includes(Store<value<int>>::properties, props::positional | props::functional | props::dense));
static_assert(urd::includes(Store<key<int>, urd::frozen>::properties, props::stable_refs));

// What each request resolves to (pinned, so a change in the selection shows up here).
static_assert(std::same_as<Store<key<int>>::representation, std::unordered_set<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted>::representation, std::set<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::frozen>::representation, boost::container::flat_set<int>>);
static_assert(std::same_as<Store<key<int>, urd::small>::representation, boost::container::flat_set<int>>);
static_assert(std::same_as<Store<key<int>, urd::stable_refs>::representation, std::unordered_set<int>>);
static_assert(std::same_as<Store<value<int>>::representation, std::vector<int>>);
static_assert(std::same_as<Store<value<int>, urd::sorted>::representation, boost::container::flat_multiset<int>>);
// Elements that must not move: segments, unless nothing changes anyway.
static_assert(std::same_as<Store<value<int>, urd::stable_refs, urd::monotone>::representation, urd::repr::SegmentedVector<int>>);
static_assert(std::same_as<Store<value<int>, urd::monotone, urd::concurrent>::representation,
                           urd::repr::SegmentedVector<int, urd::repr::default_first_segment_size, true>>);
static_assert(std::same_as<Store<value<int>, urd::stable_refs, urd::frozen>::representation, std::vector<int>>);

// Only what was asked for: a small set is stored sorted, but it was not asked to be sorted, so it is not.
static_assert(!urd::Sorted<Store<key<int>, urd::small>>);
static_assert(urd::Sorted<Store<key<int>, urd::sorted, urd::small>>);
static_assert(urd::Unique<Store<key<int>, urd::small>>);

// The interface follows from the properties.
template<typename S>
concept can_insert = requires(S store, int element) { store.insert(element); };
template<typename S>
concept can_erase = requires(S store, int element) { store.erase(element); };
static_assert(can_insert<Store<key<int>>> && can_erase<Store<key<int>>>);
static_assert(!can_insert<Store<key<int>, urd::frozen>> && !can_erase<Store<key<int>, urd::frozen>>);
static_assert(can_insert<Store<value<int>, urd::monotone>> && !can_erase<Store<value<int>, urd::monotone>>);
static_assert(!can_erase<Store<key<int>, urd::nonempty>>);

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
            URD_CHECK(store.insert(element) == reference.insert(element).second);
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
        {
            URD_CHECK(store.insert(element));
            reference.insert(element);
        }
        URD_CHECK(std::vector<int>(store.begin(), store.end()) == std::vector<int>(reference.begin(), reference.end()));
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

    // stable_refs: an element keeps its address however much is appended after it.
    {
        auto log = Store<value<int>, urd::stable_refs, urd::monotone>();
        log.insert(0);
        const int* first = &log[0];
        for (int element = 1; element < 100000; ++element)
            log.insert(element);
        URD_CHECK(&log[0] == first && *first == 0 && log.size() == 100000 && log[99999] == 99999);
    }

    // concurrent: inserts from several threads all arrive.
    {
        constexpr int THREADS = 4;
        constexpr int EACH = 20000;
        auto log = Store<value<int>, urd::monotone, urd::concurrent>();
        auto threads = std::vector<std::thread>();
        for (int thread = 0; thread < THREADS; ++thread)
            threads.emplace_back([&log, thread] {
                for (int element = 0; element < EACH; ++element)
                    log.insert(thread * EACH + element);
            });
        for (auto& thread : threads)
            thread.join();
        auto contents = std::vector<int>(log.begin(), log.end());
        std::ranges::sort(contents);
        URD_CHECK(contents.size() == THREADS * EACH && contents.front() == 0 && contents.back() == THREADS * EACH - 1);
        URD_CHECK(std::ranges::adjacent_find(contents) == contents.end());
    }

    // make and assume.
    const auto ids = Store<key<int>, urd::sorted>::make({ 3, 1, 3, 2 });
    URD_CHECK(std::vector<int>(ids.begin(), ids.end()) == std::vector<int>({ 1, 2, 3 }));
    const auto first = Store<value<int>, urd::sorted, urd::nonempty>::assume({ 4, 5 });
    URD_CHECK(first.front() == 4 && first[1] == 5);

    const auto frozen = Store<key<int>, urd::sorted, urd::frozen>::make({ 2, 3, 5 });
    URD_CHECK(frozen.contains(5) && !frozen.contains(4));
}
