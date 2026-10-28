#pragma once

#include "urd/interface.hpp"
#include "urd/properties.hpp"
#include "urd/registry.hpp"
#include "urd/store/capacity.hpp"
#include "urd/store/contiguous_data.hpp"
#include "urd/store/equality.hpp"
#include "urd/store/growth.hpp"
#include "urd/store/iteration.hpp"
#include "urd/store/lookup.hpp"
#include "urd/store/multi_values.hpp"
#include "urd/store/ordered_lookup.hpp"
#include "urd/store/ordering.hpp"
#include "urd/store/positions.hpp"
#include "urd/store/queue_access.hpp"
#include "urd/store/removal.hpp"
#include "urd/store/reverse_iteration.hpp"
#include "urd/store/set_algebra.hpp"
#include "urd/store/sizing.hpp"
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

/// A Store resolved to representation R for spec S. It offers the standard interface its requested properties
/// allow (interface.hpp), and container_traits reports S's properties, not the representation's, so code cannot
/// come to rely on what it never asked for. The Store itself holds only what every Store has, whatever its
/// representation: construction from elements and swap. Everything else is in its facets (include/urd/store/),
/// which its kind and properties switch on: size, iteration, comparison, lookup, positions, inserting, removing.
template<typename R, typename S>
class Store :
    public facet_if<S::sized, facet::sizing<Store<R, S>, R, S>>,
    public facet_if<S::iterable, facet::iteration<Store<R, S>, R, S>>,
    public facet_if<std::equality_comparable<typename S::element_type> && (std::equality_comparable<R> || S::iterable), facet::equality<Store<R, S>, R, S>>,
    public facet_if<compares_in_order<R, S>(), facet::ordering<Store<R, S>, R, S>>,
    public facet_if<store_types<R, S>::keyed, facet::lookup<Store<R, S>, R, S>>,
    public facet_if<has<S, sorted>, facet::ordered_lookup<Store<R, S>, R, S>>,
    public facet_if<store_types<R, S>::ordered, facet::reverse_iteration<Store<R, S>, R, S>>,
    public facet_if<has<S, positional>, facet::positions<Store<R, S>, R, S>>,
    public facet_if<has<S, contiguous>, facet::contiguous_data<Store<R, S>, R, S>>,
    public facet_if<has<S, growable>, facet::capacity<Store<R, S>, R, S>>,
    public facet_if<has<S, growable> && S::iterable, facet::growth<Store<R, S>, R, S>>,
    public facet_if<has<S, shrinkable> && S::iterable, facet::removal<Store<R, S>, R, S>>,
    public facet_if<S::is_map && has<S, unique>, facet::values_by_key<Store<R, S>, R, S>>,
    public facet_if<S::is_map && !has<S, unique>, facet::multi_values<Store<R, S>, R, S>>,
    public facet_if<S::is_set, facet::set_algebra<Store<R, S>, R, S>>,
    public facet_if<S::is_queue, facet::queue_access<Store<R, S>, R, S>>
{
    using types = store_types<R, S>;

public:
    using representation = R;
    using value_type = typename types::value_type;
    // The other member types come with the facets that use them: size_type with sizing, the iterator types and
    // difference_type with iteration, key_type, lookup_type and mapped_type with lookup.

    Store()
        requires(!has<S, nonempty>)
    = default;

    // Construction stays here, as a facet's constructor could not initialize the Store's representation.

    /// From any elements: a set keeps each once, a sorted sequence sorts them. A nonempty Store needs one.
    template<std::input_iterator InputIt>
    Store(InputIt first, InputIt last) : m_representation(first, last)
    {
        if constexpr (has<S, nonempty>)
            assert(!this->empty() && "urd::Store: a nonempty Store needs an element");
    }

    Store(std::initializer_list<value_type> elements) : Store(elements.begin(), elements.end()) {}

    void swap(Store& other) noexcept
    {
        using std::swap;
        swap(m_representation, other.m_representation);
    }
    friend void swap(Store& lhs, Store& rhs) noexcept { lhs.swap(rhs); }

private:
    friend struct store_access;

    representation m_representation;
};

template<typename... Ps>
struct resolve
{
    using spec_type = typename to_spec<Ps...>::type;
    using type = Store<typename choose<spec_type>::type, spec_type>;
};

}  // namespace detail

/// A data structure described by its properties. List what the data is and how it is used, in any order:
///
///     using Ids = urd::Store<urd::key<int>, urd::sorted>;            // a sorted set of ints
///     using Log = urd::Store<urd::value<Event>, urd::monotone>;      // a sequence that only grows
///
/// urd picks the representation; Store<…>::representation names it, for tests that pin it.
template<typename... Ps>
using Store = typename detail::resolve<Ps...>::type;

/// A Store has the properties it was asked for, not those of its representation.
template<typename R, typename S>
struct container_traits<detail::Store<R, S>> : S
{
};

}  // namespace urd
