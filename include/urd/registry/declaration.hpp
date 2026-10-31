#pragma once

// A container's declaration: the one source of everything urd knows about a container. A declaration is a plain
// struct (never a template over element types, so the registry reads it as values and only the chosen container's
// type is ever named):
//
//     struct std_set_decl
//     {
//         static constexpr std::string_view id = "set";
//         static constexpr auto headers = header_list { "urd/repr/std_set.hpp" };
//         static constexpr offered offers(const request_features& f);   // what it offers these elements, or nothing
//         struct operation_costs { … };                                   // urd::container_costs, by operation
//         static constexpr std::string_view description = "…", holds = "…";  // in words
//     };
//
// A wrapper is a template over its part's declaration (urd/repr/layered_fwd.hpp, …), composing the part's offer,
// headers, costs and words; a container with variants is a template over its flags. Two links tie a declaration to
// types, each specialized once, beside the container: representation_of<Decl, S> (the type for a request, in the
// container's own header, so that a file lacking it lacks the type) and declaration_of<R> (a type's declaration,
// with the facts its template arguments give). urd::container_traits, container_costs, container_synopsis,
// element_traits, relies_on, parts and defined_in are derived from them (urd/registry/derived.hpp).

#include "urd/registry/facts.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace urd::detail
{

/// What a container offers a request with given element facts: what its Stores are, what it guarantees, what it
/// relies on the request to promise, and what it gives the Stores it holds as elements. Every field is meaningful for
/// every container: offer_of fills the usual values, and a declaration changes only what is its own.
struct offer
{
    urd::kind kind = urd::kind::sequence;
    property_set properties = property_set::roots();
    property_set relies_on = property_set::roots();
    /// What Stores held as its elements (a map's values, else its elements) get: by default their own request, and
    /// their storage returned when they are destroyed, as erase and clear do.
    property_set nested = property_set::roots();

    constexpr offer relying_on(std::initializer_list<tag> promised) const
    {
        auto result = *this;
        for (const auto each : promised)
            result.relies_on.put(each);
        return result;
    }
    constexpr offer giving_nested(const property_set& given) const
    {
        auto result = *this;
        result.nested = given;
        return result;
    }
};

/// What a declaration answers: its offer, or nothing where it does not take these elements or this order.
using offered = std::optional<offer>;

/// The offer of a container of kind `of` with properties `held`, for a request with facts `f`: nested Stores get
/// their own request, and give their storage back.
constexpr offer offer_of(urd::kind of, const property_set& held, const request_features& f)
{
    const auto& elements = of == urd::kind::map ? f.mapped : f.element;
    return { .kind = of,
             .properties = held,
             .relies_on = property_set::roots(),
             .nested = elements.store ? with(elements.store_properties, { tag::reclaims }) : property_set::roots() };
}

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

/// One entry of the registry's table: a declaration as values.
struct registry_entry
{
    std::string_view id;
    std::span<const std::string_view> headers;
    offered (*offers)(const request_features&) = nullptr;
};
template<typename Decl>
constexpr registry_entry entry_of()
{ return { Decl::id, std::span<const std::string_view>(Decl::headers), &Decl::offers }; }

/// The container declaration Decl names for spec S; declared only. The container's own header specializes it, so a
/// file names only the containers whose headers it includes.
template<typename Decl, typename S>
struct representation_of;

/// The declaration of container type R, with the facts R's template arguments give (its elements, its comparator or
/// hash): `declaration` and `features`. Specialized beside each declaration; none for a type urd does not register.
template<typename R>
struct declaration_of
{
};

}  // namespace urd::detail
