#pragma once

#include "urd/store/types.hpp"

#include <ranges>

namespace urd::detail::facet
{

/// Sized (every kind so far): how many elements there are. A filter, which only answers membership, would not be.
template<typename Derived, typename R, typename S>
class sizing
{
public:
    using size_type = typename store_types<R, S>::size_type;

    [[nodiscard]] size_type size() const { return std::ranges::size(store_access::representation(static_cast<const Derived&>(*this))); }
    [[nodiscard]] bool empty() const { return size() == 0; }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::sizing>
{
    template<typename R, typename S>
    static constexpr bool on()
    { return S::sized; }
};

}  // namespace urd::detail
