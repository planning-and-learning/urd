#pragma once

// How a request chooses its representation: the first row serving it, found by scanning the rows as values
// (urd/registry/rows.hpp, row_choice), then the one container type that row names (representation_of); and where none
// serves, what to drop (the diagnostics, the same scan with one tag removed per try). No container type is named but
// the one chosen, so what resolving a request costs the compiler does not grow with the number of containers.

#include "urd/elements.hpp"
#include "urd/properties.hpp"
#include "urd/registry/features.hpp"
#include "urd/repr/core/dependent_false.hpp"
#include "urd/traits.hpp"

#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::detail
{

/// The row for spec S: the first that serves it, scanned as values (urd/registry/rows.hpp). `found` is false where none
/// does.
template<typename S>
struct choose
{
    static constexpr const registry_row* row = row_choice<S>();
    static constexpr bool found = row != nullptr;
    static constexpr std::string_view id = found ? row->id : std::string_view();
};

/// The I-th header of S's row as an urd::include<"..."> where this file lacks it, else nothing.
template<typename S, std::size_t I>
consteval auto missing_header()
{
    constexpr std::string_view path = choose<S>::row->headers[I];
    if constexpr (path.empty())
        return type_list<> {};
    else
    {
        constexpr auto header = fixed_string<path.size() + 1>(path);
        if constexpr (included<header>)
            return type_list<> {};
        else
            return type_list<include<header>> {};
    }
}
template<typename A, typename B>
struct appended;
template<typename... As, typename... Bs>
struct appended<type_list<As...>, type_list<Bs...>>
{
    using type = type_list<As..., Bs...>;
};

/// What a request resolves to where this file lacks headers of its container: a type declared only, whose arguments
/// name the request (S), the container, and every missing header, a wrapper's part first. Naming a Store over it is
/// fine (a Store of Stores names its rows' Stores without holding one); the Store's check fails where one is
/// instantiated (store.hpp, complete_representation).
template<typename S, fixed_string Container, typename Missing>
struct missing_container;

template<typename S, fixed_string Container, typename Missing>
struct missing_headers_for
{
    using type = missing_container<S, Container, Missing>;
};
template<typename S, fixed_string Container>
struct missing_headers_for<S, Container, type_list<>>
{
    using type = typename representation_of<Container, S>::type;
};

/// The container for spec S, reached only where a row serves it: its type, named by the row's id, where this file
/// includes the row's headers; else the error naming them.
template<typename S>
struct chosen_type
{
    using type = typename missing_headers_for<S,
                                              fixed_string<choose<S>::id.size() + 1>(choose<S>::id),
                                              typename appended<decltype(missing_header<S, 0>()), decltype(missing_header<S, 1>())>::type>::type;
};

/// The tags of an unservable request whose removal, one at a time, would make it servable: what to drop, or what the
/// pool lacks a representation for. Named in the error for such a request.
template<tag... Ts>
struct culprits
{
};

/// The static assertion a request no representation serves fails with. Its arguments say why: the spec, the culprit
/// tags, and whether the key (a sequence's or queue's element) is hashable, orderable and indexable, for when no tag
/// is the culprit but the type is.
template<typename S, typename Culprits, bool KeyHashable, bool KeyOrderable, bool KeyIndexable>
struct no_representation_for
{
    static_assert(repr::dependent_false<S>::value,
                  "urd::Store: no representation provides this combination of properties. The culprits<...> above name the tags "
                  "whose removal, one at a time, would make it servable: drop one, or add a representation for them. No "
                  "culprit named: the key type lacks what the request needs (hashable, orderable, indexable: the booleans "
                  "above), or two of the tags clash");
    /// A stand-in, so that the assertion is the only error.
    using type = std::vector<typename S::element_type>;
};

template<typename Request, std::size_t... I>
consteval listed_tags culprits_of(std::index_sequence<I...> /*positions*/)
{
    auto result = listed_tags {};
    ((choose<typename Request::template without<Request::stated_tags.tags[I]>>::found ? void(result.tags[result.count++] = Request::stated_tags.tags[I]) :
                                                                                        void()),
     ...);
    return result;
}
template<typename Request, listed_tags Found, std::size_t... I>
consteval auto diagnosis(std::index_sequence<I...> /*positions*/)
{
    using spec = typename Request::type;
    using probe = std::conditional_t<std::is_void_v<typename spec::key_type>, typename spec::element_type, typename spec::key_type>;
    return std::type_identity<no_representation_for<spec, culprits<Found.tags[I]...>, hashable<probe>, orderable<probe>, indexable<probe>>> {};
}
/// The diagnosis of a request (a to_spec) no representation serves; its type is reached only then.
template<typename Request>
struct unservable
{
    static constexpr listed_tags found = culprits_of<Request>(std::make_index_sequence<Request::stated_tags.count> {});
    using type = typename decltype(diagnosis<Request, found>(std::make_index_sequence<found.count> {}))::type::type;
};

/// The representation for a request (a to_spec), or where none serves it, the diagnosis: the culprit probes run only
/// then.
template<typename Request>
struct resolution
{
    using spec = typename Request::type;
    using type = typename std::conditional_t<choose<spec>::found, chosen_type<spec>, unservable<Request>>::type;
};

}  // namespace urd::detail
