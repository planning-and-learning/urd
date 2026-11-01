#pragma once

#include "urd/store/types.hpp"

#include <ranges>

namespace urd::detail::facet
{

/// Contiguous: the elements as an array.
template<typename Derived, typename R, typename S>
class contiguous_data
{
    using types = store_types<S>;

public:
    [[nodiscard]] auto data() const { return std::ranges::cdata(representation()); }
    [[nodiscard]] auto data()
        requires types::writable
    { return std::ranges::data(store_access::of<Derived>(*this)); }

private:
    [[nodiscard]] const R& representation() const { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::contiguous_data> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, contiguous>; }
};

}  // namespace urd::detail
