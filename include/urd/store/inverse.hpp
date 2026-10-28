#pragma once

#include "urd/store/types.hpp"

namespace urd::detail::facet
{

/// A bidirectional map: the keys that map to a value, as a range (a frozen graph's predecessors of a vertex).
template<typename Derived, typename R, typename S>
class inverse
{
public:
    [[nodiscard]] auto keys(const typename S::mapped_type& value) const { return store_access::representation(static_cast<const Derived&>(*this)).keys(value); }
};

}  // namespace urd::detail::facet
