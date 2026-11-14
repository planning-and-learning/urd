// The global operator new and delete, counting through urd/testing/allocations.hpp: the source of the opt-in target
// urd::count_allocations. Under ThreadSanitizer, which replaces them itself, they are left out.

#include "urd/testing/allocations.hpp"

#include <cstddef>
#include <new>

#if !defined(URD_TESTING_SANITIZED_ALLOCATOR)
namespace
{

void* allocate_nothrow(std::size_t size, std::size_t alignment) noexcept
{
    try
    {
        return urd::testing::allocate(size, alignment);
    }
    catch (...)
    {
        return nullptr;
    }
}

constexpr auto default_alignment = alignof(std::max_align_t);

}  // namespace

void* operator new(std::size_t size) { return urd::testing::allocate(size, default_alignment); }
void* operator new[](std::size_t size) { return urd::testing::allocate(size, default_alignment); }
void* operator new(std::size_t size, std::align_val_t alignment) { return urd::testing::allocate(size, static_cast<std::size_t>(alignment)); }
void* operator new[](std::size_t size, std::align_val_t alignment) { return urd::testing::allocate(size, static_cast<std::size_t>(alignment)); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return allocate_nothrow(size, default_alignment); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return allocate_nothrow(size, default_alignment); }
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{ return allocate_nothrow(size, static_cast<std::size_t>(alignment)); }
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{ return allocate_nothrow(size, static_cast<std::size_t>(alignment)); }
void operator delete(void* memory) noexcept { urd::testing::deallocate(memory); }
void operator delete[](void* memory) noexcept { urd::testing::deallocate(memory); }
void operator delete(void* memory, std::size_t) noexcept { urd::testing::deallocate(memory); }
void operator delete[](void* memory, std::size_t) noexcept { urd::testing::deallocate(memory); }
void operator delete(void* memory, std::align_val_t) noexcept { urd::testing::deallocate(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { urd::testing::deallocate(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { urd::testing::deallocate(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { urd::testing::deallocate(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { urd::testing::deallocate(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { urd::testing::deallocate(memory); }
void operator delete(void* memory, std::align_val_t, const std::nothrow_t&) noexcept { urd::testing::deallocate(memory); }
void operator delete[](void* memory, std::align_val_t, const std::nothrow_t&) noexcept { urd::testing::deallocate(memory); }
#endif
