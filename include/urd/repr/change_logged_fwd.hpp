#pragma once

// A set that records what changed since its log was cleared, declared with what the registry reads of it. Defined in
// urd/repr/change_logged.hpp, which urd/use/hash.hpp includes (its log is two indexed sets).

#include "urd/interface.hpp"
#include "urd/traits.hpp"

#include <type_traits>

namespace urd::repr
{

/// The set R with a log of the elements added and removed since the log was cleared. The registry names it for any
/// element type, so it is an unconstrained template; its container_traits declare properties only where R's do.
template<typename R>
class ChangeLogged;

}  // namespace urd::repr

namespace urd
{

/// What R guarantees, with the log. Clear logs every element first, which for a dense set means a pass over its
/// words (O(U/64), not proportional to the elements), so a dense set's clear cost is not promised.
template<typename R>
    requires has<container_traits<R>, unique> && (!map_traits<container_traits<R>>)
struct container_traits<repr::ChangeLogged<R>> :
    std::conditional_t<has<container_traits<R>, dense>, with_t<container_traits<R>, change_log, any_clear>, with_t<container_traits<R>, change_log>>
{
};
template<typename R>
struct parts<repr::ChangeLogged<R>>
{
    using type = detail::type_list<R>;
};
template<typename R>
inline constexpr auto defined_in<repr::ChangeLogged<R>> = detail::fixed_string("urd/use/hash.hpp");

}  // namespace urd
