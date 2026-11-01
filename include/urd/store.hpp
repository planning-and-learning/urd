#pragma once

#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/less.hpp"
#include "urd/registry/select.hpp"
#include "urd/spec.hpp"
#include "urd/store/appending.hpp"
#include "urd/store/capacity.hpp"
#include "urd/store/changes.hpp"
#include "urd/store/construction.hpp"
#include "urd/store/contiguous_data.hpp"
#include "urd/store/ends.hpp"
#include "urd/store/entries.hpp"
#include "urd/store/equality.hpp"
#include "urd/store/grouping.hpp"
#include "urd/store/insertion.hpp"
#include "urd/store/inverse.hpp"
#include "urd/store/iteration.hpp"
#include "urd/store/layers.hpp"
#include "urd/store/lookup.hpp"
#include "urd/store/multi_values.hpp"
#include "urd/store/ordered_lookup.hpp"
#include "urd/store/ordering.hpp"
#include "urd/store/pool.hpp"
#include "urd/store/positions.hpp"
#include "urd/store/queue_access.hpp"
#include "urd/store/records.hpp"
#include "urd/store/removal.hpp"
#include "urd/store/scopes.hpp"
#include "urd/store/set_algebra.hpp"
#include "urd/store/shape.hpp"
#include "urd/store/sizing.hpp"
#include "urd/store/slots.hpp"
#include "urd/store/types.hpp"
#include "urd/store/values_by_key.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd
{

namespace detail
{

/// The first base of a Store: its container's headers are included here. Where they are not, the request resolved
/// to a missing_container (urd/registry/select.hpp), whose arguments name the request, the container and the headers
/// to include; the error is this one, at the first Store instantiated.
template<typename R>
struct complete_representation
{
};
template<typename S, fixed_string Container, typename Missing>
struct complete_representation<missing_container<S, Container, Missing>>
{
    static_assert(repr::dependent_false<S>::value,
                  "urd::Store: this file does not include the headers that define the container its request resolves to. "
                  "#include each header urd::include<\"...\"> names in the type_list above, in that order "
                  "(docs/RESOLUTION.md names each request's)");
};

/// A Store resolved to representation R for spec S. It offers the standard interface its requested properties
/// allow (its facets), and container_traits reports S's properties, not the representation's, so code cannot
/// come to rely on what it never asked for. The Store itself holds only what every Store has, whatever its
/// representation: construction from elements, assign and swap. Everything else is in its facets (include/urd/store/),
/// which its kind lists (urd/kinds.hpp) and its properties switch on: size, iteration, comparison, lookup, positions, inserting, removing.
template<typename R, typename S>
class Store : public complete_representation<R>, public facet_bases<Store<R, S>, R, S, facets_of<typename S::kind>>
{
    using types = store_types<S>;

public:
    using representation = R;
    using value_type = typename types::value_type;
    // The other member types come with the facets that use them: size_type with sizing, the iterator types and
    // difference_type with iteration, key_type and lookup_type with lookup, mapped_type with values_by_key and multi_values.

    Store()
        requires(!store_types<S>::needs_an_element)
    = default;

    // Construction stays here, as a facet's constructor could not initialize the Store's representation.

    /// From any elements: a set keeps each once, a sorted sequence sorts them. A nonempty Store needs one.
    template<std::input_iterator InputIt>
        requires std::constructible_from<R, InputIt, InputIt>
    Store(InputIt first, InputIt last) : m_representation(first, last)
    { check_holds_one(); }

    Store(std::initializer_list<value_type> elements)
        requires std::constructible_from<R, typename std::initializer_list<value_type>::iterator, typename std::initializer_list<value_type>::iterator>
        : Store(elements.begin(), elements.end())
    {
    }

    /// A Store that hands out indices for views is numbered (urd::layer): built from its number first, then from
    /// whatever else builds its container (nothing, or its elements). Who builds it gives the number.
    template<typename... Args>
        requires std::constructible_from<R, urd::layer, Args...>
    explicit Store(urd::layer number, Args&&... args) : m_representation(number, std::forward<Args>(args)...)
    { check_holds_one(); }
    Store(urd::layer number, std::initializer_list<value_type> elements)
        requires std::
            constructible_from<R, urd::layer, typename std::initializer_list<value_type>::iterator, typename std::initializer_list<value_type>::iterator>
        : Store(number, elements.begin(), elements.end())
    {
    }

    /// Replaces the contents by what a Store built from these elements would hold: the one way a frozen Store
    /// changes. A representation that rebuilds in place (assign) keeps its storage; any other is built anew.
    template<std::input_iterator InputIt>
    void assign(InputIt first, InputIt last)
    {
        if constexpr (requires { m_representation.assign(first, last); })
            m_representation.assign(first, last);
        else
            m_representation = representation(first, last);
        check_holds_one();
    }

    /// From what its kind builds it from besides elements (urd/kinds.hpp, builds_from): a fixed_shape sequence from
    /// its rows' lengths (urd::row_lengths), or for rows of rows, each row's shape. A nonempty one needs an element (a
    /// row of length 0 is a row).
    template<typename From>
        requires(S::kind::template builds_from<S, From> && std::constructible_from<R, const From&>)
    explicit Store(const From& source) : m_representation(source)
    { check_holds_one(); }

    void swap(Store& other) noexcept
    {
        using std::swap;
        swap(m_representation, other.m_representation);
    }
    friend void swap(Store& lhs, Store& rhs) noexcept { lhs.swap(rhs); }

private:
    friend struct store_access;

    /// A nonempty Store holds an element once built.
    void check_holds_one() const
    {
        if constexpr (store_types<S>::needs_an_element)
            assert(!this->empty() && "urd::Store: a nonempty Store needs an element");
    }

    representation m_representation;
};

/// The Store for spec S: over the container chosen for it.
template<typename S>
using store_for = Store<container_for<S>, S>;

}  // namespace detail

/// A data structure described by its kind, with its element types, then its properties, in any order:
///
///     using Ids = urd::Store<urd::set<int>, urd::sorted>;            // a sorted set of ints
///     using Log = urd::Store<urd::sequence<Event>, urd::monotone>;   // a sequence that only grows
///
/// urd picks the representation; Store<…>::representation names it, for tests that pin it.
template<typename Kind, typename... Tags>
using Store = detail::store_for<typename detail::to_spec<Kind, Tags...>::type>;

/// A Store has the properties it was asked for, not those of its representation.
template<typename R, typename S>
struct container_traits<detail::Store<R, S>> : S
{
};

}  // namespace urd
