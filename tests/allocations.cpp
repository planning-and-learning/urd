// The global operator new and delete, counting through tests/allocations.hpp. Under ThreadSanitizer, which
// replaces them itself, they are left out.

#include "allocations.hpp"

#include <cstddef>
#include <new>

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
