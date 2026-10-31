#pragma once

#include "urd/store/types.hpp"

#include <cstddef>

namespace urd::detail::facet
{

/// Growable: room for elements still to come.
template<typename Derived, typename R, typename S>
class capacity
{
public:
    /// Hints how many elements will be inserted; does nothing where the representation cannot reserve.
    void reserve(std::size_t count)
    {
        auto& representation = store_access::representation(static_cast<Derived&>(*this));
        if constexpr (requires { representation.reserve(count); })
            representation.reserve(count);
    }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::capacity>
{
    template<typename R, typename S>
    static constexpr bool on()
    { return grows<S>; }
};

}  // namespace urd::detail
