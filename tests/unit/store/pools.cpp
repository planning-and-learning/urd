// Interners, partitions and pools of records. Each header a Store here needs is included.

#include "urd/elements/concepts.hpp"
#include "urd/elements/data.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/view.hpp"
#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/pools/tree_bitset.hpp"
#include "urd/repr/records/arena.hpp"
#include "urd/repr/records/records.hpp"
#include "urd/repr/sequences/row_pool.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/repr/wrappers/layered_pool.hpp"
#include "urd/store.hpp"
#include "urd/testing/allocations.hpp"

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
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
struct CellTag;
}  // namespace

/// A record that is copyable, and says so.
template<>
struct urd::data<CellTag>
{
    std::uint32_t id = 0;

    data() = default;

    explicit data(std::uint32_t id_) : id(id_) {}

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
struct urd::view_mixin<CellTag, V>
{
private:
    const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    auto get_id() const noexcept { return self().template member<&urd::record_of<V>::id>(); }
};

namespace
{

using urd::partition;
using urd::pool;
using urd::set;
using urd::Store;

// An interner: each new element gets the next index, which stays its index; a known one keeps its index.
TEST(Interner, GivesEachElementItsIndex)
{
    auto symbols = Store<set<std::string>, urd::positional, urd::monotone>(urd::layer { 0 });
    const auto index = [&symbols](const std::string& symbol) { return symbols.position(symbols.insert(symbol).first); };
    EXPECT_EQ(index("on"), 0U);
    EXPECT_EQ(index("clear"), 1U);
    EXPECT_EQ(index("on"), 0U);
    EXPECT_EQ(symbols[1], "clear");
    EXPECT_EQ(symbols.position(symbols.find("clear")), 1U);
    EXPECT_EQ(symbols.position(symbols.find("holding")), symbols.size());
    EXPECT_EQ(std::vector<std::string>(symbols.rbegin(), symbols.rend()), (std::vector<std::string> { "clear", "on" }));
}

// A partition says whether a key is an element.
TEST(Partition, Contains)
{
    auto classes = Store<partition<int>>();
    classes.insert(3);
    EXPECT_TRUE(classes.contains(0) && classes.contains(3) && !classes.contains(4));
}

// A pool of records visits them in index order.
TEST(Records, Iterate)
{
    struct Name
    {
        int id = 0;

        static constexpr auto identifying_members() noexcept { return urd::Members<&Name::id> {}; }
    };

    auto names = Store<pool<Name>, urd::flat>(urd::layer { 0 });
    names.insert(Name { 7 });
    names.insert(Name { 2 });
    names.insert(Name { 7 });
    auto ids = std::vector<int>();
    for (const auto& name : names)
        ids.push_back(name.id);
    EXPECT_EQ(ids, (std::vector { 7, 2 }));
}

// A pool of records counts a record as a set does: once, or not at all.
TEST(Records, Count)
{
    struct Point
    {
        int x = 0;

        static constexpr auto identifying_members() noexcept { return urd::Members<&Point::x> {}; }
    };

    auto points = Store<pool<Point>, urd::flat>(urd::layer { 0 });
    points.insert(Point { 1 });
    EXPECT_EQ(points.count(Point { 1 }), 1U);
    EXPECT_EQ(points.count(Point { 2 }), 0U);
}

/// A move-only record that owns memory: interned serialized (flat, the arena).
struct Owning
{
    urd::flat_vector<std::uint32_t> items;

    Owning() = default;
    Owning(const Owning&) = delete;
    Owning& operator=(const Owning&) = delete;
    Owning(Owning&&) = default;
    Owning& operator=(Owning&&) = default;
    ~Owning() = default;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Owning::items> {}; }

    static constexpr auto cista_members() noexcept { return urd::Members<&Owning::items> {}; }
};

/// A copyable record: interned by value from a const T&, copied; the record given stays as it was.
template<typename Records>
void interns_from_a_reference()
{
    using Cell = urd::data<CellTag>;
    auto records = Records(urd::layer { 0 });
    const auto record = Cell { 1 };
    const auto [at, inserted] = records.insert(record);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(records.insert(record), std::pair(at, false));
    EXPECT_EQ(records.insert(Cell { 2 }).first.value, at.value + 1);
    EXPECT_EQ(records.insert(record, [&record](auto) -> const Cell& { return record; }), std::pair(at, false));
    EXPECT_EQ(records[at].id, 1U);
    EXPECT_EQ(record.id, 1U);
}

TEST(Records, InternFromAReference)
{
    interns_from_a_reference<Store<pool<urd::data<CellTag>>>>();
    interns_from_a_reference<Store<pool<urd::data<CellTag>>, urd::flat>>();
    interns_from_a_reference<Store<pool<urd::data<CellTag>>, urd::flat, urd::monotone, urd::layered>>();
}

// A scratch record interned is the caller's still: its buffer, contents and all, as a checked-out builder reuses it.
TEST(Records, InternLeavesTheRecordAsItWas)
{
    auto records = Store<pool<Owning>, urd::flat>(urd::layer { 0 });
    auto scratch = Owning();
    for (std::uint32_t item = 0; item < 3; ++item)
        scratch.items.push_back(item);
    const auto* const buffer = scratch.items.data();
    const auto at = records.insert(scratch).first;
    EXPECT_EQ(scratch.items.data(), buffer);
    EXPECT_EQ(scratch.items.size(), 3U);
    EXPECT_EQ(records[at].items.size(), 3U);
    EXPECT_NE(records[at].items.data(), buffer);
    scratch.items.push_back(3);
    EXPECT_TRUE(records.insert(scratch).second);
}

/// A move-only record that tells whether it was moved from.
struct Moving
{
    std::uint32_t id = 0;
    bool moved_from = false;

    Moving() = default;

    explicit Moving(std::uint32_t value) : id(value) {}

    Moving(const Moving&) = delete;
    Moving& operator=(const Moving&) = delete;

    Moving(Moving&& other) noexcept : id(other.id) { other.moved_from = true; }

    Moving& operator=(Moving&& other) noexcept
    {
        id = other.id;
        other.moved_from = true;
        return *this;
    }

    ~Moving() = default;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Moving::id> {}; }

    static constexpr auto cista_members() noexcept { return urd::Members<&Moving::id, &Moving::moved_from> {}; }
};

/// insert(T&&) hands a record over: moved in where it is new (serialized from, where records are serialized:
/// moves_where_new false), left as it was where an equal one is held; a make that gives T&& likewise.
template<typename Records>
void hands_over_only_where_new(bool moves_where_new)
{
    auto records = Records(urd::layer { 0 });
    auto first = Moving(1);
    const auto [at, inserted] = records.insert(std::move(first));
    EXPECT_TRUE(inserted);
    EXPECT_EQ(first.moved_from, moves_where_new);  // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved): what the move left
    auto again = Moving(1);
    EXPECT_EQ(records.insert(std::move(again)), std::pair(at, false));
    EXPECT_FALSE(again.moved_from) << "a record already held leaves the argument as it was";  // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
    auto made = Moving(2);
    const auto key = Moving(2);
    EXPECT_TRUE(records
                    .insert(key, [&made](auto) -> Moving&& { return std::move(made); })
                    .second);
    EXPECT_EQ(made.moved_from, moves_where_new);
    EXPECT_EQ(records[at].id, 1U);
    EXPECT_FALSE(records[at].moved_from);
}

TEST(Records, HandOverOnlyWhereNew)
{
    hands_over_only_where_new<Store<pool<Moving>>>(true);
    hands_over_only_where_new<Store<pool<Moving>, urd::stable_refs>>(true);
    hands_over_only_where_new<Store<pool<Moving>, urd::flat>>(false);
    hands_over_only_where_new<Store<pool<Moving>, urd::flat, urd::monotone, urd::layered>>(false);
}

// A partition: union-find over indices, by request.
static_assert(std::same_as<Store<partition<int>>::representation, urd::repr::DisjointSets<int>>);

// A partition answers what a labeling recomputed after each merge answers.
TEST(Partition, AgreesWithRelabeling)
{
    auto random = std::mt19937(17);
    for (int round = 0; round < 50; ++round)
    {
        constexpr std::size_t size = 24;
        auto partition = Store<urd::partition<int>>();
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
                ASSERT_EQ(partition.find(lhs) == partition.find(each), label[static_cast<std::size_t>(lhs)] == label[static_cast<std::size_t>(each)]);
                ASSERT_EQ(partition.find(partition.find(each)), partition.find(each));
            }
        }
    }
}

// An interner's table hashes through its own elements: copies and moves each keep theirs, and a moved-from interner
// starts empty and works.
TEST(Interner, CopiesAndMovesKeepTheirOwnIndex)
{
    using Symbols = Store<set<std::string>, urd::positional>;
    auto symbols = Symbols(urd::layer { 0 }, { "on", "clear" });
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
    auto ids = Store<set<std::uint32_t>, urd::positional>(urd::layer { 0 });
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
    auto symbols = Store<set<int>, urd::positional, urd::monotone, urd::concurrent>(urd::layer { 0 });
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

// A dense set hands out its words and takes them back, through the Store (key k: bit k % 64 of word k / 64).
TEST(DenseSets, WordsGoOutAndBackThroughTheStore)
{
    using Bits = Store<set<int>, urd::dense>;
    auto keys = Bits();
    for (const int key : { 1, 64, 130 })
        keys.insert(key);
    const auto words = std::vector<std::uint64_t>(keys.significant_words().begin(), keys.significant_words().end());
    EXPECT_EQ(words, (std::vector<std::uint64_t> { 2, 1, 4 }));
    auto copy = Bits();
    copy.insert(500);
    copy.assign_words(words);
    EXPECT_EQ(copy.size(), 3U);
    EXPECT_FALSE(copy.contains(500));
    EXPECT_TRUE(std::ranges::equal(copy, keys));
    copy.assign_words(2, [](std::span<std::uint64_t> in_place) { in_place[1] = 1; });
    EXPECT_TRUE(std::ranges::equal(copy, std::vector<int> { 64 }));
}

// A tree pool of dense sets takes a row by its words and writes one back word by word; reading a row's size, its
// elements, or the row into a set that holds its words already allocates nothing.
TEST(TreeBitsets, RowsGoInAndOutByWordsWithoutAllocating)
{
    using Row = Store<set<int>, urd::dense, urd::frozen>;
    using Rows = Store<pool<Row>, urd::structural_sharing>;
    auto rows = Rows();
    const auto values = std::vector<int> { 1, 33, 64, 200 };
    const auto row = Row(values.begin(), values.end());
    const auto [by_elements, stored] = rows.insert(values);
    EXPECT_TRUE(stored);
    const auto [by_words, again] = rows.insert(row);
    EXPECT_EQ(by_words, by_elements);
    EXPECT_FALSE(again);
    EXPECT_EQ(rows.find(row), by_elements);
    auto elements = std::vector<int>(values.size());
    auto out = Row(values.begin(), values.end());
    auto size = std::size_t { 0 };
    {
        const auto counted = urd::testing::Counted();
        size = rows.row_size(by_words);
        rows.copy(by_words, std::span<int>(elements));
        rows.copy(by_words, out);
    }
    if (urd::testing::counts_allocations)
    {
        EXPECT_EQ(urd::testing::counts().allocations, 0);
    }
    EXPECT_EQ(size, values.size());
    EXPECT_EQ(elements, values);
    EXPECT_TRUE(std::ranges::equal(out, row));
    // Into a set of other keys: its words replaced by the row's.
    auto other = Row(values.begin(), values.begin() + 1);
    rows.copy(by_words, other);
    EXPECT_TRUE(std::ranges::equal(other, values));
}

}  // namespace

// Every container with a size answers empty() itself (the sizing facet forwards it): a partition, a pool of rows and a
// sequence of rows kept for reuse.
TEST(Sizing, EmptyIsTheContainers)
{
    auto partition = Store<urd::partition<int>>();
    EXPECT_TRUE(partition.empty());
    partition.insert(2);
    EXPECT_FALSE(partition.empty());
    auto rows = Store<urd::pool<Store<urd::sequence<int>, urd::stable_refs, urd::frozen>>>();
    EXPECT_TRUE(rows.empty());
    rows.insert(std::vector<int> { 1, 2 });
    EXPECT_FALSE(rows.empty());
    auto kept = Store<urd::sequence<Store<urd::sequence<int>, urd::retains>>, urd::retains>();
    EXPECT_TRUE(kept.empty());
    kept.emplace_back();
    EXPECT_FALSE(kept.empty());
}
