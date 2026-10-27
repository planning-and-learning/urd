#pragma once

// Counts allocations, for the memory properties: this header replaces the global operator new and delete, so a
// test program includes it from one file only.

#include <atomic>
#include <cstddef>
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
inline std::atomic<bool> counting { false };
inline std::atomic<long> allocations { 0 };
inline std::atomic<long> deallocations { 0 };

/// Counts the allocations and deallocations made during its lifetime.
struct Counted
{
    Counted()
    {
        allocations = 0;
        deallocations = 0;
        counting = true;
    }
    Counted(const Counted&) = delete;
    Counted& operator=(const Counted&) = delete;
    ~Counted() { counting = false; }
};

inline void* allocate(std::size_t size, std::size_t alignment)
{
    if (counting)
        ++allocations;
    const auto rounded = (size + alignment - 1) / alignment * alignment;
    void* memory =
        alignment <= alignof(std::max_align_t) ? std::malloc(size == 0 ? 1 : size) : std::aligned_alloc(alignment, rounded == 0 ? alignment : rounded);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

inline void deallocate(void* memory) noexcept
{
    if (memory != nullptr && counting)
        ++deallocations;
    std::free(memory);
}

}  // namespace urd::test

#if !defined(URD_TEST_SANITIZED_ALLOCATOR)
void* operator new(std::size_t size) { return urd::test::allocate(size, alignof(std::max_align_t)); }
void* operator new[](std::size_t size) { return urd::test::allocate(size, alignof(std::max_align_t)); }
void* operator new(std::size_t size, std::align_val_t alignment) { return urd::test::allocate(size, static_cast<std::size_t>(alignment)); }
void* operator new[](std::size_t size, std::align_val_t alignment) { return urd::test::allocate(size, static_cast<std::size_t>(alignment)); }
void operator delete(void* memory) noexcept { urd::test::deallocate(memory); }
void operator delete[](void* memory) noexcept { urd::test::deallocate(memory); }
void operator delete(void* memory, std::size_t) noexcept { urd::test::deallocate(memory); }
void operator delete[](void* memory, std::size_t) noexcept { urd::test::deallocate(memory); }
void operator delete(void* memory, std::align_val_t) noexcept { urd::test::deallocate(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { urd::test::deallocate(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { urd::test::deallocate(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { urd::test::deallocate(memory); }
#endif
