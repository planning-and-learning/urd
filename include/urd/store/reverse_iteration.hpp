#pragma once

#include "urd/store/types.hpp"

#include <iterator>

namespace urd::detail::facet
{

/// Iterated in an order the request asked for (sorted, insertion order, or a sequence): backwards too.
template<typename Derived, typename R, typename S>
class reverse_iteration
{
public:
    [[nodiscard]] auto rbegin() const { return std::make_reverse_iterator(self().end()); }
    [[nodiscard]] auto rend() const { return std::make_reverse_iterator(self().begin()); }

private:
    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::reverse_iteration>
{
    static constexpr bool reads_container = false;
    template<typename R, typename S>
    static constexpr bool on()
    { return store_types<R, S>::ordered; }
};

}  // namespace urd::detail
