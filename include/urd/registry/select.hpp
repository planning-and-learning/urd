#pragma once

// How a request chooses its container: the entry the scan chooses (urd/registry/scan.hpp), then the one type its
// declaration names for the request (representation_of); and where none serves, an error. No container type is named but the one chosen, so what resolving a
// request costs the compiler in instantiations does not grow with the number of containers.

#include "urd/elements/requirements.hpp"
#include "urd/registry/scan.hpp"
#include "urd/spec.hpp"
#include "urd/store/types.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <array>
#include <boost/mp11.hpp>
#include <cstddef>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::detail
{

/// The facts about roles that some container of Kind relies on: all that choosing among them reads of a request's
/// data.
template<typename Kind>
inline constexpr property_set relied_on_v = []
{
    auto result = property_set::roots();
    for (const auto& each : table_of<Kind>)
        result = with(result, facts_where(each.relies_on, true));
    return result;
}();
/// Asks the traits a container of Kind relies on, and no other: what a request's data is asked. A sequence's
/// containers rely on none of their elements', so a sequence may be held of an incomplete type.
template<typename Kind>
struct traits_relied_on_by
{
    template<typename Role, typename Trait>
    static consteval bool asks()
    { return relied_on_v<Kind>.has(trait_tag<Trait>.about(role_v<Role>)); }
};

/// What spec S is, as the scan reads it: what it requires, its roles, and the elements it holds; computed once.
template<typename S>
inline constexpr request_shape shape_of = []
{
    using held = held_as<typename S::held_type>;
    return request_shape { S::required, request_roles<S, traits_relied_on_by<typename S::kind>>(), held::properties, held::required };
}();

/// Every entry of Kind serving a request of this shape, scanned once per shape: requests over different element types
/// often share one.
template<typename Kind, request_shape Shape>
inline constexpr serving_entries<Kind> serving_for = all_serving<Kind>(Shape);
/// Every entry serving spec S, in the order they are preferred: what the tables and tests read of a request.
template<typename S>
inline constexpr const serving_entries<typename S::kind>& serving_v = serving_for<typename S::kind, shape_of<S>>;
template<typename S, std::size_t... I>
auto serving_declarations_of(std::index_sequence<I...> /*serving*/)
    -> boost::mp11::mp_list<boost::mp11::mp_at_c<declarations_t<typename S::kind>, serving_v<S>.held[I]>...>;
template<typename S>
using serving_declarations = decltype(serving_declarations_of<S>(std::make_index_sequence<serving_v<S>.count> {}));

/// The entry chosen for spec S: the first serving it (its position in its kind's table), or no_entry. `found` is false
/// where none serves.
template<typename S>
struct choose
{
    using kind = typename S::kind;
    static constexpr std::size_t position = serving_v<S>.count != 0 ? serving_v<S>.held[0] : no_entry;
    static constexpr bool found = position != no_entry;
    static constexpr std::string_view id = found ? table_of<kind>[position].id : std::string_view();
    /// The headers of the chosen container, a wrapper's part's first.
    static constexpr std::span<const std::string_view> headers = found ? table_of<kind>[position].headers : std::span<const std::string_view>();
};

/// What a request resolves to where this file lacks headers of its container: a type declared only, whose arguments
/// name the request (S), the container, and every missing header, a wrapper's part first. Naming a Store over it is
/// fine (a Store of Stores names its rows' Stores without holding one); the Store's check fails where one is
/// instantiated (store.hpp, complete_representation).
template<typename S, fixed_string Container, typename Missing>
struct missing_container;

/// The headers of S's chosen container that this file lacks, as urd::include<"..."> in a boost::mp11::mp_list, a wrapper's
/// part's first.
template<typename S, std::size_t... I>
consteval auto missing_headers(std::index_sequence<I...> /*headers*/)
{
    constexpr auto headers = choose<S>::headers;
    return boost::mp11::mp_append<boost::mp11::mp_list<>,
                                  std::conditional_t<included<fixed_string<headers[I].size() + 1>(headers[I])>,
                                                     boost::mp11::mp_list<>,
                                                     boost::mp11::mp_list<include<fixed_string<headers[I].size() + 1>(headers[I])>>>...> {};
}

/// The container declaration Decl names for spec S, numbered where the Store hands out indices (numbered_t), where
/// this file includes its headers (Missing is empty); else the missing_container naming them.
template<typename S, typename Decl, typename Missing>
struct container_of_declaration
{
    using type = missing_container<S, fixed_string<Decl::id.size() + 1>(Decl::id), Missing>;
};
template<typename S, typename Decl>
struct container_of_declaration<S, Decl, boost::mp11::mp_list<>>
{
    using type = numbered_t<S, typename representation_of<Decl, S>::type>;
};

/// The container for spec S, reached only where an entry serves it.
template<typename S>
struct chosen_type
{
    using declaration = boost::mp11::mp_at_c<declarations_t<typename S::kind>, choose<S>::position>;
    using type = typename container_of_declaration<S, declaration, decltype(missing_headers<S>(std::make_index_sequence<choose<S>::headers.size()> {}))>::type;
};

/// The static assertion a request no container serves fails with; S, in its instantiation, names the request.
// ponytail: a fixed message, as C++20 allows; a C++26 computed one could spell each entry's shortfall_of here.
template<typename S>
struct no_representation_for
{
    static_assert(false,
                  "urd::Store: no container serves this request: drop or change a tag (urd-search's why_not names, for a "
                  "container, the tags it lacks and the promises the request must make)");
    /// A stand-in, so that the assertion is the only error.
    using type = std::vector<typename S::element_type>;
};

/// The container for spec S, or where none serves it, the error.
template<typename S>
using container_for = typename std::conditional_t<choose<S>::found, chosen_type<S>, no_representation_for<S>>::type;

}  // namespace urd::detail
