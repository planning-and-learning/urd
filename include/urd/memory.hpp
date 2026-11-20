#pragma once

// The memory a value holds: urd::memory_usage(x), the heap bytes x owns beyond sizeof(x). Each container answers for
// itself, beside its own code (memory_of<R>); its declaration states at most how many words it takes per element (or
// per key of its range, or slot of its capacity: words_per, urd/registry/declaration.hpp), which the benchmarks judge
// against what is allocated (benchmarks/tools/record.py). Its declared words are urd/cost.hpp's.

#include "urd/elements/concepts.hpp"
#include "urd/elements/handle.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
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

/// A variant owns what the alternative it holds owns.
template<typename... Ts>
struct memory_of<std::variant<Ts...>>
{
    static std::size_t bytes(const std::variant<Ts...>& held)
    {
        return std::visit([](const auto& alternative) { return memory_usage(alternative); }, held);
    }
};

/// A type that names the members it owns memory through, as a urd::Members list
/// (`static constexpr auto memory_members() noexcept { return Members<&C::m_a, &C::m_b> {}; }`), owns what they own.
template<DeclaresMemoryMembers T>
struct memory_of<T>
{
    static std::size_t bytes(const T& held)
    {
        return [&held]<auto... Ms>(Members<Ms...> /*members*/) { return (std::size_t { 0 } + ... + memory_usage(held.*Ms)); }(T::memory_members());
    }
};

template<typename T, typename C>
struct builder;

/// A builder owns what its fields own (a child its built or held record, through its handle: nothing of its own).
template<typename T, typename C>
    requires HasFields<builder<T, C>>
struct memory_of<builder<T, C>>
{
    static std::size_t bytes(const builder<T, C>& held)
    {
        auto result = std::size_t { 0 };
        for_each_field<builder<T, C>>([&]<typename F>() { result += memory_usage(held.*F::member); });
        return result;
    }
};

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

/// A record held by its handle (urd::handle): one in a slot of a Store is the Store's, which counts it; one on the heap
/// (make_owned) is its handle's, with what it owns.
template<typename V>
struct memory_of<std::unique_ptr<V, release>>
{
    static std::size_t bytes(const std::unique_ptr<V, release>& held) { return held && is_owned(held) ? sizeof(V) + memory_usage(*held) : 0; }
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
        // Its characters are inside it (the small-string buffer) where their address lies within its own bytes.
        const auto data = std::bit_cast<std::uintptr_t>(static_cast<const void*>(held.data()));
        const auto begin = std::bit_cast<std::uintptr_t>(static_cast<const void*>(&held));
        const bool inside = data >= begin && data - begin < sizeof(held);
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
