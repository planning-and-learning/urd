// Pools of plain rows: a pool of frozen sequences of trivially copyable elements stores its rows flat (no vector per
// row, no offsets while all have one width), hands them out as spans that stay where they are, and looks a row up by
// the span of its elements.

#include "urd/repr/pools/pools.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

using Row = urd::Store<urd::sequence<std::uint32_t>, urd::frozen>;
using Rows = urd::Store<urd::pool<Row>>;
using ConcurrentRows = urd::Store<urd::pool<Row>, urd::concurrent>;

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
    EXPECT_TRUE(std::ranges::equal(rows.view(rows.find(b)), b));
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
            views.push_back(rows.view(handle));
        }
    }
    ASSERT_EQ(rows.size(), reference.size());
    for (std::uint32_t at = 0; at < reference.size(); ++at)
    {
        const auto handle = urd::index<Row>(at);
        EXPECT_TRUE(std::ranges::equal(rows.view(handle), reference[at]));
        EXPECT_EQ(rows.view(handle).data(), views[at].data());
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
            const auto row = rows.view(urd::index<Row>(static_cast<std::uint32_t>(at)));
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

// Threads interning overlapping rows at once: each row one handle, the same for every thread.
TEST(RowPools, ConcurrentInsertsAgree)
{
    auto rows = ConcurrentRows();
    constexpr int threads = 4;
    constexpr std::uint32_t distinct = 2000;
    auto seen = std::vector<std::vector<std::uint32_t>>(threads, std::vector<std::uint32_t>(distinct));
    auto workers = std::vector<std::thread>();
    for (int thread = 0; thread < threads; ++thread)
        workers.emplace_back(
            [&, thread]
            {
                for (std::uint32_t each = 0; each < distinct; ++each)
                {
                    const auto value = (each * 7 + static_cast<std::uint32_t>(thread)) % distinct;
                    const auto row = std::array<std::uint32_t, 2> { value, value + 1 };
                    const auto handle = rows.insert(std::span<const std::uint32_t>(row)).first;
                    seen[static_cast<std::size_t>(thread)][value] = handle.value;
                    EXPECT_TRUE(std::ranges::equal(rows.view(handle), row));
                }
            });
    for (auto& worker : workers)
        worker.join();
    EXPECT_EQ(rows.size(), distinct);
    for (int thread = 1; thread < threads; ++thread)
        EXPECT_EQ(seen[static_cast<std::size_t>(thread)], seen[0]);
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

// A pool of rows of indices whose request promises urd::bit stores each element in the bit width it is built with:
// the bit row interner, whose rows are ranges decoding their elements; the same rows, handles and lookups as before.
TEST(RowPools, BitRowsPackElements)
{
    using Index = urd::index<struct BitObject>;
    using IndexRow = urd::Store<urd::sequence<Index>, urd::frozen>;
    using Packed = urd::Store<urd::pool<IndexRow>, urd::bit>;
    using ConcurrentPacked = urd::Store<urd::pool<IndexRow>, urd::bit, urd::concurrent>;
    static_assert(std::same_as<Packed::representation, urd::repr::RowInterner<IndexRow, false, true>>);
    static_assert(std::same_as<ConcurrentPacked::representation, urd::repr::RowInterner<IndexRow, true, true>>);
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
            const auto row = rows.view(handle);
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
