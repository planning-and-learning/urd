#pragma once

// Copied from yggdrasil (include/yggdrasil/core/spin_mutex.hpp): SpinMutex and its backoff; names moved from ygg to
// urd::repr. A lock of one byte, for locks per element.

#include <atomic>
#include <cstdint>
#include <sched.h>

namespace urd::repr
{

namespace detail
{
/// Pauses 1, 2, 4, ..., 16 times, then yields to the scheduler.
class SpinBackoff
{
    static constexpr std::int32_t LOOPS_BEFORE_YIELD = 16;
    std::int32_t m_count = 1;

public:
    void reset() noexcept { m_count = 1; }
    void pause() noexcept
    {
        if (m_count <= LOOPS_BEFORE_YIELD)
        {
            for (auto i = m_count; i > 0; --i)
            {
#if defined(__x86_64__) || defined(__i386__)
                __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__ARM_ARCH_7A__)
                __asm__ __volatile__("isb sy" ::: "memory");
#else
                sched_yield();
#endif
            }
            m_count *= 2;
        }
        else
        {
            sched_yield();
        }
    }
};
}  // namespace detail

// oneTBB's spin_mutex and spin_rw_mutex with their state words. oneTBB's headers pull in <immintrin.h> and <thread>
// for the backoff (about 100 MB in every includer), so the backoff uses compiler builtins.

/// The algorithm and state word of oneTBB's spin_mutex: unfair test-and-test-and-set.
class SpinMutex
{
    std::atomic<bool> m_flag { false };

public:
    SpinMutex() noexcept = default;
    SpinMutex(const SpinMutex&) = delete;
    SpinMutex& operator=(const SpinMutex&) = delete;
    SpinMutex(SpinMutex&&) = delete;
    SpinMutex& operator=(SpinMutex&&) = delete;
    ~SpinMutex() = default;

    void lock() noexcept
    {
        for (detail::SpinBackoff backoff; m_flag.load(std::memory_order_relaxed) || m_flag.exchange(true);)
            backoff.pause();
    }

    bool try_lock() noexcept { return !m_flag.load(std::memory_order_relaxed) && !m_flag.exchange(true); }

    void unlock() noexcept { m_flag.store(false, std::memory_order_release); }
};

}  // namespace urd::repr
