#pragma once

#include "urd/kinds.hpp"
#include "urd/store.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <nanobind/nanobind.h>
#include <nanobind/stl/detail/nb_dict.h>
#include <nanobind/stl/detail/nb_list.h>
#include <nanobind/stl/detail/nb_set.h>

// nanobind type casters for urd Stores: a sequence Store converts as std::vector does (a list), a set Store as
// std::set (a set), a map Store with one value per key as std::map (a dict), with nanobind's own casters. Which Stores
// convert is decided by their representation: the header in urd/python/ named as the representation's in urd/repr/
// (urd/python/values/unordered_flat_set.hpp for urd/repr/values/unordered_flat_set.hpp) switches it on, so a binding
// includes the caster of each representation its Stores resolve to, as it includes the representation itself. A
// multimap has no caster: a dict holds one value per key.

namespace urd::python
{

/// Whether a Store whose representation is R converts to and from Python: specialized by R's header in urd/python/.
template<typename R>
inline constexpr bool casts = false;

/// A Store whose representation converts.
template<typename S>
concept castable = casts<typename S::representation>;

}  // namespace urd::python

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// A sequence Store: a list.
template<typename Kind, typename... Tags>
    requires urd::python::castable<urd::Store<Kind, Tags...>> && std::same_as<typename urd::traits_t<urd::Store<Kind, Tags...>>::kind, urd::sequence_kind>
struct type_caster<urd::Store<Kind, Tags...>> : list_caster<urd::Store<Kind, Tags...>, typename urd::Store<Kind, Tags...>::value_type>
{
};

/// A set Store: a set.
template<typename Kind, typename... Tags>
    requires urd::python::castable<urd::Store<Kind, Tags...>> && std::same_as<typename urd::traits_t<urd::Store<Kind, Tags...>>::kind, urd::set_kind>
struct type_caster<urd::Store<Kind, Tags...>> : set_caster<urd::Store<Kind, Tags...>, typename urd::Store<Kind, Tags...>::key_type>
{
};

/// A map Store with one value per key: a dict.
template<typename Kind, typename... Tags>
    requires urd::python::castable<urd::Store<Kind, Tags...>> && std::same_as<typename urd::traits_t<urd::Store<Kind, Tags...>>::kind, urd::map_kind>
             && urd::has<urd::traits_t<urd::Store<Kind, Tags...>>, urd::unique>
struct type_caster<urd::Store<Kind, Tags...>>
    : dict_caster<urd::Store<Kind, Tags...>, typename urd::Store<Kind, Tags...>::key_type, typename urd::Store<Kind, Tags...>::mapped_type>
{
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
