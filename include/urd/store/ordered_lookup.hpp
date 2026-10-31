#pragma once

#include "urd/store/types.hpp"

#include <utility>

namespace urd::detail::facet
{

/// Sorted: searching by order.
template<typename Derived, typename R, typename S>
class ordered_lookup
{
    using types = store_types<R, S>;
    using const_iterator_type = typename iterator_types<R, S>::const_iterator;

public:
    [[nodiscard]] const_iterator_type lower_bound(const typename types::lookup_type& element) const { return representation().lower_bound(element); }
    [[nodiscard]] const_iterator_type upper_bound(const typename types::lookup_type& element) const { return representation().upper_bound(element); }
    [[nodiscard]] std::pair<const_iterator_type, const_iterator_type> equal_range(const typename types::lookup_type& element) const
    { return representation().equal_range(element); }

private:
    [[nodiscard]] const R& representation() const { return store_access::representation(static_cast<const Derived&>(*this)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::ordered_lookup>
{
    static constexpr bool reads_container = false;
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, sorted> && !is_entry_less<typename S::order_type>; }
};

}  // namespace urd::detail
