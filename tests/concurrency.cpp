// What concurrent promises, per kind, as pairs of operations that may overlap (include/urd/tags.hpp): threads
// insert at once and agree; they read published elements while others insert; maps take updates of the same keys
// at once and reads beside them; pools hand equal rows equal handles across threads; a layered pool reads a parent
// no thread changes. Each test runs the pair from four threads. Run under ThreadSanitizer in CI, where a data race
// is the failure; here the results are checked.

#include "names.hpp"
#include "urd/elements/flat.hpp"
#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/parallel_hash_map.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/pools/pools.hpp"
#include "urd/repr/pools/tree_bitset.hpp"
#include "urd/repr/pools/tree_vector.hpp"
#include "urd/repr/records/arena.hpp"
#include "urd/repr/records/records.hpp"
#include "urd/repr/sequences/jagged.hpp"
#include "urd/repr/sequences/row_pool.hpp"
#include "urd/repr/sequences/segmented.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/parallel_hash_set.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/repr/wrappers/layer.hpp"
#include "urd/repr/wrappers/layered_pool.hpp"
#include "urd/store.hpp"
#include "urd/testing/inspect.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <numeric>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using urd::map;
using urd::pool;
using urd::sequence;
using urd::set;
using urd::slots;
using urd::Store;

namespace
{

constexpr int threads = 4;

/// Runs body(thread) on `threads` threads and joins them.
template<typename Body>
void in_threads(Body body)
{
    auto workers = std::vector<std::jthread>();
    for (int thread = 0; thread < threads; ++thread)
        workers.emplace_back([&body, thread] { body(thread); });
}

/// The keys 0..count-1, each thread in its own rotation, so that threads collide on every key.
int rotated(int thread, int step, int count) { return (step + thread * (count / threads)) % count; }

// Sets: insert‖insert agree; insert‖lookup of published elements.

template<typename S>
class ConcurrentSets : public testing::Test
{
};
using Sets = testing::Types<Store<set<int>, urd::positional, urd::monotone, urd::concurrent>,
                            urd::repr::ConcurrentIndexedSet<int>,
                            Store<set<int>, urd::concurrent>,
                            Store<set<int>, urd::stable_refs, urd::concurrent>>;
TYPED_TEST_SUITE(ConcurrentSets, Sets, urd::test::Names);

TYPED_TEST(ConcurrentSets, InsertsFromThreadsAgree)
{
    constexpr int count = 5000;
    auto set = urd::test::make_empty<TypeParam>();
    in_threads(
        [&set](int thread)
        {
            for (int step = 0; step < count; ++step)
                set.insert(rotated(thread, step, count));
        });
    EXPECT_EQ(set.size(), static_cast<std::size_t>(count));
    for (int element = 0; element < count; ++element)
        EXPECT_TRUE(set.contains(element)) << element;
}

TYPED_TEST(ConcurrentSets, LookupsOfPublishedElementsBesideInserts)
{
    constexpr int published = 2000;
    constexpr int count = 5000;
    auto set = urd::test::make_empty<TypeParam>();
    for (int element = 0; element < published; ++element)
        set.insert(element);
    auto misses = std::vector<int>(threads);
    in_threads(
        [&set, &misses](int thread)
        {
            if (thread % 2 == 0)
                for (int step = 0; step < count; ++step)
                    set.insert(published + rotated(thread, step, count));
            else
                for (int step = 0; step < count; ++step)
                    if (!set.contains(step % published))
                        ++misses[static_cast<std::size_t>(thread)];
        });
    EXPECT_EQ(misses, std::vector<int>(threads));
    EXPECT_EQ(set.size(), static_cast<std::size_t>(published + count));
}

// Sequences: push‖push, all arrive; push‖read of published positions.

TEST(ConcurrentSequences, PushesFromThreadsAllArrive)
{
    constexpr int count = 5000;
    auto sequence = Store<urd::sequence<int>, urd::monotone, urd::concurrent>();
    in_threads(
        [&sequence](int thread)
        {
            for (int step = 0; step < count; ++step)
                sequence.push_back(thread * count + step);
        });
    ASSERT_EQ(sequence.size(), static_cast<std::size_t>(threads * count));
    auto contents = std::vector<int>(sequence.begin(), sequence.end());
    std::ranges::sort(contents);
    auto expected = std::vector<int>(contents.size());
    std::iota(expected.begin(), expected.end(), 0);
    EXPECT_EQ(contents, expected);
}

TEST(ConcurrentSequences, ReadsOfPublishedPositionsBesidePushes)
{
    constexpr int published = 2000;
    constexpr int count = 5000;
    auto sequence = Store<urd::sequence<int>, urd::monotone, urd::concurrent>();
    for (int element = 0; element < published; ++element)
        sequence.push_back(element);
    auto wrong = std::vector<int>(threads);
    in_threads(
        [&sequence, &wrong](int thread)
        {
            if (thread % 2 == 0)
                for (int step = 0; step < count; ++step)
                    sequence.push_back(published + step);
            else
                for (int step = 0; step < count; ++step)
                    if (std::as_const(sequence)[static_cast<std::size_t>(step % published)] != step % published)
                        ++wrong[static_cast<std::size_t>(thread)];
        });
    EXPECT_EQ(wrong, std::vector<int>(threads));
}

// Maps: insert‖insert; insert‖find of published keys; update‖update of the same keys; update‖read of the same keys.

template<typename M>
class ConcurrentMaps : public testing::Test
{
};
using Maps = testing::Types<Store<map<int, int>, urd::dense, urd::constant_clear, urd::concurrent>,
                            Store<map<int, int>, urd::concurrent>,
                            Store<map<int, int>, urd::stable_refs, urd::concurrent>>;
TYPED_TEST_SUITE(ConcurrentMaps, Maps, urd::test::Names);

TYPED_TEST(ConcurrentMaps, InsertsFromThreadsAgree)
{
    constexpr int count = 5000;
    auto map = urd::test::make_empty<TypeParam>();
    in_threads(
        [&map](int thread)
        {
            for (int step = 0; step < count; ++step)
            {
                const int k = rotated(thread, step, count);
                map.insert(std::pair<const int, int>(k, -k));
            }
        });
    EXPECT_EQ(map.size(), static_cast<std::size_t>(count));
    for (int k = 0; k < count; ++k)
        EXPECT_EQ(map.visit(k, [](const int& v) { return v; }, 1), -k);
}

TYPED_TEST(ConcurrentMaps, LookupsOfPublishedKeysBesideInserts)
{
    constexpr int published = 2000;
    constexpr int count = 5000;
    auto map = urd::test::make_empty<TypeParam>();
    for (int k = 0; k < published; ++k)
        map.insert(std::pair<const int, int>(k, -k));
    auto misses = std::vector<int>(threads);
    in_threads(
        [&map, &misses](int thread)
        {
            if (thread % 2 == 0)
                for (int step = 0; step < count; ++step)
                {
                    const int k = published + rotated(thread, step, count);
                    map.insert(std::pair<const int, int>(k, -k));
                }
            else
                for (int step = 0; step < count; ++step)
                {
                    const int k = step % published;
                    if (!map.contains(k) || map.visit(k, [](const int& v) { return v; }, 1) != -k)
                        ++misses[static_cast<std::size_t>(thread)];
                }
        });
    EXPECT_EQ(misses, std::vector<int>(threads));
    EXPECT_EQ(map.size(), static_cast<std::size_t>(published + count));
}

TYPED_TEST(ConcurrentMaps, UpdatesOfTheSameKeysSum)
{
    constexpr int keys = 64;
    constexpr int rounds = 5120;  // a multiple of keys: every key is updated rounds / keys times per thread
    auto map = urd::test::make_empty<TypeParam>();
    in_threads(
        [&map](int thread)
        {
            for (int round = 0; round < rounds; ++round)
                map.update(rotated(thread, round, keys), [](int& counter, bool /*held*/) { ++counter; });
        });
    EXPECT_EQ(map.size(), static_cast<std::size_t>(keys));
    for (int k = 0; k < keys; ++k)
        EXPECT_EQ(map.visit(k, [](const int& v) { return v; }, -1), threads * rounds / keys) << k;
}

TYPED_TEST(ConcurrentMaps, ReadsOfTheSameKeysBesideUpdatesNeverGoBack)
{
    constexpr int keys = 64;
    constexpr int rounds = 5000;
    auto map = urd::test::make_empty<TypeParam>();
    for (int k = 0; k < keys; ++k)
        map.insert(std::pair<const int, int>(k, 0));
    auto regressions = std::vector<int>(threads);
    in_threads(
        [&map, &regressions](int thread)
        {
            if (thread % 2 == 0)
                for (int round = 0; round < rounds; ++round)
                    map.update(rotated(thread, round, keys), [](int& counter, bool /*held*/) { ++counter; });
            else
            {
                auto seen = std::vector<int>(keys, 0);
                for (int round = 0; round < rounds; ++round)
                {
                    const int k = round % keys;
                    const int now = map.visit(k, [](const int& v) { return v; }, -1);
                    if (now < seen[static_cast<std::size_t>(k)])
                        ++regressions[static_cast<std::size_t>(thread)];
                    seen[static_cast<std::size_t>(k)] = now;
                }
            }
        });
    EXPECT_EQ(regressions, std::vector<int>(threads));
}

// Pools: insert‖insert of equal rows gives equal handles; read by handle of published rows beside inserts.

template<typename P>
class ConcurrentRowPools : public testing::Test
{
};
using RowPools = testing::Types<Store<pool<Store<sequence<int>, urd::frozen>>, urd::concurrent>,
                                Store<pool<Store<sequence<int>, urd::frozen>>, urd::structural_sharing, urd::concurrent>,
                                Store<pool<Store<set<int>, urd::dense, urd::frozen>>, urd::structural_sharing, urd::concurrent>>;
TYPED_TEST_SUITE(ConcurrentRowPools, RowPools, urd::test::Names);

/// The row of `step`: a few small elements, so that equal rows recur across threads.
std::vector<int> row_of(int step)
{
    auto elements = std::vector<int>();
    for (int element = 0; element < 40; ++element)
        if ((element * 7 + step) % 5 < 2)
            elements.push_back(element + step % 3);
    return elements;
}

TYPED_TEST(ConcurrentRowPools, EqualRowsGetEqualHandlesAcrossThreads)
{
    constexpr int count = 2000;
    auto pool = urd::test::make_empty<TypeParam>();
    auto handles = std::vector<std::vector<typename TypeParam::handle_type>>(threads, std::vector<typename TypeParam::handle_type>(count));
    in_threads(
        [&pool, &handles](int thread)
        {
            for (int step = 0; step < count; ++step)
                handles[static_cast<std::size_t>(thread)][static_cast<std::size_t>(step)] = pool.insert(row_of(step)).first;
        });
    for (int thread = 1; thread < threads; ++thread)
        EXPECT_EQ(handles[static_cast<std::size_t>(thread)], handles[0]);
}

TYPED_TEST(ConcurrentRowPools, ReadsByHandleOfPublishedRowsBesideInserts)
{
    constexpr int published = 500;
    constexpr int count = 2000;
    using Row = typename TypeParam::value_type;
    auto pool = urd::test::make_empty<TypeParam>();
    auto early = std::vector<typename TypeParam::handle_type>();
    auto rows = std::vector<Row>();
    for (int step = 0; step < published; ++step)
    {
        const auto elements = row_of(step);
        early.push_back(pool.insert(elements).first);
        rows.emplace_back(elements.begin(), elements.end());
    }
    auto wrong = std::vector<int>(threads);
    in_threads(
        [&pool, &early, &rows, &wrong](int thread)
        {
            if (thread % 2 == 0)
                for (int step = published; step < published + count; ++step)
                    (void) pool.insert(row_of(step));
            else
                for (int step = 0; step < count; ++step)
                {
                    const auto at = static_cast<std::size_t>(step % published);
                    if (!(std::as_const(pool).get(early[at]) == rows[at]))
                        ++wrong[static_cast<std::size_t>(thread)];
                }
        });
    EXPECT_EQ(wrong, std::vector<int>(threads));
}

// Records stored flat: the same pairs, and a layered pool whose root no thread changes.

/// A trivially copyable record.
struct Point
{
    int x = 0;
    int y = 0;

    Point() = default;
    explicit Point(int drawn) : x(drawn % 50), y(drawn / 50) {}
    static constexpr auto identifying_members() noexcept { return urd::Members<&Point::x, &Point::y> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Point::x, &Point::y> {}; }
};
/// A record with a string, serialized into the arena.
struct Named
{
    urd::flat_string name;
    std::uint32_t arity = 0;

    Named() = default;
    explicit Named(int drawn) : name(std::string_view(std::string(static_cast<std::size_t>(drawn % 4), 'a'))), arity(static_cast<std::uint32_t>(drawn / 4)) {}
    static constexpr auto identifying_members() noexcept { return urd::Members<&Named::name, &Named::arity> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Named::name, &Named::arity> {}; }
};

template<typename P>
class ConcurrentRecordPools : public testing::Test
{
};
using RecordPools = testing::Types<Store<pool<Point>, urd::flat, urd::concurrent>,
                                   Store<pool<Named>, urd::flat, urd::concurrent>,
                                   Store<pool<Point>, urd::flat, urd::monotone, urd::layered, urd::concurrent>,
                                   Store<pool<Named>, urd::flat, urd::monotone, urd::layered, urd::concurrent>>;
TYPED_TEST_SUITE(ConcurrentRecordPools, RecordPools, urd::test::Names);

template<typename P>
constexpr bool layered = urd::has<P, urd::layered>;

TYPED_TEST(ConcurrentRecordPools, EqualRecordsGetEqualIndicesAcrossThreads)
{
    using T = typename TypeParam::value_type;
    using Index = typename TypeParam::handle_type;
    constexpr int count = 2000;
    constexpr int rooted = 8;
    // A layered pool works over a root of eight records that no thread changes; its indices continue the root's.
    auto root = urd::test::make_empty<TypeParam>(0);
    auto pool = urd::test::make_empty<TypeParam>(1);
    if constexpr (layered<TypeParam>)
    {
        for (int drawn = 0; drawn < rooted; ++drawn)
            root.insert(T(drawn));
        pool.layer_over(&root);
    }
    auto indices = std::vector<std::vector<Index>>(threads, std::vector<Index>(count));
    in_threads(
        [&pool, &indices](int thread)
        {
            for (int step = 0; step < count; ++step)
            {
                const int drawn = rotated(thread, step, count);
                // Every fourth by a factory, which builds the record with the index reserved for it.
                indices[static_cast<std::size_t>(thread)][static_cast<std::size_t>(step)] =
                    step % 4 == 3 ? pool.insert(T(drawn), [drawn](Index) { return T(drawn); }).first : pool.insert(T(drawn)).first;
                // A record published by the root, or by this thread just now, reads back beside the inserts.
                if (!urd::EqualTo<T> {}(std::as_const(pool)[indices[static_cast<std::size_t>(thread)][static_cast<std::size_t>(step)]], T(drawn)))
                    indices[static_cast<std::size_t>(thread)][static_cast<std::size_t>(step)] = Index();
            }
        });
    for (int step = 0; step < count; ++step)
    {
        // Every thread got the same index for the record of `step`, and that index gives it back.
        auto expected = Index();
        for (int thread = 0; thread < threads; ++thread)
        {
            // The step at which this thread inserted record `step`.
            const int own = (step - thread * (count / threads) + count) % count;
            const auto at = indices[static_cast<std::size_t>(thread)][static_cast<std::size_t>(own)];
            ASSERT_FALSE(at.is_none()) << "record " << step << " did not read back on thread " << thread;
            if (thread == 0)
                expected = at;
            else
                EXPECT_EQ(at, expected) << "record " << step;
        }
        EXPECT_TRUE(urd::EqualTo<T> {}(std::as_const(pool)[expected], T(step)));
    }
    EXPECT_EQ(pool.size(), static_cast<std::size_t>(count));
    if constexpr (layered<TypeParam>)
    {
        // The root's records kept their indices and the root its size.
        for (int drawn = 0; drawn < rooted; ++drawn)
            EXPECT_EQ(pool.find(T(drawn)), Index(static_cast<std::uint32_t>(drawn)));
        EXPECT_EQ(root.size(), static_cast<std::size_t>(rooted));
        EXPECT_FALSE((urd::testing::inspect::parent(pool) != nullptr && urd::testing::inspect::parent(pool)->size() > pool.base()));
    }
}

// Slots: a thread inserts, acquires, reads and erases its own handles while others do the same.

TEST(ConcurrentSlots, ThreadsWorkOnTheirOwnHandlesAtOnce)
{
    constexpr int rounds = 5000;
    auto pool = Store<slots<int>, urd::concurrent>();
    auto wrong = std::vector<int>(threads);
    in_threads(
        [&pool, &wrong](int thread)
        {
            auto mine = std::vector<typename std::remove_cvref_t<decltype(pool)>::handle_type>();
            for (int round = 0; round < rounds; ++round)
            {
                if (round % 4 == 3)
                {
                    const auto handle = pool.emplace();
                    pool[handle] = thread * rounds + round;
                    mine.push_back(handle);
                }
                else
                    mine.push_back(pool.insert(thread * rounds + round));
                if (round % 3 == 2)
                {
                    pool.erase(mine.front());
                    mine.erase(mine.begin());
                }
                for (const auto handle : mine)
                    if (std::as_const(pool)[handle] / rounds != thread)
                        ++wrong[static_cast<std::size_t>(thread)];
            }
        });
    EXPECT_EQ(wrong, std::vector<int>(threads));
}

// A layered dense map shared by threads (a concurrent relation repository): threads touch the same predicates at once;
// each predicate's own pool is made and layered over the parent's once, under its slot's lock, so every thread
// inserts into that one pool, equal rows get equal handles, and the handles continue the parent's.

struct ConcurrentPredicate;
using ConcurrentRow = Store<sequence<std::uint32_t>, urd::frozen>;
using ConcurrentRows = Store<pool<ConcurrentRow>, urd::monotone, urd::layered, urd::concurrent>;
using ConcurrentRelations = Store<map<urd::index<ConcurrentPredicate>, ConcurrentRows>, urd::dense, urd::layered, urd::concurrent>;
static_assert(
    std::same_as<ConcurrentRelations::representation, urd::repr::Layered<urd::repr::IndexMap<urd::index<ConcurrentPredicate>, ConcurrentRows, true, true>>>);

TEST(ConcurrentLayered, EachLayersValueIsMadeOnceAcrossThreads)
{
    constexpr int predicates = 16;
    constexpr int per_predicate = 200;
    constexpr int count = predicates * per_predicate;
    const auto predicate_of = [](int drawn) { return urd::index<ConcurrentPredicate>(static_cast<std::uint32_t>(drawn % predicates)); };
    // Every other predicate has one row in the domain, which no thread changes.
    auto domain = ConcurrentRelations();
    for (int drawn = 0; drawn < predicates; drawn += 2)
        domain[predicate_of(drawn)].insert({ 0U, 0U });
    auto task = ConcurrentRelations();
    task.layer_over(&domain);
    auto handles = std::vector<std::vector<urd::index<ConcurrentRow>>>(threads, std::vector<urd::index<ConcurrentRow>>(count));
    in_threads(
        [&](int thread)
        {
            for (int step = 0; step < count; ++step)
            {
                const int drawn = rotated(thread, step, count);
                const auto row = std::array<std::uint32_t, 2> { static_cast<std::uint32_t>(drawn / predicates), 1U };
                handles[static_cast<std::size_t>(thread)][static_cast<std::size_t>(drawn)] = task.at(predicate_of(drawn)).insert(row).first;
                // The domain's row, read through the task beside the inserts.
                if (drawn % 2 == 0 && std::as_const(task).at(predicate_of(drawn)).row_size(urd::index<ConcurrentRow>(0)) != 2)
                    handles[static_cast<std::size_t>(thread)][static_cast<std::size_t>(drawn)] = urd::index<ConcurrentRow>();
            }
        });
    EXPECT_EQ(task.size(), static_cast<std::size_t>(predicates));
    for (int drawn = 0; drawn < count; ++drawn)
        for (int thread = 1; thread < threads; ++thread)
            ASSERT_EQ(handles[static_cast<std::size_t>(thread)][static_cast<std::size_t>(drawn)], handles[0][static_cast<std::size_t>(drawn)])
                << "row " << drawn;
    for (int drawn = 0; drawn < predicates; ++drawn)
    {
        const auto& rows = std::as_const(task).at(predicate_of(drawn));
        const auto base = drawn % 2 == 0 ? 1U : 0U;
        EXPECT_EQ(rows.base(), base);
        EXPECT_EQ(rows.size(), base + per_predicate);
        EXPECT_EQ(&task.owner(predicate_of(drawn)), &task);
        EXPECT_FALSE((urd::testing::inspect::parent(rows) != nullptr && urd::testing::inspect::parent(rows)->size() > rows.base()));
    }
    for (int drawn = 0; drawn < predicates; drawn += 2)
        EXPECT_EQ(std::as_const(domain).at(predicate_of(drawn)).size(), 1U);
}

// Reads of a thread-safe generation map take no lock: a slot is published once its value is made (here, layered over
// the parent's pool, numbered after its row), so a reader that finds it present finds it whole.
TEST(ConcurrentLayered, ReadsFindMadeValuesWhole)
{
    constexpr std::uint32_t predicates = 512;
    const auto predicate_of = [](std::uint32_t at) { return urd::index<ConcurrentPredicate>(at); };
    auto domain = ConcurrentRelations();
    for (std::uint32_t at = 0; at < predicates; at += 2)
        domain[predicate_of(at)].insert({ 0U, 0U });
    auto task = ConcurrentRelations();
    task.layer_over(&domain);
    auto wrong = std::vector<int>(threads);
    in_threads(
        [&](int thread)
        {
            if (thread % 2 == 0)
                for (std::uint32_t at = 0; at < predicates; ++at)
                    (void) task.at(predicate_of(at)).insert({ at, 1U });
            else
                for (int round = 0; round < 20; ++round)
                    for (std::uint32_t at = 0; at < predicates; ++at)
                    {
                        const auto* own = std::as_const(task).own_value(predicate_of(at));
                        const auto base = own == nullptr ? -1 : static_cast<int>(own->base());
                        if (base != -1 && base != (at % 2 == 0 ? 1 : 0))
                            ++wrong[static_cast<std::size_t>(thread)];
                        // The domain's row, read through whichever layer holds the predicate.
                        if (at % 2 == 0 && !std::as_const(task).visit(predicate_of(at), [](const ConcurrentRows& rows) { return rows.size() > 0; }, false))
                            ++wrong[static_cast<std::size_t>(thread)];
                    }
        });
    EXPECT_EQ(wrong, std::vector<int>(threads));
    EXPECT_EQ(task.size(), static_cast<std::size_t>(predicates));
}

}  // namespace
