#pragma once

// The memory a value holds: urd::memory_usage(x), the heap bytes x owns beyond sizeof(x). Each container answers for
// itself, beside its own code (memory_of<R>); its declaration states at most how many words it takes per element (or
// per key of its range, or slot of its capacity: words_per, urd/registry/declaration.hpp), which the benchmarks judge
// against what is allocated (benchmarks/record.py). Its declared words are urd/cost.hpp's.

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd
{

/// The heap bytes a value of type T owns beyond sizeof(T), as `static std::size_t bytes(const T&)`. Specialized once
/// per type, beside it: a container (with its elements' own memory), a string, a Store. A type that owns nothing
/// (trivially copyable) answers 0 here; any other type needs its specialization.
template<typename T>
struct memory_of
{
    static_assert(std::is_trivially_copyable_v<T>, "urd::memory_of: a type that may own memory, without a specialization (urd/memory.hpp)");
    static constexpr std::size_t bytes(const T& /*held*/) { return 0; }
};

/// The heap bytes `held` owns beyond sizeof(held): one pass over its elements.
template<typename T>
[[nodiscard]] std::size_t memory_usage(const T& held)
{ return memory_of<std::remove_cv_t<T>>::bytes(held); }

template<typename A, typename B>
struct memory_of<std::pair<A, B>>
{
    static std::size_t bytes(const std::pair<A, B>& held) { return memory_usage(held.first) + memory_usage(held.second); }
};

/// A unique pointer owns its object, and what that owns, where it points at one.
template<typename V, typename Deleter>
struct memory_of<std::unique_ptr<V, Deleter>>
{
    static std::size_t bytes(const std::unique_ptr<V, Deleter>& held) { return held ? sizeof(V) + memory_usage(*held) : 0; }
};

/// An optional owns what its value owns, where it has one.
template<typename V>
struct memory_of<std::optional<V>>
{
    static std::size_t bytes(const std::optional<V>& held) { return held ? memory_usage(*held) : 0; }
};

/// A string owns its characters where they do not fit inside it.
template<typename C, typename Traits, typename Allocator>
struct memory_of<std::basic_string<C, Traits, Allocator>>
{
    static std::size_t bytes(const std::basic_string<C, Traits, Allocator>& held)
    {
        const auto* data = static_cast<const void*>(held.data());
        const auto* begin = static_cast<const void*>(&held);
        const auto* end = static_cast<const void*>(&held + 1);
        const bool inside = !std::less<const void*> {}(data, begin) && std::less<const void*> {}(data, end);
        return inside ? 0 : (held.capacity() + 1) * sizeof(C);
    }
};

namespace detail
{

/// The heap bytes the elements of `range` own, each its memory_usage.
template<std::ranges::range R>
std::size_t held_bytes(const R& range)
{
    std::size_t result = 0;
    for (const auto& each : range)
        result += memory_usage(each);
    return result;
}

}  // namespace detail

template<typename T, typename Allocator>
struct memory_of<std::vector<T, Allocator>>
{
    static std::size_t bytes(const std::vector<T, Allocator>& held) { return (held.capacity() * sizeof(T)) + detail::held_bytes(held); }
};

}  // namespace urd
