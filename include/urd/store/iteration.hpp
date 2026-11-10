#pragma once

#include "urd/cost.hpp"
#include "urd/store/types.hpp"

#include <concepts>
#include <iterator>
#include <ranges>

namespace urd::detail::facet
{

/// The elements in turn, changeable in place where the Store's elements are: a range urd reads (equality, hashing, a
/// row's elements). Sets, maps, multisets and sequences list it; a queue lists visiting instead.
template<typename Derived, typename R, typename S>
class iteration
{
    using types = store_types<S>;

public:
    using iterator = typename iterator_types<R, S>::iterator;
    using const_iterator = typename iterator_types<R, S>::const_iterator;
    using reference = typename iterator_types<R, S>::reference;
    using const_reference = typename iterator_types<R, S>::const_reference;
    using difference_type = typename store_types<S>::difference_type;

    [[nodiscard]] const_iterator begin() const
    {
        // Checked where it is iterated, not where its types are formed (iterator_types): asking would complete its
        // element type.
        static_assert(std::same_as<const_reference, std::ranges::range_reference_t<const R>>,
                      "urd::Store: its container's iterator names a reference (std::iterator_traits) that iterating it does not give");
        return representation().begin();
    }
    [[nodiscard]] const_iterator end() const { return representation().end(); }
    /// Where what it iterates may change in place (a sequence's elements, a map's values).
    [[nodiscard]] iterator begin()
        requires types::values_writable
    { return store_access::of<Derived>(*this).begin(); }
    [[nodiscard]] iterator end()
        requires types::values_writable
    { return store_access::of<Derived>(*this).end(); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend() const { return end(); }
    /// Backwards too, where it is iterated in an order the request fixes (sorted, insertion order, a sequence).
    [[nodiscard]] auto rbegin() const
        requires types::ordered
    { return std::make_reverse_iterator(end()); }
    [[nodiscard]] auto rend() const
        requires types::ordered
    { return std::make_reverse_iterator(begin()); }

private:
    [[nodiscard]] const R& representation() const { return store_access::of<Derived>(*this); }
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

template<>
struct facet_operations<facet::iteration>
{
    using offered = operation_list<operation::iterate>;
};

/// When a queue has this facet: not a bucket queue (monotone_priority), which would visit every priority's bucket.
template<>
struct facet_condition<facet::visiting> : facet_defaults
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
