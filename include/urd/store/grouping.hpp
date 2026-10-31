#pragma once

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
    [[nodiscard]] key find(const key& element) const { return constant().find(element); }
    /// Whether `element` is an element of some class.
    [[nodiscard]] bool contains(const key& element) const { return constant().contains(element); }
    [[nodiscard]] bool same(const key& lhs, const key& rhs) const { return constant().same(lhs, rhs); }
    [[nodiscard]] std::size_t class_size(const key& element) const { return constant().class_size(element); }
    /// How many classes there are.
    [[nodiscard]] std::size_t classes() const { return constant().classes(); }
    void clear()
        requires shrinks<S>
    { representation().clear(); }

private:
    [[nodiscard]] const R& constant() const { return store_access::representation(static_cast<const Derived&>(*this)); }
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::grouping>
{
    template<typename R, typename S>
    static constexpr bool on()
    { return S::is_partition; }
};

}  // namespace urd::detail
