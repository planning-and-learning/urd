// Pools: rows identified by the handles insert gives. Each pool is checked against the rows it was given through
// random inserts and clears: equal rows get equal handles, and every handle gives its row back.

#include "names.hpp"
#include "urd/flat.hpp"
#include "urd/store.hpp"
#include "urd/view.hpp"
#include "verify.hpp"

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <gtest/gtest.h>
#include <random>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using urd::key;
using urd::Store;
using urd::value;

namespace
{

template<typename P>
class Pools : public testing::Test
{
};
using Containers = testing::Types<urd::repr::RowInterner<std::vector<int>>,
                                  Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique>,
                                  Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles, urd::unique>,
                                  Store<value<Store<key<int>, urd::sorted>>, urd::handles, urd::unique, urd::monotone>,
                                  urd::repr::TreeVectorSet<std::vector<int>>,
                                  urd::repr::TreeVectorSet<std::vector<double>, 2>,
                                  Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing>,
                                  Store<value<Store<value<double>, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing>,
                                  urd::repr::TreeBitsetSet<urd::repr::DenseSet<int>>,
                                  urd::repr::TreeBitsetSet<urd::repr::DenseSet<int>, 2>,
                                  Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing>,
                                  Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::concurrent>,
                                  Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing, urd::concurrent>,
                                  Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing, urd::concurrent>,
                                  urd::repr::LayeredPool<urd::repr::RowInterner<std::vector<int>>>,
                                  Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::monotone, urd::layered>,
                                  Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles, urd::unique, urd::layered, urd::concurrent>>;
TYPED_TEST_SUITE(Pools, Containers, urd::test::Names);

/// A trivially copyable record: its own block of bytes, interned in memory.
struct Point
{
    int x = 0;
    int y = 0;

    Point() = default;
    explicit Point(int drawn) : x(drawn % 5), y(drawn / 5) {}
    static constexpr auto identifying_members() noexcept { return urd::Members<&Point::x, &Point::y> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Point::x, &Point::y> {}; }
};
/// A record with a string: serialized by cista into the arena.
struct Named
{
    urd::flat_string name;
    std::uint32_t arity = 0;

    Named() = default;
    explicit Named(int drawn) : name(std::string_view(std::string(static_cast<std::size_t>(drawn % 4), 'a'))), arity(static_cast<std::uint32_t>(drawn / 4)) {}
    static constexpr auto identifying_members() noexcept { return urd::Members<&Named::name, &Named::arity> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Named::name, &Named::arity> {}; }
};
/// A trivially copyable record without cista_members: only the in-memory interners store it.
struct Plain
{
    int x = 0;

    Plain() = default;
    explicit Plain(int drawn) : x(drawn) {}
    static constexpr auto identifying_members() noexcept { return urd::Members<&Plain::x> {}; }
};
static_assert(std::is_trivially_copyable_v<Point> && !std::is_trivially_copyable_v<Named>);

template<typename P>
class Records : public testing::Test
{
};
using RecordPools = testing::Types<urd::repr::RecordInterner<Point>,
                                   urd::repr::RecordInterner<Point, false, true>,
                                   urd::repr::RecordInterner<Point, true>,
                                   urd::repr::ArenaInterner<Named>,
                                   urd::repr::ArenaInterner<Named, true>,
                                   urd::repr::ArenaInterner<Point>,
                                   Store<value<Point>, urd::handles, urd::unique, urd::flat>,
                                   Store<value<Point>, urd::handles, urd::unique, urd::flat, urd::monotone>,
                                   Store<value<Point>, urd::handles, urd::unique, urd::flat, urd::concurrent>,
                                   Store<value<Plain>, urd::handles, urd::unique, urd::flat, urd::stable_refs>,
                                   Store<value<Named>, urd::handles, urd::unique, urd::flat>,
                                   Store<value<Named>, urd::handles, urd::unique, urd::flat, urd::monotone, urd::concurrent>,
                                   urd::repr::LayeredPool<urd::repr::RecordInterner<Point>>,
                                   urd::repr::LayeredPool<urd::repr::RecordInterner<Point, false, true>>,
                                   urd::repr::LayeredPool<urd::repr::ArenaInterner<Named, true>>,
                                   Store<value<Plain>, urd::handles, urd::unique, urd::flat, urd::stable_refs, urd::layered>,
                                   Store<value<Point>, urd::handles, urd::unique, urd::flat, urd::monotone, urd::layered>,
                                   Store<value<Named>, urd::handles, urd::unique, urd::flat, urd::layered, urd::concurrent>>;
TYPED_TEST_SUITE(Records, RecordPools, urd::test::Names);

}  // namespace

// Records stored flat: trivially copyable ones in memory, the others serialized into an arena; thread-safe on request.
static_assert(std::same_as<Store<value<Point>, urd::handles, urd::unique, urd::flat>::representation, urd::repr::RecordInterner<Point>>);
static_assert(std::same_as<Store<value<Point>, urd::handles, urd::unique, urd::flat, urd::concurrent>::representation, urd::repr::RecordInterner<Point, true>>);
// Records that never move, without the lock: the interner over segments; a record cista can serialize takes the
// arena, which never moves a block either.
static_assert(
    std::same_as<Store<value<Plain>, urd::handles, urd::unique, urd::flat, urd::stable_refs>::representation, urd::repr::RecordInterner<Plain, false, true>>);
static_assert(std::same_as<Store<value<Point>, urd::handles, urd::unique, urd::flat, urd::stable_refs>::representation, urd::repr::ArenaInterner<Point>>);
static_assert(std::same_as<Store<value<Plain>, urd::handles, urd::unique, urd::flat, urd::stable_refs, urd::layered>::representation,
                           urd::repr::LayeredPool<urd::repr::RecordInterner<Plain, false, true>>>);
static_assert(std::same_as<Store<value<Named>, urd::handles, urd::unique, urd::flat>::representation, urd::repr::ArenaInterner<Named>>);
static_assert(std::same_as<Store<value<Named>, urd::handles, urd::unique, urd::flat, urd::concurrent>::representation, urd::repr::ArenaInterner<Named, true>>);
// Layered pools: the same interners over a parent, indices continuing after the parent's.
static_assert(std::same_as<Store<value<Point>, urd::handles, urd::unique, urd::flat, urd::monotone, urd::layered>::representation,
                           urd::repr::LayeredPool<urd::repr::RecordInterner<Point>>>);
static_assert(std::same_as<Store<value<Named>, urd::handles, urd::unique, urd::flat, urd::layered, urd::concurrent>::representation,
                           urd::repr::LayeredPool<urd::repr::ArenaInterner<Named, true>>>);
static_assert(std::same_as<Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::layered>::representation,
                           urd::repr::LayeredPool<urd::repr::RowInterner<Store<value<int>, urd::frozen>>>>);
// A flat pool is counted and read by index, as its records are whole; its handle is the index.
static_assert(std::same_as<Store<value<Named>, urd::handles, urd::unique, urd::flat>::handle_type, urd::index<Named>>);

TYPED_TEST(Records, OffersWhatItsPropertiesRequire) { static_assert(urd::conforming<TypeParam>); }

TYPED_TEST(Records, KeepsEachDistinctRecordOnceThroughRandomOperations)
{
    for (unsigned seed = 0; seed < 32; ++seed)
    {
        auto pool = TypeParam();
        const auto found = urd::verify(std::move(pool), seed, 300);
        EXPECT_FALSE(found) << found.value_or("");
    }
}

// Without structural_sharing, each row is stored once, in full.
static_assert(std::same_as<Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique>::representation,
                           urd::repr::RowInterner<Store<value<int>, urd::frozen>>>);
// With it, sequence rows are trees whose equal subtrees are stored once.
static_assert(std::same_as<Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing>::representation,
                           urd::repr::TreeVectorSet<Store<value<int>, urd::frozen>>>);
// Rows that are sets of indices are bitset trees, with zero subtrees left out.
static_assert(std::same_as<Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing>::representation,
                           urd::repr::TreeBitsetSet<Store<key<int>, urd::dense, urd::frozen>>>);
// Thread-safe where asked: the same pools over concurrent tables.
static_assert(std::same_as<Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::concurrent>::representation,
                           urd::repr::RowInterner<Store<value<int>, urd::frozen>, true>>);
static_assert(
    std::same_as<Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing, urd::concurrent>::representation,
                 urd::repr::TreeBitsetSet<Store<key<int>, urd::dense, urd::frozen>, 4, true>>);
// A pool is neither iterated nor counted.
template<typename P>
concept iterable = requires(P pool) { pool.begin(); };
template<typename P>
concept sized = requires(P pool) { pool.size(); };
static_assert(!iterable<Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique>>
              && !sized<Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique>>);

TYPED_TEST(Pools, OffersWhatItsPropertiesRequire) { static_assert(urd::conforming<TypeParam>); }

TYPED_TEST(Pools, GivesEqualRowsEqualHandlesThroughRandomOperations)
{
    for (unsigned seed = 0; seed < 32; ++seed)
    {
        const auto found = urd::verify(TypeParam(), seed, 300);
        EXPECT_FALSE(found) << found.value_or("");
    }
}

// A set row is equal whatever order its elements came in.
TEST(Pools, SetRowsAreEqualInAnyOrder)
{
    auto facts = Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles, urd::unique>();
    const auto first = facts.insert({ 3, 1, 4 });
    const auto second = facts.insert(std::vector<int> { 4, 3, 1, 1 });
    EXPECT_EQ(first, second);
    EXPECT_NE(first, facts.insert({ 3, 1 }));
    EXPECT_EQ(facts.row_size(first), 3U);
    EXPECT_TRUE(facts.get(first).contains(4));
}

// A row written in k positions of n adds at most k nodes per level of the tree: the paths to the root, no more.
TEST(Pools, WritesShareAllButTheirPaths)
{
    auto random = std::mt19937(31);
    for (const std::uint32_t size : { 1U, 2U, 5U, 64U, 1000U })
    {
        auto pool = urd::repr::TreeVectorSet<std::vector<std::uint32_t>>();
        auto values = std::vector<std::uint32_t>(size);
        std::ranges::generate(values, [&random] { return static_cast<std::uint32_t>(random() % 1000); });
        const auto base = pool.insert(values);
        const auto levels = (static_cast<std::size_t>(std::bit_width(size)) + 1) / 2 + 1;  // ⌈log₄ size⌉ + 1, generously
        for (int round = 0; round < 20; ++round)
        {
            auto writes = std::vector<urd::Write<std::uint32_t>>();
            for (std::uint32_t position = 0; position < size; ++position)
                if (random() % 50 == 0)
                    writes.push_back({ position, static_cast<std::uint32_t>(random()) });
            const auto before = pool.num_nodes();
            const auto written = pool.insert(base, writes);
            EXPECT_LE(pool.num_nodes() - before, writes.size() * levels);
            auto expected = values;
            for (const auto& write : writes)
                expected[write.position] = write.value;
            EXPECT_EQ(written, pool.insert(expected));
        }
    }
}

namespace
{

template<typename P>
class Slots : public testing::Test
{
};
using SlotContainers = testing::
    Types<urd::repr::SlotVector<int>, urd::repr::SlotVector<int, true>, Store<value<int>, urd::handles>, Store<value<int>, urd::handles, urd::concurrent>>;
TYPED_TEST_SUITE(Slots, SlotContainers, urd::test::Names);

}  // namespace

// Without unique, a pool hands out a new slot per insert: a SlotVector, thread-safe where asked.
static_assert(std::same_as<Store<value<int>, urd::handles>::representation, urd::repr::SlotVector<int>>);
static_assert(std::same_as<Store<value<int>, urd::handles, urd::concurrent>::representation, urd::repr::SlotVector<int, true>>);
static_assert(urd::has<Store<value<int>, urd::handles, urd::stable_refs>, urd::stable_refs>);

TYPED_TEST(Slots, OffersWhatItsPropertiesRequire) { static_assert(urd::conforming<TypeParam>); }

TYPED_TEST(Slots, KeepsElementsInPlaceAndReusesFreedSlots)
{
    for (unsigned seed = 0; seed < 32; ++seed)
    {
        const auto found = urd::verify(TypeParam(), seed, 1000);
        EXPECT_FALSE(found) << found.value_or("");
    }
}

// A scratch buffer per task: erased and acquired again, it keeps its storage, so reuse allocates nothing.
TEST(Slots, AcquireReusesAnErasedElementAsItIs)
{
    auto buffers = Store<value<std::vector<int>>, urd::handles>();
    const auto first = buffers.acquire();
    buffers[first].assign(1000, 7);
    buffers.erase(first);
    const auto again = buffers.acquire();
    EXPECT_EQ(again, first);
    EXPECT_GE(buffers[again].capacity(), 1000U);
    buffers[again].clear();
    EXPECT_EQ(buffers.size(), 1U);
}

// Threads insert, write, read and erase their own elements at once; each finds its own values throughout.
TEST(Slots, ThreadSafeSlotsServeThreadsAtOnce)
{
    auto pool = Store<value<int>, urd::handles, urd::concurrent>();
    constexpr int threads = 4;
    constexpr int rounds = 2000;
    auto failures = std::vector<int>(threads);
    {
        auto workers = std::vector<std::jthread>();
        for (int thread = 0; thread < threads; ++thread)
            workers.emplace_back(
                [&pool, &failures, thread]
                {
                    auto mine = std::vector<typename std::remove_cvref_t<decltype(pool)>::handle_type>();
                    for (int round = 0; round < rounds; ++round)
                    {
                        mine.push_back(pool.insert(thread * rounds + round));
                        if (round % 3 == 2)
                        {
                            pool.erase(mine.front());
                            mine.erase(mine.begin());
                        }
                    }
                    for (const auto handle : mine)
                        if (std::as_const(pool)[handle] / rounds != thread)
                            ++failures[static_cast<std::size_t>(thread)];
                });
    }
    EXPECT_EQ(failures, std::vector<int>(threads));
    EXPECT_EQ(pool.size(), static_cast<std::size_t>(threads * (rounds - rounds / 3)));
}

// Threads inserting the same rows, and rows derived from them, into a shared tree pool at once: every thread gets
// the same handles, and every row reads back. Run under ThreadSanitizer in CI.
namespace
{

template<typename P>
void expect_threads_agree(int rounds)
{
    auto pool = P();
    constexpr int threads = 4;
    auto handles = std::vector<std::vector<typename P::handle_type>>(threads);
    {
        auto workers = std::vector<std::jthread>();
        for (int thread = 0; thread < threads; ++thread)
            workers.emplace_back(
                [&pool, &handles, rounds, thread]
                {
                    auto& mine = handles[static_cast<std::size_t>(thread)];
                    for (int round = 0; round < rounds; ++round)
                    {
                        auto elements = std::vector<int>();
                        for (int element = 0; element < 40; ++element)
                            if ((element * 7 + round) % 5 < 2)
                                elements.push_back(element + round % 3);
                        mine.push_back(pool.insert(elements));
                        // Read a row published earlier, while others insert.
                        if (round > 0)
                            (void) pool.get(mine[static_cast<std::size_t>(round / 2)]);
                    }
                });
    }
    for (int thread = 1; thread < threads; ++thread)
        EXPECT_EQ(handles[static_cast<std::size_t>(thread)], handles[0]);
}

}  // namespace

TEST(Pools, ThreadsAgreeOnHandles)
{
    expect_threads_agree<Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::concurrent>>(300);
    expect_threads_agree<Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing, urd::concurrent>>(300);
    expect_threads_agree<Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing, urd::concurrent>>(300);
}

// A pool that keeps rows in full lends them out: view(h) is the stored row, a span of its elements or, for rows of
// dense indices, their bits; views taken early still show their rows after many more inserts.
TEST(Pools, ViewsLendTheStoredRows)
{
    auto random = std::mt19937(53);
    const auto row = [&random]
    {
        auto elements = std::vector<int>(std::uniform_int_distribution<std::size_t>(0, 20)(random));
        for (auto& each : elements)
            each = std::uniform_int_distribution<int>(0, 300)(random);
        return elements;
    };
    {
        auto pool = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique>();
        auto handles = std::vector<decltype(pool)::handle_type>();
        auto views = std::vector<std::span<const int>>();
        for (int each = 0; each < 2000; ++each)
        {
            handles.push_back(pool.insert(row()));
            views.push_back(pool.view(handles.back()));
        }
        for (std::size_t each = 0; each < handles.size(); ++each)
        {
            const auto held = pool.get(handles[each]);
            ASSERT_TRUE(std::ranges::equal(views[each], held)) << each;
        }
    }
    {
        using Set = Store<key<int>, urd::dense, urd::frozen>;
        auto pool = Store<value<Set>, urd::handles, urd::unique>();
        static_assert(std::same_as<decltype(pool)::representation, urd::repr::RowInterner<Set>>);
        auto handles = std::vector<decltype(pool)::handle_type>();
        auto views = std::vector<decltype(pool.view(handles.front()))>();
        for (int each = 0; each < 2000; ++each)
        {
            handles.push_back(pool.insert(row()));
            views.push_back(pool.view(handles.back()));
        }
        for (std::size_t each = 0; each < handles.size(); ++each)
        {
            const auto held = pool.get(handles[each]);
            auto bits = std::vector<int>();
            const auto& view = views[each];
            for (auto at = view.find_first(); at != std::remove_cvref_t<decltype(view)>::npos; at = view.find_next(at))
                bits.push_back(static_cast<int>(at));
            ASSERT_TRUE(std::ranges::equal(bits, held)) << each;
        }
        // Equal sets, whatever their elements' order or repeats, are one row.
        EXPECT_EQ(pool.insert(std::vector<int> { 5, 3, 3, 200 }), pool.insert(std::vector<int> { 200, 5, 3 }));
    }
}

namespace
{

template<typename P>
void expect_threads_agree_on_records(int rounds)
{
    auto pool = P();
    constexpr int threads = 4;
    auto indices = std::vector<std::vector<typename P::handle_type>>(threads);
    {
        auto workers = std::vector<std::jthread>();
        for (int thread = 0; thread < threads; ++thread)
            workers.emplace_back(
                [&pool, &indices, rounds, thread]
                {
                    auto& mine = indices[static_cast<std::size_t>(thread)];
                    for (int round = 0; round < rounds; ++round)
                    {
                        const auto record = typename P::record_type((round * 7) % 40);
                        if (round % 4 == 3)
                            mine.push_back(pool.insert(record, [&record](typename P::handle_type) { return record; }).first);
                        else
                            mine.push_back(pool.insert(record).first);
                        // Read a record published earlier, while others insert.
                        if (round > 0)
                            (void) pool[mine[static_cast<std::size_t>(round / 2)]];
                    }
                });
    }
    for (int thread = 1; thread < threads; ++thread)
        EXPECT_EQ(indices[static_cast<std::size_t>(thread)], indices[0]);
}

/// A record whose members are what views resolve: text, indices, a variant.
struct Owner
{
    urd::flat_string name;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Owner::name> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Owner::name> {}; }
};
struct Item
{
    urd::flat_string name;
    urd::flat_vector<urd::index<Owner>> owners;
    urd::flat_variant<urd::index<Owner>, int> alternative;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Item::name, &Item::owners, &Item::alternative> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Item::name, &Item::owners, &Item::alternative> {}; }
};
using Owners = Store<value<Owner>, urd::handles, urd::unique, urd::flat, urd::monotone>;
using Items = Store<value<Item>, urd::handles, urd::unique, urd::flat, urd::monotone>;
struct Shop
{
    Owners owners;
    Items items;

    [[nodiscard]] const Owner& operator[](urd::index<Owner> at) const { return owners[at]; }
    [[nodiscard]] const Item& operator[](urd::index<Item> at) const { return items[at]; }
};

}  // namespace

TEST(Records, ThreadsAgreeOnIndices)
{
    expect_threads_agree_on_records<Store<value<Point>, urd::handles, urd::unique, urd::flat, urd::concurrent>>(300);
    expect_threads_agree_on_records<Store<value<Named>, urd::handles, urd::unique, urd::flat, urd::concurrent>>(300);
}

TEST(Records, ViewsResolveFlatMembers)
{
    auto shop = Shop();
    auto owner = Owner();
    owner.name = urd::flat_string(std::string_view("ann"));
    const auto ann = shop.owners.insert(owner).first;
    auto item = Item();
    item.name = urd::flat_string(std::string_view("lamp"));
    item.owners.push_back(ann);
    item.alternative = ann;
    const auto lamp = shop.items.insert(item).first;
    EXPECT_EQ(shop.items.insert(item).first, lamp);
    EXPECT_EQ(shop.items.find(item), lamp);
    EXPECT_TRUE(shop.items.contains(lamp));

    const auto alone = shop.items.view(lamp);  // the Store resolves items only
    EXPECT_EQ(alone.get<&Item::name>(), "lamp");
    EXPECT_EQ(alone.get<&Item::owners>()[0], ann);
    EXPECT_EQ(std::get<urd::index<Owner>>(alone.get<&Item::alternative>()), ann);

    const auto whole = urd::view<Item, Shop>(lamp, shop);  // the shop resolves owners too
    EXPECT_EQ(whole.get<&Item::owners>()[0].get<&Owner::name>(), "ann");
    EXPECT_EQ((std::get<urd::view<Owner, Shop>>(whole.get<&Item::alternative>()).get<&Owner::name>()), "ann");
}

TEST(Layered, RowPoolsContinueTheirParent)
{
    using Row = Store<value<int>, urd::frozen>;
    using RowIndex = urd::index<Row>;
    using Rows = Store<value<Row>, urd::handles, urd::unique, urd::monotone, urd::layered>;
    auto parent = Rows();
    const auto first = parent.insert({ 1, 2, 3 });
    const auto second = parent.insert({ 4 });
    auto child = Rows();
    child.layer_over(&parent);
    EXPECT_EQ(child.base(), 2U);
    EXPECT_EQ(child.layer(), 1U);
    EXPECT_EQ(child.insert({ 1, 2, 3 }), first);  // the parent's row, found through the chain
    const auto third = child.insert({ 5, 6 });
    EXPECT_EQ(third, RowIndex(2));
    const auto expected_first = Row { 1, 2, 3 };
    const auto expected_third = Row { 5, 6 };
    EXPECT_EQ(child.get(first), expected_first);
    EXPECT_EQ(child.get(third), expected_third);
    EXPECT_EQ(child.row_size(second), 1U);
    EXPECT_EQ(child.view(first).size(), 3U);
    const auto writes = std::vector<urd::Write<int>> { { 1, 9 } };
    const auto written = child.insert(first, std::span<const urd::Write<int>>(writes));
    EXPECT_EQ(written, RowIndex(3));
    const auto expected_written = Row { 1, 9, 3 };
    EXPECT_EQ(child.get(written), expected_written);
    EXPECT_EQ(parent.insert({ 1, 9, 3 }), RowIndex(2));  // the parent knows nothing of it
    EXPECT_TRUE(child.parent_grew());
    EXPECT_EQ(child.parent(), &parent);
}

TEST(Layered, ViewsPointAtTheOwningLayer)
{
    using Points = Store<value<Point>, urd::handles, urd::unique, urd::flat, urd::layered>;
    using PointView = urd::view<Point, Points>;
    auto root = Points();
    const auto one = root.insert(Point(1)).first;
    auto child = Points();
    child.layer_over(&root);
    const auto two = child.insert(Point(2)).first;
    EXPECT_EQ(child.insert(Point(1)).first, one);
    EXPECT_EQ(&child.owner(one), &root);
    EXPECT_EQ(&child.owner(two), &child);
    EXPECT_EQ(child.view(one), root.view(one));  // the same element, whichever layer it was asked of
    EXPECT_EQ(urd::Hash<PointView> {}(child.view(one)), urd::Hash<PointView> {}(root.view(one)));
    EXPECT_NE(child.view(two), root.view(one));
    EXPECT_EQ(child.view(one).get<&Point::x>(), 1);
    EXPECT_EQ(child.view(two).context().layer(), 1U);
    EXPECT_EQ(child.view(one).context().layer(), 0U);
    EXPECT_FALSE(child.contains(urd::index<Point>(2)));
    child.clear();
    EXPECT_EQ(child.size(), 1U);
    EXPECT_FALSE(child.contains(Point(2)));
    EXPECT_TRUE(child.contains(Point(1)));
}
