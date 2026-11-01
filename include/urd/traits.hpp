#pragma once

// What urd knows about a container: its properties (container_traits), what it relies on (relies_on), and the
// questions asked of a container, a Store or a request's spec alike (has, satisfies). For a registered container they
// are derived from its declaration (urd/registry/derived.hpp); the tags are in urd/tags.hpp.

#include "urd/property_set.hpp"
#include "urd/semantics/concepts.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace urd
{

/// What a container guarantees, by itself, through every operation it offers. A registered container's is derived
/// from its declaration's `provides` (urd/registry/derived.hpp), and its property tests check it
/// (tests/properties.hpp). A container outside the registry may specialize it:
///
///     template<> struct urd::container_traits<MyList> : urd::properties<urd::sorted, urd::unique> {};
///
/// A container nobody declared guarantees nothing, and no Store resolves to it.
template<typename R>
struct container_traits : properties<>
{
    static constexpr bool declared = false;
};

/// The promises representation R relies on, among those only a request makes (monotone_priority, small, …): it
/// serves only requests that make them. A bucket queue pops in order whatever is pushed, but is fast only where pushes
/// never go below the last popped. None by default.
template<typename R>
inline constexpr detail::property_set relies_on = detail::property_set::roots();

/// Whether map representation R orders its entries by their values too (order_by<P> with P(key, value)): its values
/// then change only through operations that put the entry back in order (update, insert_or_assign), never through a
/// reference. False by default.
template<typename R>
inline constexpr bool orders_by_value = false;

namespace detail
{

template<typename X>
struct traits_of
{
    using type = container_traits<std::remove_cvref_t<X>>;
};
template<typename X>
    requires(!is_store<X>) && requires { X::urd_properties; }
struct traits_of<X>
{
    using type = X;
};

}  // namespace detail

/// The properties of X: X itself when it lists them (a request's spec), else its container_traits.
template<typename X>
using traits_t = typename detail::traits_of<X>::type;

/// Whether X (a container, a Store, or a request's spec) has every property in Tags.
template<typename X, typename... Tags>
    requires(detail::a_tag<Tags> && ...)
inline constexpr bool has = (traits_t<X>::urd_properties.has(Tags::value) && ...);

/// Whether Traits places elements by their value or key, as the standard's associative containers (set,
/// flat_multiset, map, multimap) do; a sequence (vector) keeps an element where it was put.
template<typename Traits>
inline constexpr bool by_value = has<Traits, sorted> || has<Traits, unique> || has<Traits, multi>;

/// Whether Have provides, in every category a container declares, at least what Want asks for.
template<typename Have, typename Want>
inline constexpr bool satisfies = detail::satisfies(traits_t<Have>::urd_properties, traits_t<Want>::urd_properties);

}  // namespace urd
