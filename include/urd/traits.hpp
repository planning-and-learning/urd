#pragma once

// What a container declares about itself: its properties (container_traits),
// and the header that defines it (defined_in). The tags it declares them with are in urd/tags.hpp.

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

/// What a container guarantees, by itself, through every operation it offers. Specialize it for a container
/// to declare its properties; a registered container's property tests check the declaration (tests/properties.hpp):
///
///     template<> struct urd::container_traits<MyList> : urd::properties_of<urd::sorted, urd::shrinkable> {};
///
/// A container nobody declared guarantees nothing, and no Store resolves to it.
template<typename R>
struct container_traits : properties_of<>
{
    static constexpr bool declared = false;
};

namespace detail
{

/// A string literal as a template argument: a header's path, or the reason a denied operation is denied.
template<std::size_t N>
struct fixed_string
{
    std::array<char, N> text {};

    consteval fixed_string(const char (&literal)[N]) { std::ranges::copy(literal, text.begin()); }
    /// From a string computed at compile time (a row's id), N its size plus the terminating zero.
    consteval explicit fixed_string(std::string_view computed) { std::ranges::copy(computed, text.begin()); }

    [[nodiscard]] constexpr const char* c_str() const noexcept { return text.data(); }
};

}  // namespace detail

// The registry declares representations and does not define them: a file pays for the definition of each container
// its Stores resolve to, by including that container's own header (urd/repr/<container>.hpp), and for nothing else.
// That a header is included is told by a marker it defines, never by looking at a representation (completing a
// wrapper of an undefined part is an error, not a false); a Store whose container's header is absent names it in its
// error (store.hpp).

/// Declared only; each container's header urd/repr/X.hpp defines include<"urd/repr/X.hpp">.
template<detail::fixed_string Header>
struct include;
/// Whether the header's marker is defined here.
template<detail::fixed_string Header>
inline constexpr bool included = requires { sizeof(include<Header>); };
/// The header that defines representation R, specialized beside R's declaration. A representation that names none
/// has no header; resolution_table --check rejects that for anything the registry chooses.
template<typename R>
inline constexpr auto defined_in = detail::fixed_string("urd/repr/<unnamed>.hpp");

namespace detail
{

/// The elements of a row type, read without completing it: a Store's by its request (where it iterates: a pool or
/// queue has no elements to hand out), else what iterating a range gives. Undefined otherwise, so a constraint
/// naming it is simply not satisfied.
template<typename Row>
struct row_element
{
};
template<typename Row>
    requires(!is_store<Row>) && std::ranges::range<Row>
struct row_element<Row>
{
    using type = std::ranges::range_value_t<Row>;
};
template<typename Row>
    requires(is_store<Row> && container_traits<Row>::iterable)
struct row_element<Row>
{
    using type = typename container_traits<Row>::element_type;
};
template<typename Row>
using row_element_t = typename row_element<Row>::type;

// Three questions about an element type that a trait or rule may ask, answered for a Store from its request and
// never by completing it (a template argument evaluates every operand, so `!is_store<V> && std::trait<V>` would
// still complete V: these are specialized instead).

/// Whether V can fill a slot that is emptied and filled again: default-constructible and movable. Every Store is
/// movable, and one not asked to be nonempty or given a fixed shape has an empty value.
template<typename V>
inline constexpr bool slot_value = std::default_initializable<V> && std::movable<V>;
template<typename R, typename S>
inline constexpr bool slot_value<Store<R, S>> = !S::urd_properties.has(tag::nonempty) && !S::urd_properties.has(tag::fixed_shape);

/// Whether destroying a V does nothing. A Store owns storage, so never.
template<typename V>
inline constexpr bool trivially_destructible = std::is_trivially_destructible_v<V>;
template<typename R, typename S>
inline constexpr bool trivially_destructible<Store<R, S>> = false;

/// Whether a V is its own block of bytes (copied by memcpy). A Store owns storage, so never.
template<typename V>
inline constexpr bool trivially_copyable = std::is_trivially_copyable_v<V>;
template<typename R, typename S>
inline constexpr bool trivially_copyable<Store<R, S>> = false;

/// Whether V is iterated as a range of elements. A Store is where its request says so (a set, map or sequence).
template<typename V>
inline constexpr bool iterates = std::ranges::input_range<V>;
template<typename R, typename S>
inline constexpr bool iterates<Store<R, S>> = S::iterable;

}  // namespace detail

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

/// What a container guarantees about its elements where they are Stores: the properties a nested Store's rows
/// have as the container holds them. By default their own, except memory: destroying a row frees what it owns,
/// as erase and clear do. A container that keeps rows, or stores them flattened, specializes it. The primary
/// only marks that nothing was declared: the default is computed by the registry from the request's row type
/// (row_traits), so that selection reads nothing of a candidate.
template<typename R>
struct element_traits
{
    static constexpr bool defaulted = true;
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

/// Whether Traits places elements by their value or key, as the standard's associative containers (set,
/// flat_multiset, map, multimap) do; a sequence (vector) keeps an element where it was put.
template<typename Traits>
inline constexpr bool by_value = has<Traits, sorted> || has<Traits, unique> || has<Traits, multi>;

/// Whether Have provides, in every category a container declares, at least what Want asks for.
template<typename Have, typename Want>
inline constexpr bool satisfies = detail::satisfies(traits_t<Have>::urd_properties, traits_t<Want>::urd_properties);

}  // namespace urd
