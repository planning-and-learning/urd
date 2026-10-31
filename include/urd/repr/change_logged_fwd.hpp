#pragma once

// A set that records what changed since its log was cleared, declared with what the registry reads of it. Defined in
// urd/repr/change_logged.hpp (its log is two indexed sets); the set it logs in its own header.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/traits.hpp"

#include <type_traits>

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P change_logged(const P& part)
{
    if (!part || !part->has(tag::unique))
        return {};
    return part->has(tag::dense) ? with(*part, { tag::change_log, tag::any_clear }) : with(*part, { tag::change_log });
}

}  // namespace urd::detail::guarantees


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
    static constexpr auto headers = then(Part::headers, "urd/repr/change_logged.hpp");
    static constexpr offered offers(const request_features& f)
    {
        // Over its part with urd::Less: the wrapper's type is written for it.
        if (f.by != order::none)
            return {};
        const auto part = Part::offers(f);
        if (!part)
            return {};
        const auto held = guarantees::change_logged(part->properties);
        if (!held)
            return {};
        auto result = *part;
        result.properties = *held;
        return result;
    }
    struct operation_costs : Part::operation_costs
    {
        using part = Part;
        static constexpr cost erase = costs::lin_changes;
        static constexpr cost clear = costs::n_changes;
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
    static constexpr request_features features = declaration_of<R>::features;
};

}  // namespace urd
