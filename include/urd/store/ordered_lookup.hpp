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
    using const_iterator = typename types::const_iterator;
    using lookup_type = typename types::lookup_type;

public:
    [[nodiscard]] const_iterator lower_bound(const lookup_type& element) const { return representation().lower_bound(element); }
    [[nodiscard]] const_iterator upper_bound(const lookup_type& element) const { return representation().upper_bound(element); }
    [[nodiscard]] std::pair<const_iterator, const_iterator> equal_range(const lookup_type& element) const { return representation().equal_range(element); }

private:
    [[nodiscard]] const R& representation() const { return store_access::representation(static_cast<const Derived&>(*this)); }
};

}  // namespace urd::detail::facet
