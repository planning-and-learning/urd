#pragma once

#include "urd/store/types.hpp"

namespace urd::detail::facet
{

/// A fixed shape: its rows' contents go back to their defaults, the shape stays.
template<typename Derived, typename R, typename S>
class shape
{
public:
    /// Every element back to its default, every bit unset; with proportional_clear, in the rows handed out to change
    /// since the last reset only.
    void reset() { store_access::representation(static_cast<Derived&>(*this)).reset(); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::shape>
{
    static constexpr bool reads_container = false;
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, fixed_shape>; }
};

}  // namespace urd::detail
