#pragma once

#include "urd/store/types.hpp"

#include <ranges>
#include <type_traits>

namespace urd::detail::facet
{

/// Sized: how many elements there are, where the container counts them.
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

/// When a Store has this facet: where its container counts its elements (a pool that shares parts of its rows keeps
/// no count); asked from the spec alone (R void), never.
template<>
struct facet_condition<facet_id::sizing>
{
    template<typename R, typename S>
    static constexpr bool on()
    {
        if constexpr (std::is_void_v<R>)
            return false;
        else
            return std::ranges::sized_range<const R> || requires(const R& range) { range.size(); };
    }
};

}  // namespace urd::detail
