#pragma once

// How a request chooses its container: the entry the scan chooses (urd/registry/scan.hpp), then the one type its
// declaration names for the request (representation_of); and where none serves, an error. No container type is named but the one chosen, so what resolving a
// request costs the compiler in instantiations does not grow with the number of containers.

#include "urd/elements/requirements.hpp"
#include "urd/registry/scan.hpp"
#include "urd/repr/core/dependent_false.hpp"
#include "urd/repr/core/numbered.hpp"
#include "urd/spec.hpp"
#include "urd/store/types.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::detail
{

/// What spec S is, as the scan reads it: what it requires, its roles, and the elements it holds; computed once.
template<typename S>
inline constexpr request_shape shape_of = []
{
    using held = held_as<typename S::held_type>;
    return request_shape { S::required, request_roles<S>(), held::properties, held::required };
}();

/// The entry for spec S: its position in its kind's table, or no_entry. `found` is false where none serves.
template<typename S>
struct choose
{
    using kind = typename S::kind;
    static constexpr std::size_t position = first_serving<kind>(shape_of<S>);
    static constexpr bool found = position != no_entry;
    static constexpr std::string_view id = found ? table_of<kind>[position].id : std::string_view();
    /// The headers of the chosen container, a wrapper's part's first.
    static constexpr std::span<const std::string_view> headers = found ? table_of<kind>[position].headers : std::span<const std::string_view>();
};

/// Every declaration serving spec S, in the order they are preferred: what the tables and tests read of a request.
template<typename S>
inline constexpr serving_entries<typename S::kind> serving_v = all_serving<typename S::kind>(shape_of<S>);
template<typename S, std::size_t... I>
auto serving_declarations_of(std::index_sequence<I...> /*serving*/) -> type_list<typename nth<serving_v<S>.at[I], declarations_t<typename S::kind>>::type...>;
template<typename S>
using serving_declarations = decltype(serving_declarations_of<S>(std::make_index_sequence<serving_v<S>.count> {}));

/// The headers of S's entry that this file lacks, as urd::include<"..."> in a type_list, a wrapper's part's first.
template<typename S, std::size_t... I>
consteval auto missing_headers(std::index_sequence<I...> /*headers*/)
{
    constexpr auto entry = table_of<typename S::kind>[choose<S>::position];
    return typename join<std::conditional_t<included<fixed_string<entry.headers[I].size() + 1>(entry.headers[I])>,
                                            type_list<>,
                                            type_list<include<fixed_string<entry.headers[I].size() + 1>(entry.headers[I])>>>...>::type {};
}

/// What a request resolves to where this file lacks headers of its container: a type declared only, whose arguments
/// name the request (S), the container, and every missing header, a wrapper's part first. Naming a Store over it is
/// fine (a Store of Stores names its rows' Stores without holding one); the Store's check fails where one is
/// instantiated (store.hpp, complete_representation).
template<typename S, fixed_string Container, typename Missing>
struct missing_container;

/// The container of a Store whose facets hand out views (records, insertion_positions) is numbered (urd::layer): as
/// those facets' own conditions say, so that the rule has one source.
template<typename S, typename R>
using numbered_t = std::conditional_t<facet_on<void, S, facet::records>() || facet_on<void, S, facet::insertion_positions>(), repr::Numbered<R>, R>;

template<typename S, typename Decl, typename Missing>
struct missing_headers_for
{
    using type = missing_container<S, fixed_string<Decl::id.size() + 1>(Decl::id), Missing>;
};
template<typename S, typename Decl>
struct missing_headers_for<S, Decl, type_list<>>
{
    using type = numbered_t<S, typename representation_of<Decl, S>::type>;
};

/// The container for spec S, reached only where an entry serves it: the type its declaration names, where this file
/// includes the entry's headers; else the error naming them.
template<typename S>
struct chosen_type
{
    using declaration = typename nth<choose<S>::position, declarations_t<typename S::kind>>::type;
    using type = typename missing_headers_for<S, declaration, decltype(missing_headers<S>(std::make_index_sequence<choose<S>::headers.size()> {}))>::type;
};

/// The static assertion a request no container serves fails with; S, in its instantiation, names the request.
template<typename S>
struct no_representation_for
{
    static_assert(repr::dependent_false<S>::value,
                  "urd::Store: no container serves this request: drop or change a tag (urd-search lists each container of its "
                  "kind with what it lacks or needs)");
    /// A stand-in, so that the assertion is the only error.
    using type = std::vector<typename S::element_type>;
};

/// The representation for a request (a to_spec), or where none serves it, the error.
template<typename Request>
struct resolution
{
    using spec = typename Request::type;
    using type = typename std::conditional_t<choose<spec>::found, chosen_type<spec>, no_representation_for<spec>>::type;
};

}  // namespace urd::detail
