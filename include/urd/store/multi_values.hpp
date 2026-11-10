#pragma once

#include "urd/cost.hpp"
#include "urd/store/types.hpp"

#include <ranges>

namespace urd::detail::facet
{

/// A multimap: the values of a key, as a range. A representation that keeps them together (compressed sparse rows)
/// gives them as a span; any other, as the values of its equal_range.
template<typename Derived, typename R, typename S>
class multi_values
{
public:
    /// What a key maps to (values_by_key says it for unique keys; the two are never both on).
    using mapped_type = typename S::mapped_type;

    [[nodiscard]] auto values(const typename S::key_type& key) const
        requires requires(const R& held) { held.values(key); }
    { return store_access::of<Derived>(*this).values(key); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::multi_values> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return !has<S, unique>; }
};
template<>
struct facet_operations<facet::multi_values>
{
    using offered = operation_list<operation::lookup>;
};

}  // namespace urd::detail
