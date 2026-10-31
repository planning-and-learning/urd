#pragma once

// Layers: a set, map or pool that answers lookups from its own elements, then from a parent's it neither owns nor
// changes. Declared with what the registry reads of them; defined in urd/repr/layer.hpp and
// urd/repr/layered_pool.hpp. What is layered is defined in its own header.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/traits.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/registry/guarantees.hpp"

namespace urd::repr
{

/// A set or map with unique keys over a parent: holds, iterates and erases its own elements, looks up in both.
template<typename R>
class Layered;
/// A pool over a parent pool: its indices continue the parent's, a lookup answers from the chain.
template<typename R>
class LayeredPool;

}  // namespace urd::repr

namespace urd
{




}  // namespace urd

namespace urd
{

namespace detail
{

inline constexpr std::string_view layered_prefix = "layered_";
inline constexpr std::string_view layered_pool_description = "layered pool (its own elements after a parent's, indices continuing) of a ";
inline constexpr std::string_view layered_description = "layered (its own elements, then a parent's) over a ";

/// A layer of Part's declaration: its id and words the part's, prefixed; its offer the part's with layered, `Pool`
/// for a pool of rows or records (indices continuing the parent's) where it holds its order, else in no order.
template<typename Part, bool Pool>
struct layered_decl
{
    static constexpr std::string_view id = joined<layered_prefix, Part::id>::value;
    static constexpr auto headers = then(Part::headers, Pool ? "urd/repr/layered_pool.hpp" : "urd/repr/layer.hpp");
    static constexpr offered offers(const request_features& f)
    {
        const auto part = Part::offers(f);
        if (!part)
            return {};
        const auto held = Pool ? guarantees::layered_pool(part->properties) : guarantees::layered(part->properties);
        if (!held)
            return {};
        auto result = *part;
        result.properties = *held;
        return result;
    }
    struct operation_costs : Part::operation_costs
    {
        using part = Part;
    };
    static constexpr std::string_view description = joined<Pool ? layered_pool_description : layered_description, Part::description>::value;
    static constexpr std::string_view holds = Pool ? "its own rows, indices after the parent's" : "its own elements; the parent is not owned";
};

}  // namespace detail

template<typename R>
struct detail::declaration_of<repr::Layered<R>>
{
    using declaration = layered_decl<declaration_t<R>, false>;
    static constexpr request_features features = declaration_of<R>::features;
};
template<typename R>
struct detail::declaration_of<repr::LayeredPool<R>>
{
    using declaration = layered_decl<declaration_t<R>, true>;
    static constexpr request_features features = declaration_of<R>::features;
};

}  // namespace urd
