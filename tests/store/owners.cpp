// A slots Store's elements owned by the standard smart pointers: std::unique_ptr and std::shared_ptr give the slot
// back when the (last) owner goes, the element kept with its storage for the next acquire, and none of it allocates
// once as many owners have lived at once as will.

#include "../allocations.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/store.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

namespace
{

/// A scratch buffer that keeps its storage between uses, and lets go of what it refers to when released.
struct Scratch
{
    std::vector<int> buffer;
    int released = 0;

    void on_pool_release() noexcept
    {
        buffer.clear();
        ++released;
    }
};

using Pool = urd::Store<urd::slots<Scratch>>;
using ConcurrentPool = urd::Store<urd::slots<Scratch>, urd::concurrent>;

}  // namespace

// The last owner gives the slot back, the release hook run; the next acquire gets the same element, its buffer kept.
TEST(Owners, ReleaseKeepsTheElementAndItsStorage)
{
    auto pool = Pool();
    const Scratch* first = nullptr;
    {
        auto owner = pool.acquire_unique();
        owner->buffer.assign(100, 1);
        first = owner.get();
        EXPECT_EQ(pool.size(), 1U);
    }
    EXPECT_EQ(pool.size(), 0U);
    auto again = pool.acquire_shared();
    EXPECT_EQ(again.get(), first);
    EXPECT_EQ(again->released, 1);
    EXPECT_TRUE(again->buffer.empty());
    EXPECT_GE(again->buffer.capacity(), 100U);
    auto copy = again;
    again.reset();
    EXPECT_EQ(pool.size(), 1U);
    copy.reset();
    EXPECT_EQ(pool.size(), 0U);
}

// Once warm, acquiring and releasing owners allocates nothing: the slots are reused, and so are shared_ptr's
// control blocks.
TEST(Owners, NoAllocationOnceWarm)
{
    if (!urd::test::counts_allocations)
        GTEST_SKIP() << "allocations are not counted here";
    auto pool = Pool();
    const auto round = [&pool]
    {
        auto shared = std::vector<std::shared_ptr<Scratch>>();
        auto unique = std::vector<urd::unique_slot_ptr<Scratch>>();
        shared.reserve(8);
        unique.reserve(8);
        for (int each = 0; each < 8; ++each)
        {
            shared.push_back(pool.acquire_shared());
            unique.push_back(pool.acquire_unique());
            shared.back()->buffer.assign(16, each);
            unique.back()->buffer.assign(16, each);
        }
    };
    round();
    {
        const auto counted = urd::test::Counted();
        for (int again = 0; again < 10; ++again)
            round();
    }
    // The two vectors of owners are the only allocations: two per round.
    EXPECT_EQ(urd::test::allocations(), 20);
}

// Threads acquire and release shared owners at once: every slot goes back.
TEST(Owners, ConcurrentSharedOwners)
{
    auto pool = ConcurrentPool();
    auto workers = std::vector<std::thread>();
    for (int thread = 0; thread < 4; ++thread)
        workers.emplace_back(
            [&pool]
            {
                for (int each = 0; each < 2000; ++each)
                {
                    auto owner = pool.acquire_shared();
                    auto copy = owner;
                    copy->buffer.push_back(each);
                }
            });
    for (auto& worker : workers)
        worker.join();
    EXPECT_EQ(pool.size(), 0U);
}

namespace
{

/// Owns a pool and hands out its slots keeping itself alive; records, when it goes, how many slots were still in use.
struct PoolOwner : std::enable_shared_from_this<PoolOwner>
{
    Pool pool;
    std::size_t* in_use_at_destruction;

    explicit PoolOwner(std::size_t* in_use) : in_use_at_destruction(in_use) {}
    PoolOwner(const PoolOwner&) = delete;
    PoolOwner& operator=(const PoolOwner&) = delete;
    ~PoolOwner() { *in_use_at_destruction = pool.size(); }
};

constexpr auto alive = std::size_t(-1);

}  // namespace

// Owners holding `keep` outlive the Store's owner: it goes exactly with the last of them, after its slot is back.
TEST(Owners, KeepHoldsTheStoresOwner)
{
    auto in_use = alive;
    auto owner = std::make_shared<PoolOwner>(&in_use);
    auto shared = owner->pool.acquire_shared(owner->shared_from_this());
    auto copy = shared;
    auto unique = owner->pool.acquire_unique(owner->shared_from_this());
    owner->pool.acquire_shared().reset();  // both shared forms take their control blocks from one pool
    owner.reset();
    shared.reset();
    EXPECT_EQ(in_use, alive);
    copy.reset();
    EXPECT_EQ(in_use, alive);
    unique.reset();
    EXPECT_EQ(in_use, 0U);  // destroyed now, with every slot back
}

TEST(Owners, KeepHoldsTheStoresOwnerLastShared)
{
    auto in_use = alive;
    auto owner = std::make_shared<PoolOwner>(&in_use);
    auto unique = owner->pool.acquire_unique(owner);
    auto shared = owner->pool.acquire_shared(owner);
    owner.reset();
    unique.reset();
    EXPECT_EQ(in_use, alive);
    shared.reset();
    EXPECT_EQ(in_use, 0U);  // the control block is back in the Store's pool before the Store goes
}

// `keep` is the last owner of the Store, held by the control block's allocator, not its deleter: the slot goes back
// with the last shared owner, the Store only with the control block, after the block is back in the Store's pool (a
// use after free otherwise, which the sanitizers see).
TEST(Owners, KeepOutlivesTheControlBlock)
{
    auto in_use = alive;
    auto owner = std::make_shared<PoolOwner>(&in_use);
    auto shared = owner->pool.acquire_shared(owner);
    auto weak = std::weak_ptr<Scratch>(shared);
    owner.reset();
    shared.reset();
    EXPECT_EQ(in_use, alive);  // the slot is back, the control block still held by `weak`, and so the Store
    weak.reset();
    EXPECT_EQ(in_use, 0U);
}

TEST(Owners, MovedFromPoolStillSharesItsElements)
{
    // The control blocks' pool is made on first use: a moved-from pool makes a new one. (Owners must all be back before a pool
    // moves: each returns its slot to the pool it came from.)
    auto pool = Pool();
    pool.acquire_shared().reset();
    auto moved = std::move(pool);
    auto again = pool.acquire_shared();  // NOLINT(bugprone-use-after-move): a moved-from pool is valid and empty
    EXPECT_NE(again, nullptr);
    again.reset();
    EXPECT_EQ(pool.size(), 0U);  // NOLINT(bugprone-use-after-move)
    auto from_moved = moved.acquire_shared();
    EXPECT_EQ(moved.size(), 1U);
}
