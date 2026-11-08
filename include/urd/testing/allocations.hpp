#pragma once

// Counts allocations, for tests: a test program opts in by linking urd::count_allocations, whose allocations.cpp
// replaces the global operator new and delete with these functions. A Counted counts its own thread's only.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

// ThreadSanitizer replaces operator new itself, so under it allocations are not counted, and tests that count them
// skip.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define URD_TESTING_SANITIZED_ALLOCATOR 1
#endif
#endif
#if defined(__SANITIZE_THREAD__)
#define URD_TESTING_SANITIZED_ALLOCATOR 1
#endif

namespace urd::testing
{

#if defined(URD_TESTING_SANITIZED_ALLOCATOR)
inline constexpr bool counts_allocations = false;
#else
inline constexpr bool counts_allocations = true;
#endif

/// Allocations and deallocations counted by a Counted.
struct Counts
{
    std::int64_t allocations = 0;
    std::int64_t deallocations = 0;
};

namespace detail
{

/// This thread's counts, and whether a Counted counts them.
struct Counters
{
    bool on = false;
    Counts counts;
};

inline Counters& counters() noexcept
{
    thread_local constinit Counters instance {};
    return instance;
}

}  // namespace detail

/// Counts this thread's allocations and deallocations during its lifetime, from zero.
struct Counted
{
    Counted() noexcept { detail::counters() = { .on = true, .counts = {} }; }
    Counted(const Counted&) = delete;
    Counted(Counted&&) = delete;
    Counted& operator=(const Counted&) = delete;
    Counted& operator=(Counted&&) = delete;
    ~Counted() { detail::counters().on = false; }
};

/// What this thread's last (or current) Counted counted.
inline Counts counts() noexcept { return detail::counters().counts; }

inline void* allocate(std::size_t size, std::size_t alignment)
{
    if (detail::counters().on)
        ++detail::counters().counts.allocations;
    const auto rounded = (size + alignment - 1) / alignment * alignment;
    void* memory =
        alignment <= alignof(std::max_align_t) ? std::malloc(size == 0 ? 1 : size) : std::aligned_alloc(alignment, rounded == 0 ? alignment : rounded);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

inline void deallocate(void* memory) noexcept
{
    if (memory != nullptr && detail::counters().on)
        ++detail::counters().counts.deallocations;
    std::free(memory);
}

}  // namespace urd::testing
