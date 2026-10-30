#pragma once

#include "urd/index.hpp"
#include "urd/interface.hpp"
#include "urd/properties.hpp"
#include "urd/registry.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/semantics/less.hpp"
#include "urd/store/capacity.hpp"
#include "urd/store/changes.hpp"
#include "urd/store/contiguous_data.hpp"
#include "urd/store/denials.hpp"
#include "urd/store/entries.hpp"
#include "urd/store/equality.hpp"
#include "urd/store/grouping.hpp"
#include "urd/store/growth.hpp"
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
#include "urd/store/reverse_iteration.hpp"
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

/// The first base of a Store: the representation must be defined here. The registry only declares representations;
/// the header of R's family, urd/use/<family>.hpp (defined_in<R>), defines them and marks that it was included.
/// Checked by the marker, never by completing R, and for a wrapper's parts first, so that one compile names every
/// header missing. Both compilers print the header's path in the failed condition: urd::include<"urd/use/...">.
template<typename List>
struct complete_parts;
template<typename R>
struct complete_representation : complete_parts<typename parts<R>::type>
{
    static_assert(included<defined_in<R>> || repr::dependent_false<include<defined_in<R>>>::value,
                  "urd::Store: the representation's family is not included here: #include the header that urd::include<\"...\"> names "
                  "in this error (urd/use/all.hpp has every family)");
};
template<typename... Ps>
struct complete_parts<type_list<Ps...>> : complete_representation<Ps>...
{
};

/// A Store resolved to representation R for spec S. It offers the standard interface its requested properties
/// allow (interface.hpp), and container_traits reports S's properties, not the representation's, so code cannot
/// come to rely on what it never asked for. The Store itself holds only what every Store has, whatever its
/// representation: construction from elements and swap. Everything else is in its facets (include/urd/store/),
/// which its kind and properties switch on: size, iteration, comparison, lookup, positions, inserting, removing.
template<typename R, typename S>
class Store :
    public complete_representation<R>,
    public facet_if<facet_switches<R, S>::on(facet_id::sizing), facet::sizing<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::iteration), facet::iteration<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::equality), facet::equality<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::ordering), facet::ordering<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::lookup), facet::lookup<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::ordered_lookup), facet::ordered_lookup<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::reverse_iteration), facet::reverse_iteration<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::positions), facet::positions<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::contiguous_data), facet::contiguous_data<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::capacity), facet::capacity<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::growth), facet::growth<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::removal), facet::removal<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::values_by_key), facet::values_by_key<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::multi_values), facet::multi_values<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::set_algebra), facet::set_algebra<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::queue_access), facet::queue_access<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::scopes), facet::scopes<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::layers), facet::layers<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::changes), facet::changes<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::pool), facet::pool<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::records), facet::records<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::slots), facet::slots<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::grouping), facet::grouping<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::entries), facet::entries<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::inverse), facet::inverse<Store<R, S>, R, S>>,
    public facet_if<facet_switches<R, S>::on(facet_id::shape), facet::shape<Store<R, S>, R, S>>,
    // Where a member name is in none of the facets above, a denial stands in: a call then names the property that
    // rules the operation out (store/denials.hpp).
    public denial<R, S>::insert,
    public denial<R, S>::emplace,
    public denial<R, S>::appending,
    public denial<R, S>::pop_back,
    public denial<R, S>::erase,
    public denial<R, S>::clear,
    public denial<R, S>::lookup,
    public denial<R, S>::ordered_lookup,
    public denial<R, S>::iteration,
    public denial<R, S>::reverse_iteration,
    public denial<R, S>::subscript,
    public denial<R, S>::ends,
    public denial<R, S>::position,
    public denial<R, S>::data,
    public denial<R, S>::reserve,
    public denial<R, S>::sizing,
    public denial<R, S>::values_by_key,
    public denial<R, S>::update,
    public denial<R, S>::merge,
    public denial<R, S>::values,
    public denial<R, S>::keys,
    public denial<R, S>::queue,
    public denial<R, S>::scopes,
    public denial<R, S>::layers,
    public denial<R, S>::changes,
    public denial<R, S>::view,
    public denial<R, S>::rows,
    public denial<R, S>::acquire,
    public denial<R, S>::grouping,
    public denial<R, S>::reset,
    public denial<R, S>::set_algebra
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

    /// Replaces the contents by what a Store built from these elements would hold: the one way a frozen Store
    /// changes. A representation that rebuilds in place (assign) keeps its storage; any other is built anew.
    template<std::input_iterator InputIt>
    void assign(InputIt first, InputIt last)
    {
        if constexpr (requires { m_representation.assign(first, last); })
            m_representation.assign(first, last);
        else
            m_representation = representation(first, last);
        if constexpr (has<S, nonempty>)
            assert(!this->empty() && "urd::Store: a nonempty Store needs an element");
    }

    /// Rows of these lengths, empty: a fixed_shape Store's only shape. A nonempty one needs a row (a row of length 0
    /// is a row).
    explicit Store(const row_lengths& lengths)
        requires has<S, fixed_shape>
        : m_representation(lengths.lengths)
    {
        if constexpr (has<S, nonempty>)
            assert(!this->empty() && "urd::Store: a nonempty fixed_shape Store needs a row");
    }
    /// Rows of rows, empty, at any depth: for each row, the shape of its rows (a std::vector of row lengths for rows
    /// of rows, a std::vector of those one level up).
    template<typename Shape>
        requires(has<S, fixed_shape> && !std::same_as<Shape, row_lengths> && std::constructible_from<R, const Shape&>)
    explicit Store(const Shape& rows) : m_representation(rows)
    {
        if constexpr (has<S, nonempty>)
            assert(!this->empty() && "urd::Store: a nonempty fixed_shape Store needs a row");
    }

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
    using type = Store<typename resolution<to_spec<Ps...>>::type, spec_type>;
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

/// A Store is ordered by its values where its representation is.
template<typename R, typename S>
inline constexpr bool orders_by_value<detail::Store<R, S>> = orders_by_value<R>;

/// A Store has the properties it was asked for, not those of its representation.
template<typename R, typename S>
struct container_traits<detail::Store<R, S>> : S
{
};

/// The representations whose family has no header yet are defined by this header (traits.hpp, defined_in).
template<>
struct include<"urd/store.hpp">
{
};

// A Store's semantics as an element of another Store, Hash, EqualTo and Less, are decided by its request S alone:
// whether a Store hashes, compares or orders is asked while the outer Store is resolved, where the inner's
// representation is named but not yet defined. Only the bodies touch R, and they are instantiated where the Store
// is held. (semantics/concepts.hpp excludes Stores from the identity concepts for the same reason.)

/// Hashed by its contents where they fix the order its elements come in: sorted, canonical, or a sequence (positions
/// are contents). An unordered Store iterates in an order its contents do not determine, so it has no Hash and cannot
/// be a hashed key: ask for sorted (or canonical) on the inner Store. Equality is the facet's (equality.hpp), which
/// compares in that order, so equal Stores hash equally.
template<typename R, typename S>
    requires(S::iterable && (has<S, sorted> || has<S, canonical> || S::is_sequence) && Hashable<typename S::element_type>)
struct Hash<detail::Store<R, S>>
{
    hash_t operator()(const detail::Store<R, S>& store) const
    {
        // The representation's own hash where it has one (a bitset hashes its words).
        if constexpr (Hashable<R>)
            return Hash<R> {}(detail::store_access::representation(store));
        else
            return urd::hash_range(store);
    }
};

/// Equal contents, as the Store's == compares them (equality.hpp): an iterable Store of comparable elements.
template<typename R, typename S>
    requires(S::iterable && EqualityComparableByEqualTo<typename S::element_type>)
struct EqualTo<detail::Store<R, S>>
{
    bool operator()(const detail::Store<R, S>& lhs, const detail::Store<R, S>& rhs) const { return lhs == rhs; }
};

/// Ordered lexicographically by urd::Less over the elements, in the order the request fixes (sorted,
/// insertion_ordered, a sequence): a strict weak order agreeing with EqualTo, as the standard's ordered containers'.
template<typename R, typename S>
    requires(S::iterable && (has<S, sorted> || has<S, insertion_ordered> || S::is_sequence) && OrderedByLess<typename S::element_type>)
struct Less<detail::Store<R, S>>
{
    bool operator()(const detail::Store<R, S>& lhs, const detail::Store<R, S>& rhs) const
    {
        return std::ranges::lexicographical_compare(lhs, rhs, Less<typename S::element_type> {});
    }
};

}  // namespace urd
