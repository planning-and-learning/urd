#pragma once

// A set that records what changed since its log was cleared, declared with what the registry reads of it. Defined in
// urd/repr/wrappers/change_logged.hpp (its log is two indexed sets); the set it logs in its own header.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/repr/maps/dense_map_fwd.hpp"
#include "urd/repr/values/dense_set_fwd.hpp"
#include "urd/repr/values/std_set.hpp"
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

namespace detail
{

inline constexpr std::string_view change_logged_prefix = "change_logged_";
inline constexpr std::string_view change_logged_description = "change-logged (plus what was added and removed) ";

/// A set that logs its changes, over Part's declaration: its id and words are the part's, prefixed; its offer the
/// part's with change_log (and any clear for a dense part, whose words a clear must walk).
template<typename Part>
struct change_logged_decl
{
    static constexpr std::string_view id = joined<change_logged_prefix, Part::id>::value;
    static constexpr auto headers = then(Part::headers, "urd/repr/wrappers/change_logged.hpp");
    static_assert(Part::provides.has(uniqueness::unique), "change_logged_decl: a change log is kept over a set");
    using kind = typename Part::kind;
    static constexpr property_set provides =
        Part::provides.has(keys::dense) ? with(Part::provides, { tracking::change_log, clearing::none }) : with(Part::provides, { tracking::change_log });
    /// The part's, and its own order: the wrapper's type is written for urd::Less.
    static constexpr role_tags relies_on = []
    {
        auto result = Part::relies_on;
        result.at(role_v<role::order>).traits.put(trait_v<trait::own_order>);
        return result;
    }();
    static constexpr auto gives_elements = Part::gives_elements;
    struct operation_costs : Part::operation_costs
    {
        using part = Part;
        static constexpr cost erase = costs::lin_changes;
        // A clear walks the elements, then clears the part: in every part logged, iterating costs at least clearing.
        static constexpr cost clear = Part::operation_costs::iterate;
        static constexpr double words = Part::operation_costs::words + 1;
    };
    static constexpr std::string_view description = joined<change_logged_description, Part::description>::value;
    static constexpr std::string_view holds = "+ the elements added and removed";
};

}  // namespace detail

template<typename R>
struct detail::declaration_of<repr::ChangeLogged<R>>
{
    using declaration = change_logged_decl<declaration_t<R>>;
    static constexpr auto roles = declaration_of<R>::roles;
};

}  // namespace urd
