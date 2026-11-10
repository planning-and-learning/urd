#pragma once

#include "urd/cost.hpp"
#include "urd/store/types.hpp"

#include <cstddef>

namespace urd::detail::facet
{

/// A partition: keys join as elements by insert, each in a class of its own, and classes join by merge. find names
/// an element's class by one of its elements.
template<typename Derived, typename R, typename S>
class grouping
{
    using key = typename S::key_type;

public:
    /// Makes `element` an element (and, for dense keys, every smaller index not yet one).
    void insert(const key& element) { representation().insert(element); }
    /// Joins the classes of `lhs` and `rhs`; false where they were one already.
    bool merge(const key& lhs, const key& rhs) { return representation().merge(lhs, rhs); }
    /// The element that names the class of `element`.
    [[nodiscard]] key find(const key& element) const { return representation().find(element); }
    /// Whether `element` is an element of some class.
    [[nodiscard]] bool contains(const key& element) const { return representation().contains(element); }
    void clear()
        requires clears<S>
    { representation().clear(); }

private:
    [[nodiscard]] const R& representation() const { return store_access::of<Derived>(*this); }
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

template<>
struct facet_operations<facet::grouping>
{
    using offered = operation_list<operation::insert, operation::merge, operation::lookup>;
};

}  // namespace urd::detail
