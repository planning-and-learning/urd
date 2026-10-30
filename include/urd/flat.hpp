#pragma once

// Records stored flat: the cista containers a record is written with, under urd's names, their hash and equality,
// and how a view resolves them. Include it where flat records are defined; the pools that store them are the
// records family, urd/use/records.hpp, included where such a Store is held.

#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/view.hpp"

#include <cista/containers/string.h>
#include <cista/containers/variant.h>
#include <cista/containers/vector.h>
#include <cstddef>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

/// The sequence, text and variant of a record stored flat: cista's offset containers, which serialize as a whole.
template<typename T>
using flat_vector = ::cista::offset::vector<T>;
using flat_string = ::cista::offset::string;
template<typename... Ts>
using flat_variant = ::cista::offset::variant<Ts...>;

namespace detail
{

/// The alternative a flat variant holds, as the type its apply takes.
template<typename... Ts>
[[nodiscard]] constexpr auto held_index(const flat_variant<Ts...>& held) noexcept
{
    return static_cast<typename flat_variant<Ts...>::index_t>(held.index());
}

}  // namespace detail

/// The same byte hash as Hash<std::string>, so equal text hashes alike across representations.
template<>
struct Hash<flat_string>
{
    hash_t operator()(const flat_string& text) const noexcept { return hashing::hash_bytes(text.data(), text.size()); }
};
template<>
struct EqualTo<flat_string>
{
    bool operator()(const flat_string& lhs, const flat_string& rhs) const noexcept { return equal_range(lhs, rhs); }
};

template<typename T, template<typename> typename Ptr, bool IndexPointers, typename SizeType, class Allocator>
struct Hash<::cista::basic_vector<T, Ptr, IndexPointers, SizeType, Allocator>>
{
    hash_t operator()(const ::cista::basic_vector<T, Ptr, IndexPointers, SizeType, Allocator>& elements) const
        requires Hashable<T>
    {
        return urd::hash_range(elements);
    }
};
template<typename T, template<typename> typename Ptr, bool IndexPointers, typename SizeType, class Allocator>
struct EqualTo<::cista::basic_vector<T, Ptr, IndexPointers, SizeType, Allocator>>
{
    bool operator()(const ::cista::basic_vector<T, Ptr, IndexPointers, SizeType, Allocator>& lhs,
                    const ::cista::basic_vector<T, Ptr, IndexPointers, SizeType, Allocator>& rhs) const
        requires EqualityComparableByEqualTo<T>
    {
        return equal_range(lhs, rhs);
    }
};

template<typename... Ts>
struct Hash<flat_variant<Ts...>>
{
    hash_t operator()(const flat_variant<Ts...>& held) const
        requires(Hashable<Ts> && ...)
    {
        hash_t seed = held.index();
        if (held.valid())
            flat_variant<Ts...>::apply([&seed](const auto& alternative) { urd::hash_combine(seed, alternative); }, detail::held_index(held), held);
        return seed;
    }
};
template<typename... Ts>
struct EqualTo<flat_variant<Ts...>>
{
    bool operator()(const flat_variant<Ts...>& lhs, const flat_variant<Ts...>& rhs) const
        requires(EqualityComparableByEqualTo<Ts> && ...)
    {
        if (lhs.valid() != rhs.valid())
            return false;
        if (!lhs.valid())
            return true;
        if (lhs.index() != rhs.index())
            return false;
        return flat_variant<Ts...>::apply([]<typename U>(const U& a, const U& b) { return EqualTo<std::remove_cvref_t<U>> {}(a, b); },
                                          detail::held_index(lhs),
                                          lhs,
                                          rhs);
    }
};

/// A view resolves a flat variant as it does a std::variant: the variant of its alternatives resolved.
template<typename... Ts>
struct variant_traits<flat_variant<Ts...>>
{
    template<typename C>
    using resolved = std::variant<resolved_t<Ts, C>...>;
    template<typename F>
    static decltype(auto) visit(F&& function, const flat_variant<Ts...>& held)
    {
        if (!held.valid())
            throw std::bad_variant_access();
        return flat_variant<Ts...>::apply([&function](const auto& alternative) -> decltype(auto) { return function(alternative); },
                                          detail::held_index(held),
                                          held);
    }
};

}  // namespace urd
