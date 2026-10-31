#pragma once

// A set that records what changed since its log was cleared, declared with what the registry reads of it. Defined in
// urd/repr/change_logged.hpp (its log is two indexed sets); the set it logs in its own header.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/interface.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/repr/std_set.hpp"
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
inline constexpr auto defined_in<repr::ChangeLogged<R>> = detail::fixed_string("urd/repr/change_logged.hpp");

}  // namespace urd

namespace urd
{

/// change_logged: its part's costs, but for what it declares here.
template<typename R>
struct container_costs<repr::ChangeLogged<R>> : part_costs<part_of<repr::ChangeLogged<R>>>
{
    static constexpr cost erase = costs::lin_changes_t;
    static constexpr cost clear = costs::n_changes_t;
    static constexpr double words = part_costs<part_of<repr::ChangeLogged<R>>>::words + 1;
};

}  // namespace urd

namespace urd
{

namespace detail
{
inline constexpr std::string_view change_logged_prefix = "change_logged_";
inline constexpr std::string_view change_logged_description = "change-logged (plus what was added and removed) ";
}  // namespace detail

/// A change-logged set: `change_logged_` and its part's id (`change_logged_set`).
template<typename R>
struct container_synopsis<repr::ChangeLogged<R>>
{
    static constexpr std::string_view id = joined<detail::change_logged_prefix, container_synopsis<R>::id>::value;
    static constexpr std::string_view description = joined<detail::change_logged_description, container_synopsis<R>::description>::value;
    static constexpr std::string_view holds = "+ the elements added and removed";
};

}  // namespace urd
