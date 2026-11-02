#pragma once

// Layers: a set, map or pool that answers lookups from its own elements, then from a parent's it neither owns nor
// changes. Declared with what the registry reads of them; defined in urd/repr/wrappers/layer.hpp and
// urd/repr/wrappers/layered_pool.hpp. What is layered is defined in its own header.

#include "urd/cost.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

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
    static constexpr auto headers = with_header(Part::headers, Pool ? "urd/repr/wrappers/layered_pool.hpp" : "urd/repr/wrappers/layer.hpp");
    static_assert(Part::provides.has(uniqueness::unique) && (!Pool || Part::provides.has(handling::handles)),
                  "layered_decl: a layer is over a set, map or pool");
    using kind = typename Part::kind;
    static constexpr property_set provides = Pool ? with(Part::provides, { layering::layered }) : with(Part::provides, { layering::layered, order::none });
    static constexpr property_set relies_on = Part::relies_on;
    static constexpr auto gives_elements = Part::gives_elements;
    using operation_costs = costs_over<Part>;
    static constexpr double words = Part::words;
    static constexpr std::string_view description = joined < Pool ? layered_pool_description : layered_description, Part::description > ::value;
    static constexpr std::string_view holds = Pool ? "its own rows, indices after the parent's" : "its own elements; the parent is not owned";
};

}  // namespace detail

template<typename R>
struct detail::declaration_of<repr::Layered<R>>
{
    using declaration = layered_decl<declaration_t<R>, false>;
    static constexpr auto roles = declaration_of<R>::roles;
};
template<typename R>
struct detail::declaration_of<repr::LayeredPool<R>>
{
    using declaration = layered_decl<declaration_t<R>, true>;
    static constexpr auto roles = declaration_of<R>::roles;
};

}  // namespace urd
