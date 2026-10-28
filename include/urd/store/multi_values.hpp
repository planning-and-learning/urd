#pragma once

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
    [[nodiscard]] auto values(const typename S::key_type& key) const
    {
        const auto& representation = store_access::representation(static_cast<const Derived&>(*this));
        if constexpr (requires { representation.values(key); })
            return representation.values(key);
        else
        {
            const auto [first, last] = representation.equal_range(key);
            return std::ranges::subrange(first, last) | std::views::values;
        }
    }
};

}  // namespace urd::detail::facet
