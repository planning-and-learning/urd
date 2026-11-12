#pragma once

#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/less.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/select.hpp"
#include "urd/spec.hpp"
#include "urd/store/appending.hpp"
#include "urd/store/assigning.hpp"
#include "urd/store/capacity.hpp"
#include "urd/store/contiguous_data.hpp"
#include "urd/store/ends.hpp"
#include "urd/store/entries.hpp"
#include "urd/store/equality.hpp"
#include "urd/store/grouping.hpp"
#include "urd/store/indexes.hpp"
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
#include "urd/store/store_facts.hpp"
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
#include <memory>
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
    static_assert(false,
                  "urd::Store: this file does not include the headers that define the container its request resolves to. "
                  "#include each header urd::include<\"...\"> names in the mp_list above, in that order "
                  "(docs/RESOLUTION.md names each request's)");
};

/// What a Store<Kind, Tags...> is over: its spec, the request without over<R>, and its representation, R where an
/// over<R> is among the Tags (which must serve the spec), else the container urd chooses for the spec.
template<typename S, typename Forced>
struct representation_over
{
    static_assert(
        []
        {
            for (std::size_t each = 0; each < serving_v<S>.count; ++each)
                if (table_of<typename S::kind>[serving_v<S>.held[each]].id == declaration_of<Forced>::declaration::id)
                    return true;
            return false;
        }(),
        "urd::Store: over<R>: R does not serve the request");
    using type = Forced;
};
template<typename S>
struct representation_over<S, void>
{
    using type = container_for<S>;
};
template<typename Kind, typename... Tags>
struct store_parts
{
    using spec = typename to_spec<Kind, Tags...>::type;
    using representation = typename representation_over<spec, typename over_in<Tags...>::type>::type;
};

}  // namespace detail

/// A data structure described by its kind, with its element types, then its properties, in any order:
///
///     using Ids = urd::Store<urd::set<int>, urd::sorted>;            // a sorted set of ints
///     using Log = urd::Store<urd::sequence<Event>, urd::monotone>;   // a sequence that only grows
///
/// urd picks the representation; Store<…>::representation names it, for tests that pin it. A class template, not an
/// alias, so that a function template deduces what a Store holds from it (`template<typename K> void f(const
/// Store<set<K>>&)`), as it does from the standard's containers; each spelling of a request is its own class, over the
/// same representation.
///
/// It offers the standard interface its requested properties allow (its facets), and container_traits reports the
/// request's properties, not the representation's, so code cannot come to rely on what it never asked for. The Store
/// itself holds only what every Store has, whatever its representation: construction from elements, assign and swap.
/// Everything else is in its facets (include/urd/store/), which its kind lists (urd/kinds.hpp) and its properties
/// switch on: size, iteration, comparison, lookup, positions, inserting, removing.
template<typename Kind, typename... Tags>
class Store :
    public detail::complete_representation<typename detail::store_parts<Kind, Tags...>::representation>,
    public detail::facet_bases<Store<Kind, Tags...>,
                               typename detail::store_parts<Kind, Tags...>::representation,
                               typename detail::store_parts<Kind, Tags...>::spec,
                               detail::facets_of<typename detail::store_parts<Kind, Tags...>::spec::kind>>
{
    using S = typename detail::store_parts<Kind, Tags...>::spec;
    using R = typename detail::store_parts<Kind, Tags...>::representation;
    using types = detail::store_types<S>;

public:
    using representation = R;
    using value_type = typename types::value_type;
    // The other member types come with the facets that use them: size_type with sizing, the iterator types and
    // difference_type with iteration, key_type with lookup, mapped_type with values_by_key and multi_values.

    Store()
        requires(!detail::store_types<S>::needs_an_element)
    = default;

    // Construction stays here, as a facet's constructor could not initialize the Store's representation.

    /// From any elements: a set keeps each once, a sorted sequence sorts them. A nonempty Store needs one.
    template<std::input_iterator InputIt>
        requires std::constructible_from<R, InputIt, InputIt>
    Store(InputIt first, InputIt last) : m_representation(first, last)
    { check_holds_one(); }

    /// A template, so that its constraint is asked only of a list of elements: asked of a Store to copy or move, it
    /// would ask whether the element is constructible while the element's own constructors are being declared (a type
    /// that holds a Store of itself, rule I5).
    template<typename Deferred = void>
        requires std::constructible_from<R, typename std::initializer_list<value_type>::iterator, typename std::initializer_list<value_type>::iterator>
    Store(std::initializer_list<value_type> elements) : Store(elements.begin(), elements.end())
    {
    }

    /// A sequence of `count` elements, value-initialized or copies of `value`, as std::vector(count[, value]).
    explicit Store(std::size_t count)
        requires std::same_as<typename S::kind, sequence_kind> && std::constructible_from<R, std::size_t>
        : m_representation(count)
    { check_holds_one(); }
    Store(std::size_t count, const value_type& value)
        requires std::same_as<typename S::kind, sequence_kind> && std::constructible_from<R, std::size_t, const value_type&>
        : m_representation(count, value)
    { check_holds_one(); }

    /// A Store of urd::bit is built with the bit width every element fits in; a representation that stores elements
    /// whole (which serves the request too) is built as it is, without it.
    explicit Store(urd::bit_width bits)
        requires detail::store_types<S>::built_with_width && std::constructible_from<R, urd::bit_width>
        : m_representation(bits)
    {
    }
    explicit Store(urd::bit_width /*bits*/)
        requires detail::store_types<S>::built_with_width && (!std::constructible_from<R, urd::bit_width>) && std::default_initializable<R>
    {
    }
    /// A Store of urd::by_column_widths is built with the column widths its rows are stored by.
    explicit Store(const urd::column_widths& columns)
        requires detail::store_types<S>::built_with_column_widths && std::constructible_from<R, const urd::column_widths&>
        : m_representation(columns)
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
    friend struct detail::store_access;

    /// A nonempty Store holds an element once built.
    void check_holds_one() const
    {
        if constexpr (detail::store_types<S>::needs_an_element)
            assert(!this->empty() && "urd::Store: a nonempty Store needs an element");
    }

    representation m_representation;
};

/// Read from its name alone: its request (what is_store promises, urd/traits.hpp).
template<typename Kind, typename... Tags>
struct container_traits<Store<Kind, Tags...>> : detail::to_spec<Kind, Tags...>::type
{
};
template<typename Kind, typename... Tags>
inline constexpr bool is_store<Store<Kind, Tags...>> = true;

/// A Store whose contents fix the order its elements come in (a sequence, a sorted or canonical Store) is its elements
/// in order (urd/elements/concepts.hpp): Hash, EqualTo and Less follow from theirs. A hash table's order is not its
/// contents', so an unsorted set or map is not.
template<typename Kind, typename... Tags>
    requires detail::store_types<typename detail::to_spec<Kind, Tags...>::type>::hashed_in_order
inline constexpr bool value_range<Store<Kind, Tags...>> = true;

/// Hashed as its container hashes (urd::Hash), where its contents fix the order its elements come in: sorted,
/// canonical, or a sequence (positions are contents). An unordered Store iterates in an order its contents do not
/// determine, so it has no Hash and cannot be a hashed key: ask for sorted (or canonical) on the inner Store.
template<typename Kind, typename... Tags>
    requires(detail::store_types<typename detail::store_parts<Kind, Tags...>::spec>::hashed_in_order
             && Hashable<typename detail::store_parts<Kind, Tags...>::representation>)
struct Hash<Store<Kind, Tags...>>
{
    hash_t operator()(const Store<Kind, Tags...>& store) const
    { return Hash<typename Store<Kind, Tags...>::representation> {}(detail::store_access::representation(store)); }
};

/// A Store holds what its container holds.
template<typename Kind, typename... Tags>
struct memory_of<Store<Kind, Tags...>>
{
    static std::size_t bytes(const Store<Kind, Tags...>& store) { return memory_usage(detail::store_access::representation(store)); }
};

/// The owner make_unique gives of an element of Store<slots<T>, Tags...> (urd/store/slots.hpp): a std::unique_ptr
/// whose deleter gives the slot back. A shared owner is a std::shared_ptr<T>.
template<typename T, typename... Tags>
using unique_slot_ptr = std::unique_ptr<T, typename Store<slots<T>, Tags...>::releaser>;

}  // namespace urd
