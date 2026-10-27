#pragma once

#include "urd/store/types.hpp"

namespace urd::detail::facet
{

/// Kept by value or key: finding elements, as the standard's associative containers do.
template<typename Derived, typename R, typename S>
class lookup
{
    using types = store_types<R, S>;

public:
    [[nodiscard]] typename types::const_iterator find(const typename types::lookup_type& element) const { return representation().find(element); }
    [[nodiscard]] typename types::size_type count(const typename types::lookup_type& element) const { return representation().count(element); }
    [[nodiscard]] bool contains(const typename types::lookup_type& element) const { return representation().contains(element); }

private:
    [[nodiscard]] const R& representation() const { return store_access::representation(static_cast<const Derived&>(*this)); }
};

}  // namespace urd::detail::facet
