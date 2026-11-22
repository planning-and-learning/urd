// Pools: rows identified by the handles insert gives. Each pool is checked against the rows it was given through
// random inserts and clears: equal rows get equal handles, and every handle gives its row back.

#include "urd/repr/pools/pools.hpp"

#include "names.hpp"
#include "urd/elements/children.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/keyed.hpp"
#include "urd/elements/view.hpp"
#include "urd/format.hpp"
#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
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
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/values/std_set.hpp"
#include "urd/repr/wrappers/layer.hpp"
#include "urd/repr/wrappers/layered_pool.hpp"
#include "urd/repr/wrappers/multi_index_pool.hpp"
#include "urd/repr/wrappers/recycling.hpp"
#include "urd/store.hpp"
#include "urd/testing/inspect.hpp"
#include "verify.hpp"

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <gtest/gtest.h>
#include <random>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

using urd::pool;
using urd::sequence;
using urd::set;
using urd::slots;
using urd::Store;

namespace
{

template<typename P>
class Pools : public testing::Test
{
};

using Containers = testing::Types<urd::repr::RowInterner<std::vector<int>>,
                                  Store<pool<Store<sequence<int>, urd::frozen>>>,
                                  Store<pool<Store<set<int>, urd::dense, urd::frozen>>>,
                                  Store<pool<Store<set<int>, urd::sorted>>, urd::monotone>,
                                  urd::repr::TreeVectorSet<std::vector<int>>,
                                  urd::repr::TreeVectorSet<std::vector<double>, 2>,
                                  Store<pool<Store<sequence<int>, urd::frozen>>, urd::structural_sharing>,
                                  Store<pool<Store<sequence<double>, urd::frozen>>, urd::structural_sharing>,
                                  urd::repr::TreeBitsetSet<urd::repr::DenseSet<int>>,
                                  urd::repr::TreeBitsetSet<urd::repr::DenseSet<int>, 2>,
                                  Store<pool<Store<set<int>, urd::dense, urd::frozen>>, urd::structural_sharing>,
                                  Store<pool<Store<sequence<int>, urd::frozen>>, urd::concurrent>,
                                  Store<pool<Store<sequence<int>, urd::frozen>>, urd::structural_sharing, urd::concurrent>,
                                  Store<pool<Store<set<int>, urd::dense, urd::frozen>>, urd::structural_sharing, urd::concurrent>,
                                  urd::repr::LayeredPool<urd::repr::RowInterner<std::vector<int>>>,
                                  Store<pool<Store<sequence<int>, urd::frozen>>, urd::monotone, urd::layered>,
                                  Store<pool<Store<set<int>, urd::dense, urd::frozen>>, urd::layered, urd::concurrent>>;
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
                                   urd::repr::ValueInterner<Point>,
                                   urd::repr::ValueInterner<Named, false, true>,
                                   urd::repr::ValueInterner<Named, true>,
                                   Store<pool<Point>>,
                                   Store<pool<Named>, urd::concurrent>,
                                   urd::repr::ArenaInterner<Named>,
                                   urd::repr::ArenaInterner<Named, true>,
                                   urd::repr::ArenaInterner<Point>,
                                   Store<pool<Point>, urd::flat>,
                                   Store<pool<Point>, urd::flat, urd::monotone>,
                                   Store<pool<Point>, urd::flat, urd::concurrent>,
                                   Store<pool<Plain>, urd::flat, urd::stable_refs>,
                                   Store<pool<Named>, urd::flat>,
                                   Store<pool<Named>, urd::flat, urd::monotone, urd::concurrent>,
                                   urd::repr::LayeredPool<urd::repr::RecordInterner<Point, false, true>>,
                                   urd::repr::LayeredPool<urd::repr::ArenaInterner<Named, true>>,
                                   Store<pool<Plain>, urd::flat, urd::stable_refs, urd::layered>,
                                   Store<pool<Point>, urd::flat, urd::monotone, urd::layered>,
                                   Store<pool<Named>, urd::flat, urd::layered, urd::concurrent>>;
TYPED_TEST_SUITE(Records, RecordPools, urd::test::Names);

}  // namespace

// Values interned whole, where the request does not ask for flat: the value interner, whatever the value.
static_assert(std::same_as<Store<pool<Point>>::representation, urd::repr::Numbered<urd::repr::ValueInterner<Point>>>);
static_assert(std::same_as<Store<pool<Named>, urd::stable_refs>::representation, urd::repr::Numbered<urd::repr::ValueInterner<Named, false, true>>>);
static_assert(std::same_as<Store<pool<std::string>>::representation, urd::repr::Numbered<urd::repr::ValueInterner<std::string>>>);
// Records stored flat: trivially copyable ones in memory, the others serialized into an arena; thread-safe on request.
static_assert(std::same_as<Store<pool<Point>, urd::flat>::representation, urd::repr::Numbered<urd::repr::RecordInterner<Point>>>);
static_assert(std::same_as<Store<pool<Point>, urd::flat, urd::concurrent>::representation, urd::repr::Numbered<urd::repr::RecordInterner<Point, true>>>);
// Records that never move, without the lock: the interner over segments; a record cista can serialize takes the
// arena, which never moves a block either.
static_assert(
    std::same_as<Store<pool<Plain>, urd::flat, urd::stable_refs>::representation, urd::repr::Numbered<urd::repr::RecordInterner<Plain, false, true>>>);
static_assert(std::same_as<Store<pool<Point>, urd::flat, urd::stable_refs>::representation, urd::repr::Numbered<urd::repr::ArenaInterner<Point>>>);
static_assert(std::same_as<Store<pool<Plain>, urd::flat, urd::stable_refs, urd::layered>::representation,
                           urd::repr::Numbered<urd::repr::LayeredPool<urd::repr::RecordInterner<Plain, false, true>>>>);
static_assert(std::same_as<Store<pool<Named>, urd::flat>::representation, urd::repr::Numbered<urd::repr::ArenaInterner<Named>>>);
static_assert(std::same_as<Store<pool<Named>, urd::flat, urd::concurrent>::representation, urd::repr::Numbered<urd::repr::ArenaInterner<Named, true>>>);
// Layered pools: the interners that never move a record, over a parent, indices continuing after the parent's.
static_assert(std::same_as<Store<pool<Point>, urd::flat, urd::monotone, urd::layered>::representation,
                           urd::repr::Numbered<urd::repr::LayeredPool<urd::repr::ArenaInterner<Point>>>>);
static_assert(std::same_as<Store<pool<Named>, urd::flat, urd::layered, urd::concurrent>::representation,
                           urd::repr::Numbered<urd::repr::LayeredPool<urd::repr::ArenaInterner<Named, true>>>>);
static_assert(std::same_as<Store<pool<Store<sequence<int>, urd::frozen>>, urd::layered>::representation,
                           urd::repr::LayeredPool<urd::repr::RowInterner<Store<sequence<int>, urd::frozen>>>>);
// A flat pool is counted and read by index, as its records are whole; its handle is the index.
static_assert(std::same_as<Store<pool<Named>, urd::flat>::handle_type, urd::index<Named>>);

TYPED_TEST(Records, KeepsEachDistinctRecordOnceThroughRandomOperations)
{
    for (unsigned seed = 0; seed < 32; ++seed)
    {
        auto pool = urd::test::make_empty<TypeParam>();
        const auto found = urd::verify(std::move(pool), seed, 300);
        EXPECT_FALSE(found) << found.value_or("");
    }
}

// Without structural_sharing, each row is stored once, in full.
static_assert(std::same_as<Store<pool<Store<sequence<int>, urd::frozen>>>::representation, urd::repr::RowInterner<Store<sequence<int>, urd::frozen>>>);
// With it, sequence rows are trees whose equal subtrees are stored once.
static_assert(std::same_as<Store<pool<Store<sequence<int>, urd::frozen>>, urd::structural_sharing>::representation,
                           urd::repr::TreeVectorSet<Store<sequence<int>, urd::frozen>>>);
// Rows that are sets of indices are bitset trees, with zero subtrees left out.
static_assert(std::same_as<Store<pool<Store<set<int>, urd::dense, urd::frozen>>, urd::structural_sharing>::representation,
                           urd::repr::TreeBitsetSet<Store<set<int>, urd::dense, urd::frozen>>>);
// Thread-safe where asked: the same pools over concurrent tables.
static_assert(std::same_as<Store<pool<Store<sequence<int>, urd::frozen>>, urd::concurrent>::representation,
                           urd::repr::RowInterner<Store<sequence<int>, urd::frozen>, true>>);
static_assert(std::same_as<Store<pool<Store<set<int>, urd::dense, urd::frozen>>, urd::structural_sharing, urd::concurrent>::representation,
                           urd::repr::TreeBitsetSet<Store<set<int>, urd::dense, urd::frozen>, 4, true>>);
// A pool is not iterated: it is reached by handle. It is counted, its handles 0..size()-1, unless it shares nodes
// between rows (structural_sharing), where handles name nodes and no list of rows is kept.
template<typename P>
concept iterable = requires(P pool) { pool.begin(); };
template<typename P>
concept sized = requires(P pool) { pool.size(); };
static_assert(!iterable<Store<pool<Store<sequence<int>, urd::frozen>>>> && sized<Store<pool<Store<sequence<int>, urd::frozen>>>>
              && !sized<Store<pool<Store<sequence<int>, urd::frozen>>, urd::structural_sharing>>);

// Erase moves the last row into the erased one's handle and says which handle it had, so whatever keeps handles
// (postings over the rows, yggdrasil's swap-erase) follows it.
TEST(Pools, EraseMovesTheLastRowIn)
{
    auto rows = Store<pool<Store<sequence<int>, urd::frozen>>>();
    const auto a = rows.insert({ 1 }).first;
    const auto b = rows.insert({ 2 }).first;
    const auto c = rows.insert({ 3 }).first;
    EXPECT_EQ(rows.erase(a), c);  // {3} moves into a's handle
    EXPECT_EQ(rows.size(), 2U);
    EXPECT_TRUE(std::ranges::equal(rows.get(a), std::vector { 3 }));
    EXPECT_EQ(rows.find({ 3 }), a);
    EXPECT_TRUE(is_none(rows.find({ 1 })));
    EXPECT_TRUE(is_none(rows.erase(b)));  // the last row: nothing moves
    EXPECT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows.insert({ 2 }), std::pair(b, true));  // stored again, at the next handle
}

template<typename P>
concept erasable = requires(P pool, typename P::handle_type handle) { pool.erase(handle); };
static_assert(erasable<Store<pool<Store<sequence<int>, urd::frozen>>>> && !erasable<Store<pool<Store<sequence<int>, urd::frozen>>, urd::monotone>>
              && !erasable<Store<pool<Store<sequence<int>, urd::frozen>>, urd::structural_sharing>>
              && !erasable<Store<pool<Store<sequence<int>, urd::frozen>>, urd::concurrent>>
              && !erasable<Store<pool<Store<sequence<int>, urd::frozen>>, urd::layered>>);

// A pool that shares nodes finds what it can build from them: the rows inserted, and their parts.
TEST(Pools, FindWithSharing)
{
    auto rows = Store<pool<Store<sequence<int>, urd::frozen>>, urd::structural_sharing>();
    const auto [whole, stored] = rows.insert({ 1, 2, 3, 4, 5, 6, 7, 8 });
    EXPECT_TRUE(stored);
    const auto memory = urd::memory_usage(rows);
    EXPECT_EQ(rows.find({ 1, 2, 3, 4, 5, 6, 7, 8 }), whole);
    EXPECT_FALSE(is_none(rows.find({ 1, 2, 3, 4 })));  // the first half is a node of the whole
    EXPECT_TRUE(is_none(rows.find({ 4, 3, 2, 1 })));
    EXPECT_EQ(urd::memory_usage(rows), memory);
    EXPECT_EQ(rows.insert({ 1, 2, 3, 4 }), std::pair(rows.find({ 1, 2, 3, 4 }), false));
    EXPECT_EQ(urd::memory_usage(rows), memory);  // inserting a found row stores nothing, and says so
}

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
    auto facts = Store<pool<Store<set<int>, urd::dense, urd::frozen>>>();
    const auto first = facts.insert({ 3, 1, 4 }).first;
    const auto second = facts.insert(std::vector<int> { 4, 3, 1, 1 }).first;
    EXPECT_EQ(first, second);
    EXPECT_NE(first, facts.insert({ 3, 1 }).first);
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
        const auto base = pool.insert(values).first;
        const auto levels = (static_cast<std::size_t>(std::bit_width(size)) + 1) / 2 + 1;  // ⌈log₄ size⌉ + 1, generously
        for (int round = 0; round < 20; ++round)
        {
            auto writes = std::vector<urd::Write<std::uint32_t>>();
            for (std::uint32_t position = 0; position < size; ++position)
                if (random() % 50 == 0)
                    writes.push_back({ position, static_cast<std::uint32_t>(random()) });
            const auto before = urd::testing::inspect::num_nodes(pool);
            const auto written = pool.insert(base, writes).first;
            EXPECT_LE(urd::testing::inspect::num_nodes(pool) - before, writes.size() * levels);
            auto expected = values;
            for (const auto& write : writes)
                expected[write.position] = write.value;
            EXPECT_EQ(written, pool.insert(expected).first);
        }
    }
}

namespace
{

template<typename P>
class Slots : public testing::Test
{
};

using SlotContainers = testing::Types<urd::repr::SlotVector<int>, urd::repr::SlotVector<int, true>, Store<slots<int>>, Store<slots<int>, urd::concurrent>>;
TYPED_TEST_SUITE(Slots, SlotContainers, urd::test::Names);

}  // namespace

// Without unique, a pool hands out a new slot per insert: a SlotVector, thread-safe where asked.
static_assert(std::same_as<Store<slots<int>>::representation, urd::repr::SlotVector<int>>);
static_assert(std::same_as<Store<slots<int>, urd::concurrent>::representation, urd::repr::SlotVector<int, true>>);
static_assert(urd::has<Store<slots<int>, urd::stable_refs>, urd::stable_refs>);

TYPED_TEST(Slots, KeepsElementsInPlaceAndReusesFreedSlots)
{
    for (unsigned seed = 0; seed < 32; ++seed)
    {
        const auto found = urd::verify(TypeParam(), seed, 1000);
        EXPECT_FALSE(found) << found.value_or("");
    }
}

// A scratch buffer per task: returned and handed out again, it keeps its storage, so reuse allocates nothing.
TEST(Slots, AReturnedElementIsHandedOutAgainAsItIs)
{
    auto buffers = Store<slots<std::vector<int>>>();
    auto first = buffers.make_unique();
    first->assign(1000, 7);
    const auto* slot = first.get();
    first.reset();
    const auto again = buffers.make_unique();
    EXPECT_EQ(again.get(), slot);
    EXPECT_GE(again->capacity(), 1000U);
    EXPECT_EQ(buffers.size(), 1U);
}

// A pool of rows of dense indices lends them out as their bits (rows of elements as spans: tests/unit/store/row_pools.cpp);
// views taken early still show their rows after many more inserts.
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
        using Set = Store<set<int>, urd::dense, urd::frozen>;
        auto pool = Store<urd::pool<Set>>();
        static_assert(std::same_as<decltype(pool)::representation, urd::repr::RowInterner<Set>>);
        auto handles = std::vector<decltype(pool)::handle_type>();
        auto views = std::vector<decltype(pool.at(handles.front()))>();
        for (int each = 0; each < 2000; ++each)
        {
            handles.push_back(pool.insert(row()).first);
            views.push_back(pool.at(handles.back()));
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
        EXPECT_EQ(pool.insert(std::vector<int> { 5, 3, 3, 200 }).first, pool.insert(std::vector<int> { 200, 5, 3 }).first);
    }
}

namespace
{

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

using Owners = Store<pool<Owner>, urd::flat, urd::monotone>;
using Items = Store<pool<Item>, urd::flat, urd::monotone>;

struct Shop
{
    Owners owners { urd::layer { 0 } };
    Items items { urd::layer { 0 } };

    [[nodiscard]] const Owner& operator[](urd::index<Owner> at) const { return owners[at]; }

    [[nodiscard]] const Item& operator[](urd::index<Item> at) const { return items[at]; }

    [[nodiscard]] std::size_t layer() const { return owners.layer(); }

    [[nodiscard]] urd::index_view<Owner, Shop> view(urd::index<Owner> at) const { return { at, this }; }

    [[nodiscard]] urd::index_view<Item, Shop> view(urd::index<Item> at) const { return { at, this }; }
};

}  // namespace

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

    EXPECT_EQ(shop.items.view(lamp).member<&Item::name>(), "lamp");  // the Store alone reads members that are no index
    const auto whole = urd::index_view<Item, Shop>(lamp, &shop);     // the shop resolves owners too
    EXPECT_EQ(whole.member<&Item::owners>()[0].member<&Owner::name>(), "ann");
    EXPECT_EQ((std::get<urd::index_view<Owner, Shop>>(whole.member<&Item::alternative>()).member<&Owner::name>()), "ann");
}

namespace
{
/// Tags of a key of two parts: a relation and a row of it.
struct KeyRelation
{
    static constexpr std::string_view name = "KeyRelation";
};

struct KeyRow
{
    static constexpr std::string_view name = "KeyRow";
};
}  // namespace

using RowKey = urd::key<KeyRelation, KeyRow>;

namespace
{
/// A record holding a key and a string: serialized by cista, the key part by part.
struct Keyed
{
    urd::flat_string label;
    RowKey row;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Keyed::label, &Keyed::row> {}; }

    static constexpr auto cista_members() noexcept { return urd::Members<&Keyed::label, &Keyed::row> {}; }
};
}  // namespace

static_assert(std::is_trivially_copyable_v<RowKey>);
static_assert(sizeof(urd::index<KeyRow>) == 4 && std::is_trivially_copyable_v<urd::index<KeyRow>>);

TEST(Key, ComparesHashesAndWritesPartByPart)
{
    const auto a = RowKey(urd::index<KeyRelation>(1), urd::index<KeyRow>(7));
    const auto b = RowKey(urd::index<KeyRelation>(2), urd::index<KeyRow>(0));
    EXPECT_EQ(a.get<KeyRelation>(), urd::index<KeyRelation>(1));
    EXPECT_EQ(a.get<KeyRow>(), urd::index<KeyRow>(7));
    EXPECT_LT(a, b);  // the first part first
    EXPECT_EQ(a, RowKey(urd::index<KeyRelation>(1), urd::index<KeyRow>(7)));
    EXPECT_EQ(urd::Hash<RowKey> {}(a), urd::Hash<RowKey> {}(RowKey(urd::index<KeyRelation>(1), urd::index<KeyRow>(7))));
    EXPECT_NE(urd::Hash<RowKey> {}(a), urd::Hash<RowKey> {}(b));
    EXPECT_TRUE(RowKey().is_none());
    EXPECT_FALSE(a.is_none());
    EXPECT_EQ(fmt::format("{}", a), "<1,7>");
}

TEST(Key, ItsPartsAreReadByAFreeGet)
{
    const auto a = RowKey(urd::index<KeyRelation>(1), urd::index<KeyRow>(7));
    EXPECT_EQ(urd::get<KeyRelation>(a), urd::index<KeyRelation>(1));  // a free function: no .template in dependent code
    EXPECT_EQ(urd::get<KeyRow>(a), urd::index<KeyRow>(7));
    EXPECT_EQ(urd::get<KeyRow>(urd::index<KeyRow>(3)), urd::index<KeyRow>(3));  // an index is a key of one part
}

namespace
{
/// An item numbered in its group's own pool: the record a key<KeyRelation, NestItem> names.
struct NestItem
{
    static constexpr std::string_view name = "NestItem";
};
}  // namespace

template<>
struct urd::data<NestItem>
{
    int weight = 0;
    /// Its key's last part, written by its store at insertion (a record holds its whole key).
    urd::index<NestItem> index;

    static constexpr auto identifying_members() noexcept { return urd::Members<&data::weight> {}; }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::weight, &data::index> {}; }
};

/// A group's items: a numbered pool of records (urd::layer, no default constructor), layered (an innermost container:
/// urd::HoldsRecordsOf).
using NestItems = Store<pool<urd::data<NestItem>, urd::index<NestItem>>, urd::flat, urd::layered>;
/// Items in nested containers: a group's index selects its items, an item's index numbers the item there. A layer's
/// own items of a group are a pool built in place over the parent layer's pool of the group (urd::repr::Layered), so
/// that their numbering continues the parent's.
using NestGroups = Store<urd::map<urd::index<KeyRelation>, NestItems>, urd::dense, urd::layered>;
static_assert(!std::default_initializable<NestItems>);
// A lookup hands out its record by value: a record_ref (the default stored_ref), trivially copyable.
static_assert(std::same_as<decltype(urd::record_at(std::declval<const NestItems&>(), urd::index<NestItem>())), urd::stored_ref<NestItem>>);
static_assert(std::same_as<urd::stored_ref<NestItem>, urd::record_ref<NestItem>> && std::is_trivially_copyable_v<urd::record_ref<NestItem>>);
static_assert(urd::HoldsRecordsOf<NestItems, NestItem> && urd::SelectsBy<NestGroups, KeyRelation>);

TEST(Key, ARecordIsFoundByFoldingOverTheKeysParts)
{
    using ItemKey = urd::key<KeyRelation, NestItem>;
    const auto group = urd::index<KeyRelation>(2);
    auto groups = NestGroups();
    groups.try_emplace(group);
    auto& items = *groups.own_value(group);
    const auto first = items.insert(urd::data<NestItem> { 5 }).first;
    const auto second = items.insert(urd::data<NestItem> { 8 }).first;
    EXPECT_EQ(urd::record_at(groups, ItemKey(group, first))->weight, 5);
    EXPECT_EQ(urd::record_at(groups, ItemKey(group, second))->weight, 8);
    EXPECT_EQ(&*urd::record_at(items, second), &items[second]);  // an index: the innermost container's own lookup
    EXPECT_TRUE(urd::contains_key(groups, ItemKey(group, second)));
    EXPECT_FALSE(urd::contains_key(groups, ItemKey(group, urd::index<NestItem>(2))));
    EXPECT_FALSE(urd::contains_key(groups, ItemKey(urd::index<KeyRelation>(3), first)));  // no group 3
    EXPECT_THROW((void) urd::record_at(groups, ItemKey(urd::index<KeyRelation>(3), first)), std::out_of_range);

    // Over a parent: a group the child never wrote is read from the parent; one it writes to has its own pool, built
    // over the parent's, whose numbering continues the parent's.
    auto child = NestGroups();
    child.layer_over(&groups);
    EXPECT_EQ(urd::record_at(child, ItemKey(group, second))->weight, 8);
    child.try_emplace(group);
    auto& own = *child.own_value(group);
    EXPECT_EQ(own.layer(), items.layer() + 1);
    EXPECT_EQ(own.insert(urd::data<NestItem> { 5 }).first, first);  // the parent's record, found through the chain
    const auto third = own.insert(urd::data<NestItem> { 13 }).first;
    EXPECT_EQ(third, urd::index<NestItem>(2));
    EXPECT_EQ(urd::record_at(child, ItemKey(group, third))->weight, 13);
    EXPECT_EQ(&*urd::record_at(child, ItemKey(group, second)), &items[second]);  // the parent's record, in place
    EXPECT_FALSE(urd::contains_key(groups, ItemKey(group, third)));              // the parent knows nothing of it
}

namespace
{
/// A family of stores, one per record type (here one, of items): a key of a kind first selects its kind's store.
struct NestFamily
{
    NestItems items = NestItems(urd::layer { 0 });

    template<std::same_as<NestItem> T>
    [[nodiscard]] const NestItems& get() const noexcept
    { return items; }
};
}  // namespace

static_assert(urd::SelectsStoreOf<NestFamily, NestItem> && !urd::HoldsRecordsOf<NestFamily, NestItem>);
static_assert(!urd::SelectsStoreOf<NestGroups, NestItem>);

TEST(Key, AFamilySelectsTheStoreOfTheKeysKindFirst)
{
    auto family = NestFamily();
    const auto at = family.items.insert(urd::data<NestItem> { 3 }).first;
    EXPECT_EQ(&*urd::record_at(family, at), &family.items[at]);  // the same fold: select the kind's store, then its lookup
    EXPECT_TRUE(urd::contains_key(family, at));
    EXPECT_FALSE(urd::contains_key(family, urd::index<NestItem>(1)));
    EXPECT_FALSE(urd::contains_key(family, urd::index<NestItem>()));  // none
}

namespace
{
/// A key of three parts: shelves (by relation) of blocks (by block) of items.
struct KeyBlock
{
    static constexpr std::string_view name = "KeyBlock";
};
}  // namespace

using NestBlocks = Store<urd::map<urd::index<KeyBlock>, NestItems>, urd::dense, urd::layered>;
using NestShelves = Store<urd::map<urd::index<KeyRelation>, NestBlocks>, urd::dense, urd::layered>;
static_assert(urd::SelectsForInsertBy<NestShelves, KeyRelation> && urd::InsertsRecordsOf<NestItems, NestItem>);

TEST(Key, ARecordIsInsertedUnderTheKnownPartsOfItsKey)
{
    auto record_5 = urd::data<NestItem> { 5 };
    auto record_8 = urd::data<NestItem> { 8 };
    auto record_21 = urd::data<NestItem> { 21 };
    using ShelfKey = urd::key<KeyRelation, KeyBlock, NestItem>;
    const auto known = urd::key<KeyRelation, KeyBlock>(urd::index<KeyRelation>(1), urd::index<KeyBlock>(4));
    auto shelves = NestShelves();
    const auto [first, inserted] = urd::insert_at(shelves, known, record_5);  // each level made on demand
    static_assert(std::same_as<std::remove_const_t<decltype(first)>, ShelfKey>);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(urd::get<NestItem>(first), urd::index<NestItem>(0));
    EXPECT_EQ(urd::insert_at(shelves, known, record_5), std::pair(first, false));  // deduplicated
    const auto second = urd::insert_at(shelves, known, record_8).first;
    EXPECT_EQ(record_8.index, urd::get<NestItem>(second));  // the record holds its whole key: its last part written in
    EXPECT_EQ(urd::get<NestItem>(second), urd::index<NestItem>(1));
    EXPECT_EQ(urd::record_at(shelves, second)->weight, 8);  // read back by the same fold
    EXPECT_EQ(urd::find_at(shelves, known, record_8), second);
    static_assert(std::same_as<urd::key<>, decltype(urd::built_key<>(record_8))>);  // a key of one part is known by nothing
    EXPECT_TRUE(urd::find_at(shelves, known, urd::data<NestItem> { 13 }).is_none());
    EXPECT_TRUE(urd::find_at(shelves, urd::key<KeyRelation, KeyBlock>(urd::index<KeyRelation>(1), urd::index<KeyBlock>(5)), urd::data<NestItem> { 5 })
                    .is_none());  // no block 5

    // Over a parent: its records found, its numbering continued in the child's own levels, built over the parent's.
    auto child = NestShelves();
    child.layer_over(&shelves);
    EXPECT_EQ(urd::find_at(child, known, urd::data<NestItem> { 8 }), second);
    EXPECT_EQ(urd::insert_at(child, known, record_5), std::pair(first, false));
    const auto third = urd::insert_at(child, known, record_21).first;
    EXPECT_EQ(urd::get<NestItem>(third), urd::index<NestItem>(2));
    EXPECT_EQ(urd::record_at(child, third)->weight, 21);
    EXPECT_EQ(&*urd::record_at(child, second), &*urd::record_at(shelves, second));    // the parent's record, in place
    EXPECT_TRUE(urd::find_at(shelves, known, urd::data<NestItem> { 21 }).is_none());  // the parent knows nothing of it
}

TEST(Key, StoredFlatPartByPart)
{
    auto records = Store<pool<Keyed>, urd::flat>(urd::layer { 0 });
    auto record = Keyed { urd::flat_string(std::string_view("at")), RowKey(urd::index<KeyRelation>(3), urd::index<KeyRow>(4)) };
    const auto [at, inserted] = records.insert(record);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(records[at].row, record.row);
    EXPECT_EQ(records.insert(record), std::pair(at, false));
}

TEST(Layered, RowPoolsContinueTheirParent)
{
    using Row = Store<sequence<int>, urd::frozen>;
    using RowIndex = urd::index<Row>;
    using Rows = Store<pool<Row>, urd::monotone, urd::layered>;
    auto parent = Rows();
    const auto first = parent.insert({ 1, 2, 3 }).first;
    const auto second = parent.insert({ 4 }).first;
    auto child = Rows();
    child.layer_over(&parent);
    EXPECT_EQ(child.base(), 2U);
    EXPECT_EQ(child.insert({ 1, 2, 3 }), std::pair(first, false));  // the parent's row, found through the chain
    const auto third = child.insert({ 5, 6 }).first;
    EXPECT_EQ(third, RowIndex(2));
    const auto expected_first = Row { 1, 2, 3 };
    const auto expected_third = Row { 5, 6 };
    EXPECT_EQ(child.get(first), expected_first);
    EXPECT_EQ(child.get(third), expected_third);
    EXPECT_EQ(child.row_size(second), 1U);
    EXPECT_EQ(child.at(first).size(), 3U);
    const auto writes = std::vector<urd::Write<int>> { { 1, 9 } };
    const auto written = child.insert(first, std::span<const urd::Write<int>>(writes)).first;
    EXPECT_EQ(written, RowIndex(3));
    const auto expected_written = Row { 1, 9, 3 };
    EXPECT_EQ(child.get(written), expected_written);
    EXPECT_EQ(parent.insert({ 1, 9, 3 }).first, RowIndex(2));  // the parent knows nothing of it
    EXPECT_TRUE((urd::testing::inspect::parent(child) != nullptr && urd::testing::inspect::parent(child)->size() > child.base()));
    EXPECT_THROW((void) child.insert({ 7, 7 }), std::logic_error);  // its new indices would collide with the parent's
    EXPECT_EQ(urd::testing::inspect::parent(child), &parent);
}

TEST(Layered, ViewsPointAtTheOwningLayer)
{
    using Points = Store<pool<Point>, urd::flat, urd::layered>;
    using PointView = urd::index_view<Point, Points>;
    auto root = Points(urd::layer { 0 });
    const auto one = root.insert(Point(1)).first;
    auto child = Points(urd::layer { 1 });
    child.layer_over(&root);
    const auto two = child.insert(Point(2)).first;
    EXPECT_EQ(child.insert(Point(1)).first, one);
    EXPECT_EQ(&child.owner(one), &root);
    EXPECT_EQ(&child.owner(two), &child);
    const auto equal = urd::EqualTo<PointView> {};
    EXPECT_TRUE(equal(child.view(one), root.view(one)));  // the same element, whichever layer it was asked of
    EXPECT_EQ(urd::Hash<PointView> {}(child.view(one)), urd::Hash<PointView> {}(root.view(one)));
    EXPECT_FALSE(equal(child.view(two), root.view(one)));
    EXPECT_EQ(child.view(one).member<&Point::x>(), 1);
    EXPECT_EQ(child.view(two).context().layer(), 1U);
    EXPECT_EQ(child.view(one).context().layer(), 0U);
    EXPECT_FALSE(child.contains(urd::index<Point>(2)));
    child.clear();
    EXPECT_EQ(child.size(), 1U);
    EXPECT_FALSE(child.contains(Point(2)));
    EXPECT_TRUE(child.contains(Point(1)));
}

// A pool names the handle it hands out (urd::pool<V, H>): a pool of urd::data<T>, the record a Store holds for an
// index<T>, hands out index<T>, and its views are of T; layered too, its indices continuing the parent's.
namespace
{
struct Symbol;
}  // namespace

template<>
struct urd::data<Symbol>
{
    int id = 0;

    data() = default;

    explicit data(int id_) : id(id_) {}

    static constexpr bool copyable() noexcept
    {
        static_assert(std::is_trivially_copyable_v<data>, "a copyable record is trivially copyable");
        return true;
    }

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"id", &data::id>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::id> {}; }
};

template<typename V>
struct urd::view_mixin<Symbol, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] auto get_id() const noexcept { return self().template member<&urd::record_of<V>::id>(); }
};

namespace
{
using SymbolIndex = urd::index<Symbol>;
using Symbols = Store<pool<urd::data<Symbol>, SymbolIndex>, urd::flat>;
using LayeredSymbols = Store<pool<urd::data<Symbol>, SymbolIndex>, urd::flat, urd::stable_refs, urd::layered>;
static_assert(std::same_as<Symbols::handle_type, SymbolIndex>);
static_assert(std::same_as<Symbols::representation, urd::repr::Numbered<urd::repr::RecordInterner<urd::data<Symbol>, false, false, SymbolIndex>>>);
static_assert(std::same_as<LayeredSymbols::handle_type, SymbolIndex>);
// The default is unchanged: a pool of V hands out index<V>.
static_assert(std::same_as<Store<pool<Point>, urd::flat>::handle_type, urd::index<Point>>);
}  // namespace

TEST(Pools, NameTheHandleTheyHandOut)
{
    auto symbols = Symbols(urd::layer { 0 });
    const auto [a, inserted] = symbols.insert(urd::data<Symbol> { 7 });
    static_assert(std::same_as<std::remove_const_t<decltype(a)>, SymbolIndex>);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(symbols.find(urd::data<Symbol> { 7 }), a);
    EXPECT_EQ(symbols[a].id, 7);
    static_assert(std::same_as<decltype(symbols.view(a)), urd::index_view<Symbol, Symbols>>);
    EXPECT_EQ(symbols.view(a).index(), a);

    auto root = LayeredSymbols(urd::layer { 0 });
    const auto one = root.insert(urd::data<Symbol> { 1 }).first;
    auto child = LayeredSymbols(urd::layer { 1 });
    child.layer_over(&root);
    const auto two = child.insert(urd::data<Symbol> { 2 }).first;
    EXPECT_EQ(child.insert(urd::data<Symbol> { 1 }).first, one);
    EXPECT_EQ(two, SymbolIndex(1));
    EXPECT_EQ(child[two].id, 2);
    EXPECT_EQ(child[one].id, 1);
    EXPECT_EQ(&child.owner(one), &root);
}

namespace
{
using IndexedRows = Store<pool<Store<sequence<int>, urd::frozen>>, urd::multi_indexed>;
using IndexedColumns = Store<pool<Store<sequence<std::uint32_t>, urd::frozen>>, urd::by_column_widths, urd::multi_indexed>;
static_assert(std::same_as<IndexedRows::representation, urd::repr::MultiIndexPool<Store<pool<Store<sequence<int>, urd::frozen>>>::representation>>);
static_assert(iterable<IndexedRows> && erasable<IndexedRows> && erasable<IndexedColumns>);

/// The handles of the rows of `rows` whose `columns` hold `key`, by brute force, sorted.
template<typename P>
std::vector<typename P::handle_type>
matching(const P& rows, const std::vector<std::size_t>& columns, const std::vector<typename P::value_type::value_type>& key)
{
    auto result = std::vector<typename P::handle_type>();
    for (const auto handle : rows)
    {
        const auto row = rows.get(handle);
        if (std::ranges::equal(columns | std::views::transform([&](std::size_t c) { return row[c]; }), key))
            result.push_back(handle);
    }
    return result;
}

template<typename P>
std::vector<typename P::handle_type> sorted(std::pair<typename P::const_iterator, typename P::const_iterator> range)
{
    auto result = std::vector<typename P::handle_type>(range.first, range.second);
    std::ranges::sort(result);
    return result;
}

/// Random inserts and erases, with indexes added before and in between; every index answers as a scan does.
template<typename P>
void check_indexes(P rows)
{
    using E = typename P::value_type::value_type;
    auto random = std::mt19937(7);  // NOLINT(cert-msc32-c,cert-msc51-cpp): a fixed seed, for a reproducible test
    const auto columns = std::vector<std::vector<std::size_t>> { { 0 }, { 2, 0 }, {}, { 1, 2 } };
    auto numbers = std::vector<std::size_t>();
    numbers.push_back(rows.add_index(columns[0]));
    numbers.push_back(rows.add_index(columns[1]));
    EXPECT_EQ(rows.add_index(columns[0]), numbers[0]);  // the same columns: the same index
    for (int step = 0; step < 2000; ++step)
    {
        if (step == 500)
        {
            numbers.push_back(rows.add_index(columns[2]));
            numbers.push_back(rows.add_index(columns[3]));
        }
        if (step == 1500)
            rows.clear();
        const auto row = std::vector<E> { E(random() % 5), E(random() % 3), E(random() % 4) };
        if (random() % 3 == 0 && !rows.empty())
            rows.erase(typename P::handle_type(static_cast<std::uint32_t>(random() % rows.size())));
        else
            rows.insert(row);
        for (std::size_t i = 0; i < numbers.size(); ++i)
        {
            auto key = std::vector<E>();
            for (const auto c : columns[i])
                key.push_back(row[c]);
            const auto index = rows.index(numbers[i]);
            const auto expected = matching(rows, columns[i], key);
            ASSERT_EQ(index.count(key), expected.size());
            ASSERT_EQ(index.contains(key), !expected.empty());
            ASSERT_EQ(sorted<P>(index.equal_range(key)), expected);
        }
    }
}
}  // namespace

// A multi-index pool keeps its indexes as rows come and go, the last row moving into an erased one's handle.
TEST(MultiIndexed, IndexesAnswerAsAScan)
{
    check_indexes(IndexedRows());
    check_indexes(IndexedColumns(urd::column_widths(3, 8)));
}

TEST(MultiIndexed, IteratesItsHandles)
{
    auto rows = IndexedRows();
    const auto a = rows.insert({ 1, 2 }).first;
    const auto b = rows.insert({ 1, 3 }).first;
    EXPECT_EQ(std::vector(rows.begin(), rows.end()), (std::vector { a, b }));
    const auto by_first = rows.add_index(std::vector<std::size_t> { 0 });
    EXPECT_EQ(rows.index(by_first).count(std::vector { 1 }), 2U);
    rows.erase(a);  // b moves into a's handle
    EXPECT_EQ(std::vector(rows.begin(), rows.end()), std::vector { a });
    EXPECT_EQ(*rows.index(by_first).find(std::vector { 1 }), a);
    EXPECT_EQ(rows.index(by_first).find(std::vector { 2 }), rows.index(by_first).end());
}
