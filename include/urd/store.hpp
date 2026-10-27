#pragma once

#include "urd/interface.hpp"
#include "urd/properties.hpp"
#include "urd/registry.hpp"
#include "urd/store/contiguous_data.hpp"
#include "urd/store/growth.hpp"
#include "urd/store/lookup.hpp"
#include "urd/store/ordered_lookup.hpp"
#include "urd/store/positions.hpp"
#include "urd/store/removal.hpp"
#include "urd/store/reverse_iteration.hpp"
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
/// come to rely on what it never asked for. The Store itself holds what every Store has; its facets
/// (include/urd/store/) hold what properties switch on.
template<typename R, typename S>
class Store :
    public facet_if<store_types<R, S>::keyed, facet::lookup<Store<R, S>, R, S>>,
    public facet_if<has<S, sorted>, facet::ordered_lookup<Store<R, S>, R, S>>,
    public facet_if<store_types<R, S>::ordered, facet::reverse_iteration<Store<R, S>, R, S>>,
    public facet_if<S::is_sequence, facet::positions<Store<R, S>, R, S>>,
    public facet_if<has<S, contiguous>, facet::contiguous_data<Store<R, S>, R, S>>,
    public facet_if<has<S, growable>, facet::growth<Store<R, S>, R, S>>,
    public facet_if<has<S, shrinkable>, facet::removal<Store<R, S>, R, S>>,
    public facet_if<S::is_map && has<S, unique>, facet::values_by_key<Store<R, S>, R, S>>
{
    using types = store_types<R, S>;

public:
    using representation = R;
    using value_type = typename types::value_type;
    using key_type = typename types::key_type;
    using mapped_type = typename types::mapped_type;
    using lookup_type = typename types::lookup_type;
    using size_type = typename types::size_type;
    using difference_type = typename types::difference_type;
    using const_reference = typename types::const_reference;
    using reference = typename types::reference;
    using const_iterator = typename types::const_iterator;
    using iterator = typename types::iterator;
    static constexpr workload work = S::work;

    Store()
        requires(!has<S, nonempty>)
    = default;

    /// From any elements: a set keeps each once, a sorted sequence sorts them. A nonempty Store needs one.
    template<std::input_iterator InputIt>
    Store(InputIt first, InputIt last) : m_representation(first, last)
    { assert((!has<S, nonempty> || !empty()) && "urd::Store: a nonempty Store needs an element"); }

    Store(std::initializer_list<value_type> elements) : Store(elements.begin(), elements.end()) {}

    /// From elements claimed to have the properties (a sequence's order and uniqueness, any Store's
    /// nonemptiness), so the representation need not establish them; Debug builds check the claim.
    [[nodiscard]] static Store assume(std::vector<value_type> elements)
    {
        if constexpr (S::is_sequence)
            assert(holds<S>(elements) && "urd::Store::assume: the contents lack a claimed property");
        else
            assert((!has<S, nonempty> || !elements.empty()) && "urd::Store::assume: the contents lack a claimed property");
        return Store(std::make_move_iterator(elements.begin()), std::make_move_iterator(elements.end()));
    }

    [[nodiscard]] const_iterator begin() const { return m_representation.begin(); }
    [[nodiscard]] const_iterator end() const { return m_representation.end(); }
    // Mutable iteration stays here, not in a facet: it overloads the const begin() and end() above, and a
    // base's overload would be hidden by them.
    [[nodiscard]] iterator begin()
        requires types::writable
    { return m_representation.begin(); }
    [[nodiscard]] iterator end()
        requires types::writable
    { return m_representation.end(); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend() const { return end(); }
    [[nodiscard]] size_type size() const { return std::ranges::size(m_representation); }
    [[nodiscard]] bool empty() const { return size() == 0; }

    /// Hints how many elements will be inserted; does nothing where the representation cannot reserve.
    void reserve(size_type count)
        requires has<S, growable>
    {
        if constexpr (requires { m_representation.reserve(count); })
            m_representation.reserve(count);
    }

    void swap(Store& other) noexcept
    {
        using std::swap;
        swap(m_representation, other.m_representation);
    }
    friend void swap(Store& lhs, Store& rhs) noexcept { lhs.swap(rhs); }

    [[nodiscard]] friend bool operator==(const Store& lhs, const Store& rhs)
        requires std::equality_comparable<value_type>
    {
        if constexpr (std::equality_comparable<representation>)
            return lhs.m_representation == rhs.m_representation;
        else
            return std::ranges::equal(lhs, rhs);
    }
    /// In order, as the standard's ordered containers compare.
    [[nodiscard]] friend auto operator<=>(const Store& lhs, const Store& rhs)
        requires(types::ordered && std::three_way_comparable<value_type>)
    { return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end()); }

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
