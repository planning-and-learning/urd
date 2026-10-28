// Pools: rows identified by the handles insert gives. Each pool is checked against the rows it was given through
// random inserts and clears: equal rows get equal handles, and every handle gives its row back.

#include "names.hpp"
#include "urd/store.hpp"
#include "verify.hpp"

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <gtest/gtest.h>
#include <random>
#include <thread>
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
                                  Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing, urd::concurrent>>;
TYPED_TEST_SUITE(Pools, Containers, urd::test::Names);

}  // namespace

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
                    auto mine = std::vector<urd::repr::SlotHandle>();
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
