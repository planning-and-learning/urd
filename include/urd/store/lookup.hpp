#pragma once

#include "urd/cost.hpp"
#include "urd/store/types.hpp"

#include <concepts>
#include <iterator>
#include <type_traits>
#include <ranges>
#include <utility>

namespace urd::detail::facet
{

/// Kept by value or key: finding elements, as the standard's associative containers do.
template<typename Derived, typename R, typename S>
class lookup
{
    using types = store_types<S>;
    using const_iterator_type = typename iterator_types<R, S>::const_iterator;
    using iterator_type = typename iterator_types<R, S>::iterator;

public:
    /// What lookups take: a map's key, else an element, as key_type is for the standard's sets.
    using key_type = typename types::lookup_type;

    [[nodiscard]] typename iterator_types<R, S>::const_iterator find(const typename types::lookup_type& element) const
    { return representation().find(element); }
    /// In a map, the entry found with its value changeable, as the standard's maps give it.
    [[nodiscard]] iterator_type find(const typename types::lookup_type& element)
        requires(!std::same_as<iterator_type, const_iterator_type>)
    { return representation().find(element); }
    [[nodiscard]] typename types::size_type count(const typename types::lookup_type& element) const { return representation().count(element); }
    [[nodiscard]] bool contains(const typename types::lookup_type& element) const { return representation().contains(element); }
    /// Lookups by another form of a key: forwarded as they are, so what the Store's Hash and EqualTo (or its order)
    /// compare with a key decides what K may be, as with the standard's associative containers (a key with its hash,
    /// a record's identity, a view of a key). A K they do not take does not compile.
    template<typename K>
        requires(!std::same_as<std::remove_cvref_t<K>, key_type>)
    [[nodiscard]] const_iterator_type find(const K& key) const
    { return representation().find(key); }
    template<typename K>
        requires(!std::same_as<std::remove_cvref_t<K>, key_type>) && (!std::same_as<iterator_type, const_iterator_type>)
    [[nodiscard]] iterator_type find(const K& key)
    { return representation().find(key); }
    template<typename K>
        requires(!std::same_as<std::remove_cvref_t<K>, key_type>)
    [[nodiscard]] typename types::size_type count(const K& key) const
    { return representation().count(key); }
    template<typename K>
        requires(!std::same_as<std::remove_cvref_t<K>, key_type>)
    [[nodiscard]] bool contains(const K& key) const
    { return representation().contains(key); }
    /// The elements equal to `element` (in a map, with that key), as a pair of iterators.
    [[nodiscard]] std::pair<const_iterator_type, const_iterator_type> equal_range(const typename types::lookup_type& element) const
    { return representation().equal_range(element); }

private:
    [[nodiscard]] const R& representation() const { return store_access::of<Derived>(*this); }
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

template<>
struct facet_operations<facet::lookup>
{
    using offered = operation_list<operation::lookup>;
};

}  // namespace urd::detail
