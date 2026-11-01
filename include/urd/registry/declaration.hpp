#pragma once

// A container's declaration: the one source of everything urd knows about a container, all of it tags and words. A
// declaration is a plain struct (never a template over element types, so the registry reads it as values and only
// the chosen container's type is ever named):
//
//     struct set_decl
//     {
//         static constexpr std::string_view id = "set";
//         static constexpr auto headers = header_list<1> { "urd/repr/values/std_set.hpp" };
//         using kind = set_kind;                                        // urd/kinds.hpp
//         static constexpr property_set provides = property_set::of({ order::sorted, uniqueness::unique, … });
//         static constexpr role_tags relies_on = relies(traits<role::order, trait::natural_ordered>());
//         static constexpr std::array<tag, 1> gives_elements { memory::reclaims };     // what Stores held as elements get
//         struct operation_costs { … };                                       // urd::container_costs, by operation
//         static constexpr std::string_view description = "…", holds = "…";  // in words
//     };
//
// What it provides is matched against what a request requires, what it relies on against what the request's roles
// promise by role (urd/registry/facts.hpp: role_tags): the type traits (urd::trait) of its elements, its order, its
// rows, and the rows' own tags. What depends on the elements
// beyond the tags is in its description. A wrapper is a template over its part's declaration
// (urd/repr/wrappers/layered_fwd.hpp, …), composing the part's tags, headers, costs and words; a container with
// variants is a template over its flags. Two links tie a declaration to
// types, each specialized once, beside the container: representation_of<Decl, S> (the type for a request, in the
// container's own header, so that a file lacking it lacks the type) and declaration_of<R> (a type's declaration,
// with the facts its template arguments give). urd::container_traits, container_costs, container_synopsis,
// relies_on, parts and defined_in are derived from them (urd/registry/derived.hpp).

#include "urd/kinds.hpp"
#include "urd/registry/facts.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace urd::detail
{

/// The headers a file holding a Store of a container includes: a wrapper's part's first, then its own.
template<std::size_t N>
using header_list = std::array<std::string_view, N>;
template<std::size_t N>
constexpr header_list<N + 1> then(const header_list<N>& part, std::string_view own)
{
    auto result = header_list<N + 1> {};
    for (std::size_t each = 0; each < N; ++each)
        result[each] = part[each];
    result[N] = own;
    return result;
}

/// One entry of a kind's table (urd/registry/declarations.hpp): a declaration as values.
struct registry_entry
{
    std::string_view id;
    std::span<const std::string_view> headers;
    std::string_view kind;  ///< its kind's name (urd/kinds.hpp)
    property_set provides = property_set::roots();
    role_tags relies_on;
    std::span<const tag> gives_elements;
    std::string_view description;
    std::string_view holds;
};
/// Whether every role `relies` holds is one of `roles` (a kind's).
template<typename... Roles>
consteval bool roles_within(role_list<Roles...> /*roles*/, const role_tags& relies)
{
    for (std::size_t each = 0; each < relies.count; ++each)
        if (((relies.roles[each].of != role_v<Roles>) && ...))
            return false;
    return true;
}

template<typename Decl>
constexpr registry_entry entry_of()
{
    static_assert(roles_within(typename Decl::kind::roles {}, Decl::relies_on), "urd: a container relies on a role its kind has not (urd/kinds.hpp, roles)");
    return { Decl::id,        std::span<const std::string_view>(Decl::headers), Decl::kind::name,  Decl::provides,
             Decl::relies_on, std::span<const tag>(Decl::gives_elements),       Decl::description, Decl::holds };
}

/// The container declaration Decl names for spec S; declared only. The container's own header specializes it, so a
/// file names only the containers whose headers it includes.
template<typename Decl, typename S>
struct representation_of;

/// The declaration of container type R, with the roles R's template arguments make (its elements, its comparator or
/// hash): `declaration` and `roles`. Specialized beside each declaration; none for a type urd does not register.
template<typename R>
struct declaration_of
{
};

}  // namespace urd::detail
