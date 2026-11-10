#pragma once

#include "urd/store/types.hpp"

#include <cstddef>

namespace urd::detail::facet
{

/// Growable: room for elements still to come.
template<typename Derived, typename R, typename S>
class reserving
{
public:
    /// Hints how many elements will be inserted.
    void reserve(std::size_t count)
        requires requires(R& held) { held.reserve(count); }
    { store_access::of<Derived>(*this).reserve(count); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::reserving> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return grows<S>; }
};

}  // namespace urd::detail
