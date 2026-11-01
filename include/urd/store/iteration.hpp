#pragma once

#include "urd/store/types.hpp"

namespace urd::detail::facet
{

/// The elements in turn, changeable in place where the Store's elements are: a range urd reads (equality, hashing, a
/// row's elements). Sets, maps and sequences list it; a queue lists visiting instead.
template<typename Derived, typename R, typename S>
class iteration
{
    using types = store_types<R, S>;

public:
    using iterator = typename iterator_types<R, S>::iterator;
    using const_iterator = typename iterator_types<R, S>::const_iterator;
    using reference = typename iterator_types<R, S>::reference;
    using const_reference = typename iterator_types<R, S>::const_reference;
    using difference_type = typename store_types<R, S>::difference_type;

    [[nodiscard]] const_iterator begin() const { return representation().begin(); }
    [[nodiscard]] const_iterator end() const { return representation().end(); }
    [[nodiscard]] iterator begin()
        requires types::writable
    { return store_access::representation(static_cast<Derived&>(*this)).begin(); }
    [[nodiscard]] iterator end()
        requires types::writable
    { return store_access::representation(static_cast<Derived&>(*this)).end(); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend() const { return end(); }

private:
    [[nodiscard]] const R& representation() const { return store_access::representation(static_cast<const Derived&>(*this)); }
};

/// A queue's elements, visited in the order it keeps them (pushed, or none for a priority queue): iteration's members,
/// but no range urd reads (no element-wise ==, Hash or Less, no row elements).
template<typename Derived, typename R, typename S>
class visiting : public iteration<Derived, R, S>
{
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: wherever its kind lists it (sets, maps, sequences: ranges urd reads).
template<>
struct facet_condition<facet::iteration>
{
    template<typename R, typename S>
    static constexpr bool on()
    { return true; }
};
template<>
struct facet_operations<facet::iteration>
{
    using offered = operation_list<operation::iterate>;
};

/// When a queue has this facet: not a bucket queue (monotone_priority), which would visit every priority's bucket.
template<>
struct facet_condition<facet::visiting>
{
    template<typename R, typename S>
    static constexpr bool on()
    { return !has<S, monotone_priority>; }
};
template<>
struct facet_operations<facet::visiting>
{
    using offered = operation_list<operation::iterate>;
};

}  // namespace urd::detail
