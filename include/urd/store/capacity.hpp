#pragma once

#include "urd/store/types.hpp"

namespace urd::detail::facet
{

/// Growable: room for elements still to come.
template<typename Derived, typename R, typename S>
class capacity
{
    using size_type = typename store_types<R, S>::size_type;

public:
    /// Hints how many elements will be inserted; does nothing where the representation cannot reserve.
    void reserve(size_type count)
    {
        auto& representation = store_access::representation(static_cast<Derived&>(*this));
        if constexpr (requires { representation.reserve(count); })
            representation.reserve(count);
    }
};

}  // namespace urd::detail::facet
