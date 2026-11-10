// Pools of plain rows: a pool of frozen sequences of trivially copyable elements stores its rows flat (no vector per
// row, no offsets while all have one width), hands them out as spans that stay where they are, and looks a row up by
// the span of its elements.

#include "urd/elements/subset_view.hpp"
#include "urd/repr/pools/pools.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/wrappers/layered_pool.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>
#include <gtest/gtest.h>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using Row = urd::Store<urd::sequence<std::uint32_t>, urd::frozen>;
using Rows = urd::Store<urd::pool<Row>>;

}  // namespace

// A row's handle is the order it came in; an equal row, however given, finds it.
TEST(RowPools, InternRowsBySpans)
{
    auto rows = Rows();
    const auto a = std::array<std::uint32_t, 3> { 1, 2, 3 };
    const auto b = std::vector<std::uint32_t> { 3, 2, 1 };
    EXPECT_TRUE(rows.insert(std::span<const std::uint32_t>(a)).second);
    EXPECT_TRUE(rows.insert(b).second);
    EXPECT_FALSE(rows.insert(Row { 1, 2, 3 }).second);
    EXPECT_EQ(rows.size(), 2U);
    EXPECT_EQ(rows.find(a).value, 0U);
    EXPECT_EQ(rows.find(b).value, 1U);
    EXPECT_TRUE(std::ranges::equal(rows.at(rows.find(b)), b));
    EXPECT_TRUE(rows.find(std::vector<std::uint32_t> { 1, 2 }).is_none());
    EXPECT_TRUE(rows.contains(b));
    EXPECT_FALSE(rows.contains(std::vector<std::uint32_t> { 1, 2 }));
}

// Rows handed out stay where they are while more come, of one width and after rows of others.
TEST(RowPools, RowsNeverMoveOnInsert)
{
    auto rows = Rows();
    auto random = std::mt19937(3);
    auto reference = std::vector<std::vector<std::uint32_t>>();
    auto views = std::vector<std::span<const std::uint32_t>>();
    for (int round = 0; round < 5000; ++round)
    {
        // Mostly width 3; from round 3000 on, any width up to 5, the empty row included.
        const auto width = round < 3000 ? 3U : static_cast<std::uint32_t>(random() % 6);
        auto row = std::vector<std::uint32_t>(width);
        std::ranges::generate(row, [&] { return static_cast<std::uint32_t>(random() % 50); });
        const auto [handle, inserted] = rows.insert(row);
        if (inserted)
        {
            reference.push_back(row);
            views.push_back(rows.at(handle));
        }
    }
    ASSERT_EQ(rows.size(), reference.size());
    for (std::uint32_t at = 0; at < reference.size(); ++at)
    {
        const auto handle = urd::index<Row>(at);
        EXPECT_TRUE(std::ranges::equal(rows.at(handle), reference[at]));
        EXPECT_EQ(rows.at(handle).data(), views[at].data());
        EXPECT_EQ(rows.find(reference[at]), handle);
    }
}

// Agrees with a std::set of vectors, through erase (the last row moves into the erased one's place) and clear.
TEST(RowPools, AgreeWithAStandardSet)
{
    for (const bool mixed : { false, true })
    {
        auto rows = Rows();
        auto expected = std::set<std::vector<std::uint32_t>>();
        auto random = std::mt19937(11);
        const auto copy = [&rows](std::size_t at)
        {
            const auto row = rows.at(urd::index<Row>(static_cast<std::uint32_t>(at)));
            return std::vector<std::uint32_t>(row.begin(), row.end());
        };
        for (int round = 0; round < 3; ++round)
        {
            for (int count = 0; count < 300; ++count)
            {
                auto row = std::vector<std::uint32_t>(mixed ? 1 + (random() % 3) : 2);
                std::ranges::generate(row, [&] { return static_cast<std::uint32_t>(random() % 20); });
                EXPECT_EQ(rows.insert(row).second, expected.insert(row).second);
            }
            EXPECT_EQ(rows.size(), expected.size());
            const auto first = copy(0);
            const auto last = copy(rows.size() - 1);
            const auto moved = rows.size() - 1;
            EXPECT_EQ(rows.erase(urd::index<Row>(0)).value, moved);  // the handle the moved row had
            expected.erase(first);
            EXPECT_EQ(rows.size(), expected.size());
            EXPECT_EQ(rows.find(last).value, 0U);
            for (const auto& row : expected)
                EXPECT_FALSE(rows.find(row).is_none());
            EXPECT_TRUE(rows.find(first).is_none());
            rows.clear();
            expected.clear();
            EXPECT_EQ(rows.size(), 0U);
        }
    }
}

namespace
{

/// A plain element whose hash counts its calls.
struct Counted
{
    std::uint32_t value;

    friend bool operator==(Counted, Counted) = default;
};
int hashes = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): the counter the test reads

}  // namespace

template<>
struct urd::Hash<Counted>
{
    urd::hash_t operator()(Counted counted) const noexcept
    {
        ++hashes;
        return urd::Hash<std::uint32_t> {}(counted.value);
    }
};

// A row with its hash is not hashed again: what a caller that hashes once for several lookups (a repository and its
// parent) gives.
TEST(RowPools, PrehashedRowsAreHashedOnce)
{
    auto rows = urd::Store<urd::pool<urd::Store<urd::sequence<Counted>, urd::frozen>>>();
    const auto row = std::array<Counted, 3> { Counted { 1 }, Counted { 2 }, Counted { 3 } };
    hashes = 0;
    const auto hash = urd::hash_range(row);
    EXPECT_EQ(hashes, 3);
    const auto key = urd::prehashed<std::array<Counted, 3>> { row, hash };
    EXPECT_TRUE(rows.find(key).is_none());
    const auto [handle, inserted] = rows.insert(key);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(rows.find(key), handle);
    EXPECT_TRUE(rows.contains(key));
    EXPECT_EQ(hashes, 3) << "lookups and the insert by a prehashed row hash nothing";
    EXPECT_EQ(rows.find(row), handle);  // the same row, hashed here, finds it
}

// A layered pool passes a prehashed row through its chain: found in the parent without hashing again.
TEST(RowPools, LayeredPrehashedRowsAreHashedOnce)
{
    using Rows = urd::Store<urd::pool<urd::Store<urd::sequence<Counted>, urd::frozen>>, urd::monotone, urd::layered>;
    const auto row = std::array<Counted, 3> { Counted { 1 }, Counted { 2 }, Counted { 3 } };
    auto parent = Rows();
    const auto handle = parent.insert(row).first;
    auto child = Rows();
    child.layer_over(&parent);
    hashes = 0;
    const auto key = urd::prehashed<std::array<Counted, 3>> { row, urd::hash_range(row) };
    EXPECT_EQ(child.find(key), handle);
    EXPECT_EQ(child.insert(key), std::pair(handle, false));
    const auto other = std::array<Counted, 3> { Counted { 4 }, Counted { 5 }, Counted { 6 } };
    const auto other_key = urd::prehashed<std::array<Counted, 3>> { other, urd::hash_range(other) };
    EXPECT_TRUE(child.insert(other_key).second);
    EXPECT_EQ(child.find(other_key).value, 1U);
    EXPECT_EQ(hashes, 6) << "each row hashed once, for every layer of the chain";
}

// A pool of rows of indices whose request promises urd::bit stores each element in the bit width it is built with:
// the bit row interner, whose rows are ranges decoding their elements; the same rows, handles and lookups as before.
TEST(RowPools, BitRowsPackElements)
{
    using Index = urd::index<struct BitObject>;
    using IndexRow = urd::Store<urd::sequence<Index>, urd::frozen>;
    using Packed = urd::Store<urd::pool<IndexRow>, urd::bit>;
    using ConcurrentPacked = urd::Store<urd::pool<IndexRow>, urd::bit, urd::concurrent>;
    static_assert(std::same_as<Packed::representation, urd::repr::RowInterner<IndexRow, false, urd::width::bit>>);
    static_assert(std::same_as<ConcurrentPacked::representation, urd::repr::RowInterner<IndexRow, true, urd::width::bit>>);
    // A word representation still serves a request of urd::bit it cannot use (rows of strings have no indices).
    using TextRow = urd::Store<urd::sequence<std::string>, urd::frozen>;
    static_assert(std::same_as<urd::Store<urd::pool<TextRow>, urd::bit>::representation, urd::repr::RowInterner<TextRow>>);

    const auto check = []<typename Pool>()
    {
        auto rows = Pool(urd::bit_width::for_bound(1000));  // 10 bits per element
        auto random = std::mt19937(7);
        auto expected = std::set<std::vector<std::uint32_t>>();
        auto handles = std::vector<std::pair<std::vector<std::uint32_t>, urd::index<IndexRow>>>();
        for (int count = 0; count < 3000; ++count)
        {
            // Rows of three elements; from count 2000 on, of 1 to 4 (the table of rows of other lengths).
            const auto length = count < 2000 ? 3U : 1 + static_cast<std::uint32_t>(random() % 4);
            auto values = std::vector<std::uint32_t>(length);
            std::ranges::generate(values, [&] { return static_cast<std::uint32_t>(random() % 1000); });
            auto row = std::vector<Index>();
            for (const auto value : values)
                row.push_back(Index(value));
            const auto [handle, inserted] = rows.insert(row);
            EXPECT_EQ(inserted, expected.insert(values).second);
            if (inserted)
                handles.emplace_back(values, handle);
        }
        EXPECT_EQ(rows.size(), expected.size());
        for (const auto& [values, handle] : handles)
        {
            const auto row = rows.at(handle);
            ASSERT_EQ(row.size(), values.size());
            for (std::size_t at = 0; at < values.size(); ++at)
                EXPECT_EQ(row[at].value, values[at]);
            auto lookup = std::vector<Index>();
            for (const auto value : values)
                lookup.push_back(Index(value));
            EXPECT_EQ(rows.find(lookup), handle);
        }
    };
    check.template operator()<Packed>();
    check.template operator()<ConcurrentPacked>();

    // An element that does not fit in the width (a broken promise) throws, and stores nothing.
    {
        auto narrow = Packed(urd::bit_width { 4 });
        narrow.insert(std::vector<Index> { Index(3), Index(15) });
        EXPECT_THROW(narrow.insert(std::vector<Index> { Index(3), Index(16) }), std::out_of_range);
        EXPECT_EQ(narrow.size(), 1U);
        EXPECT_TRUE(narrow.find(std::vector<Index> { Index(3), Index(16) }).is_none());
    }

    // Ten bits per element: a row of three takes 30 bits, not 12 bytes. Both pools also hold a table of positions;
    // the packed one saves at least most of the 8 bytes per row the narrower elements leave.
    auto packed = Packed(urd::bit_width { 10 });
    auto words = urd::Store<urd::pool<IndexRow>>();
    for (std::uint32_t row = 0; row < 100000; ++row)
    {
        const auto elements = std::vector<Index> { Index(row % 1000), Index((row / 1000) % 1000), Index(row % 7) };
        packed.insert(elements);
        words.insert(elements);
    }
    EXPECT_GT(urd::memory_usage(words), urd::memory_usage(packed) + (100000 * 7));
}

// A pool of rows whose request promises urd::by_column_widths stores rows of unsigned columns, each column in the bits
// its width gives it: rows read back column by column (get), equal rows found however given.
TEST(RowPools, SchemaRowsMixColumns)
{
    using Values = urd::Store<urd::sequence<std::uint64_t>, urd::frozen>;
    using Rows = urd::Store<urd::pool<Values>, urd::by_column_widths>;
    using ConcurrentRows = urd::Store<urd::pool<Values>, urd::by_column_widths, urd::concurrent>;
    static_assert(std::same_as<Rows::representation, urd::repr::RowInterner<Values, false, urd::width::by_column_widths>>);
    static_assert(std::same_as<Rows::row_type, urd::repr::SchemaRow<false>>);
    static_assert(std::same_as<Rows::row_builder_type, urd::repr::EncodedRow<false>>);
    const auto columns = urd::column_widths { 10, 32, 7, 64, 1, 32, 64 };
    EXPECT_EQ(columns.bits(), 10U + 32 + 7 + 64 + 1 + 32 + 64);

    const auto check = [&]<typename Pool>()
    {
        auto rows = Pool(columns);
        auto handles = std::vector<urd::index<Values>>();
        for (std::uint64_t at = 0; at < 2000; ++at)
        {
            const auto values = std::array<std::uint64_t, 7> { at % 1000, at + (1ULL << 31), at % 128, at << 50, at % 2, at * 3, at << 40 };
            const auto [handle, inserted] = rows.insert(values);
            EXPECT_TRUE(inserted);
            EXPECT_EQ(rows.find(values), handle);
            handles.push_back(handle);
        }
        for (std::uint64_t at = 0; at < 2000; ++at)
        {
            const auto row = rows.at(handles[at]);
            ASSERT_EQ(row.size(), 7U);
            EXPECT_EQ(row.get(0), at % 1000);
            EXPECT_EQ(row.get(1), at + (1ULL << 31));
            EXPECT_EQ(row.get(2), at % 128);
            EXPECT_EQ(row.get(3), at << 50);
            EXPECT_EQ(row.template get<bool>(4), at % 2 == 1);
            EXPECT_EQ(row.template get<std::uint32_t>(5), at * 3);
            EXPECT_EQ(row.get(6), at << 40);
            // A stored row is found as it is handed out.
            EXPECT_EQ(rows.find(row), handles[at]);
        }
        EXPECT_EQ(rows.size(), 2000U);
    };
    check.template operator()<Rows>();
    check.template operator()<ConcurrentRows>();

    // A row built column by column is found; a value that does not fit is refused.
    auto rows = Rows(columns);
    auto built = Rows::row_builder_type(rows.column_widths());
    for (std::size_t i = 0; i < columns.size(); ++i)
        EXPECT_TRUE(built.set(i, 1));
    EXPECT_FALSE(built.set(4, 2));
    const auto handle = rows.insert(built.row()).first;
    EXPECT_EQ(rows.find(std::array<std::uint64_t, 7> { 1, 1, 1, 1, 1, 1, 1 }), handle);

    // A value that does not fit its column throws and stores nothing; a row of another arity is rejected.
    auto narrow = Rows(urd::column_widths { 4, 4 });
    narrow.insert(std::array<std::uint64_t, 2> { 15, 8 });
    EXPECT_THROW(narrow.insert(std::array<std::uint64_t, 2> { 16, 0 }), std::out_of_range);
    EXPECT_THROW(narrow.insert(std::array<std::uint64_t, 1> { 1 }), std::invalid_argument);
    EXPECT_EQ(narrow.size(), 1U);
    EXPECT_TRUE(narrow.find(std::array<std::uint64_t, 2> { 16, 0 }).is_none());
    EXPECT_EQ(narrow.at(narrow.find(std::array<std::uint64_t, 2> { 15, 8 })).get(1), 8U);
    EXPECT_THROW((void) urd::column_widths(1, 65), std::invalid_argument);
    EXPECT_THROW((void) urd::column_widths(1, 0), std::invalid_argument);
}

// Rows of indices by column widths (a relation's rows of objects): read back as indices.
TEST(RowPools, SchemaRowsOfIndices)
{
    using Index = urd::index<struct SchemaObject>;
    using IndexRow = urd::Store<urd::sequence<Index>, urd::frozen>;
    using Typed = urd::Store<urd::pool<IndexRow>, urd::by_column_widths>;
    static_assert(std::same_as<Typed::representation, urd::repr::RowInterner<IndexRow, false, urd::width::by_column_widths>>);
    auto rows = Typed(urd::column_widths(3, 10));
    const auto row = std::vector<Index> { Index(1), Index(1000), Index(7) };
    const auto handle = rows.insert(row).first;
    EXPECT_TRUE(std::ranges::equal(rows.at(handle).as<Index>(), row));
    EXPECT_EQ(rows.get(handle), IndexRow(row.begin(), row.end()));
    EXPECT_THROW(rows.insert(std::vector<Index> { Index(1), Index(1024), Index(7) }), std::out_of_range);
    EXPECT_TRUE(rows.find(std::vector<Index> { Index(1), Index(1024), Index(7) }).is_none());
    EXPECT_TRUE(rows.find(std::vector<Index> { Index(1), Index(1000) }).is_none());
}

// Some rows of a pool, by their sorted handles: read by position and iterated as the pool's rows, and found by value or
// by a stored row, only where the handle is among them. Over a pool of column widths and over a layered pool, whose
// handles continue its parent's.
TEST(RowPools, SubsetViewsReadAndFindTheirRows)
{
    using Values = urd::Store<urd::sequence<std::uint64_t>, urd::frozen>;
    using Rows = urd::Store<urd::pool<Values>, urd::by_column_widths>;
    auto rows = Rows(urd::column_widths { 8, 16 });
    auto handles = std::vector<Rows::handle_type>();
    for (std::uint64_t at = 0; at < 10; ++at)
        handles.push_back(rows.insert(std::array<std::uint64_t, 2> { at, at * 100 }).first);
    const auto odd = std::vector<Rows::handle_type> { handles[1], handles[3], handles[5] };
    const auto subset = urd::subset_view(rows, std::span<const Rows::handle_type>(odd));
    static_assert(std::ranges::borrowed_range<decltype(subset)> && std::ranges::random_access_range<decltype(subset)>);
    EXPECT_EQ(subset.size(), 3U);
    EXPECT_FALSE(subset.empty());
    EXPECT_EQ(subset[1].get(1), 300U);
    auto firsts = std::vector<std::uint64_t>();
    for (const auto row : subset)
        firsts.push_back(row.get(0));
    EXPECT_EQ(firsts, (std::vector<std::uint64_t> { 1, 3, 5 }));
    EXPECT_TRUE(subset.contains(std::array<std::uint64_t, 2> { 3, 300 }));
    EXPECT_FALSE(subset.contains(std::array<std::uint64_t, 2> { 2, 200 }));  // in the pool, not in the subset
    EXPECT_FALSE(subset.contains(std::array<std::uint64_t, 2> { 3, 301 }));  // in neither
    EXPECT_TRUE(subset.contains(rows.at(handles[5])));
    EXPECT_TRUE(urd::subset_view(rows, std::span<const Rows::handle_type>()).empty());

    using Layered = urd::Store<urd::pool<urd::Store<urd::sequence<std::uint32_t>, urd::frozen>>, urd::monotone, urd::layered>;
    auto parent = Layered();
    const auto low = parent.insert(std::array<std::uint32_t, 2> { 1, 2 }).first;
    auto child = Layered();
    child.layer_over(&parent);
    const auto high = child.insert(std::array<std::uint32_t, 2> { 3, 4 }).first;
    child.insert(std::array<std::uint32_t, 2> { 5, 6 });
    const auto both = std::vector<Layered::handle_type> { low, high };
    const auto layered = urd::subset_view(child, std::span<const Layered::handle_type>(both));
    EXPECT_TRUE(layered.contains(std::array<std::uint32_t, 2> { 1, 2 }));  // the parent's row
    EXPECT_TRUE(layered.contains(std::array<std::uint32_t, 2> { 3, 4 }));
    EXPECT_FALSE(layered.contains(std::array<std::uint32_t, 2> { 5, 6 }));
    EXPECT_EQ(layered[1][0], 3U);
}
