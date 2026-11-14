#pragma once

// What urd knows about a container: its properties (container_traits), and the
// questions asked of a container, a Store or a request's spec alike (has). For a registered container they
// are derived from its declaration (urd/registry/derived.hpp); the tags are in urd/tags.hpp.

#include "urd/elements/concepts.hpp"
#include "urd/property_set.hpp"

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

/// A Store: a data structure described by its kind and properties (urd/store.hpp).
template<typename Kind, typename... Tags>
class Store;

/// Whether T is a Store. Known from its name alone, so that what a Store is as an element or a row (its facts, held_as)
/// is read from its request and never by completing it: a Store's container is defined only where the Store is held,
/// not where it is named as another Store's element.
template<typename T>
inline constexpr bool is_store = false;

/// What a container guarantees, by itself, through every operation it offers: derived from its declaration's
/// `provides` (urd/registry/derived.hpp), and checked by its property tests (tests/laws/properties.hpp). A container is
/// declared (urd/registry/declaration.hpp) or urd knows nothing of it: one nobody declared guarantees nothing, and no
/// Store resolves to it.
template<typename R>
struct container_traits : properties<>
{
};

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

}  // namespace urd
