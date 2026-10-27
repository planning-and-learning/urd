#pragma once

// Counts allocations, for the memory properties: tests/allocations.cpp replaces the global operator new and
// delete with these functions, in the test programs that link it.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

// ThreadSanitizer replaces operator new itself, so under it allocations are not counted, and the tests that
// count them skip.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define URD_TEST_SANITIZED_ALLOCATOR 1
#endif
#endif
#if defined(__SANITIZE_THREAD__)
#define URD_TEST_SANITIZED_ALLOCATOR 1
#endif

namespace urd::test
{

#if defined(URD_TEST_SANITIZED_ALLOCATOR)
inline constexpr bool counts_allocations = false;
#else
inline constexpr bool counts_allocations = true;
#endif

// Allocations and deallocations, counted while `counting` is set: the global operator new and delete below
// count them, for the memory properties.
/// Allocations and deallocations, counted while `on` is set by a Counted.
struct Counters
{
    std::atomic<bool> on { false };
    std::atomic<std::int64_t> allocations { 0 };
    std::atomic<std::int64_t> deallocations { 0 };
};

inline Counters& counters()
{
    static auto instance = Counters();
    return instance;
}

/// Counts the allocations and deallocations made during its lifetime.
struct Counted
{
    Counted()
    {
        counters().allocations = 0;
        counters().deallocations = 0;
        counters().on = true;
    }
    Counted(const Counted&) = delete;
    Counted(Counted&&) = delete;
    Counted& operator=(const Counted&) = delete;
    Counted& operator=(Counted&&) = delete;
    ~Counted() { counters().on = false; }
};

/// The allocations counted by the last Counted.
inline std::int64_t allocations() { return counters().allocations; }
inline std::int64_t deallocations() { return counters().deallocations; }

inline void* allocate(std::size_t size, std::size_t alignment)
{
    if (counters().on)
        ++counters().allocations;
    const auto rounded = (size + alignment - 1) / alignment * alignment;
    void* memory =
        alignment <= alignof(std::max_align_t) ? std::malloc(size == 0 ? 1 : size) : std::aligned_alloc(alignment, rounded == 0 ? alignment : rounded);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

inline void deallocate(void* memory) noexcept
{
    if (memory != nullptr && counters().on)
        ++counters().deallocations;
    std::free(memory);
}

}  // namespace urd::test
