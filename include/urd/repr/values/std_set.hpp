#pragma once

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/repr/core/order.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <functional>
#include <set>
#include <unordered_set>

namespace urd
{

// std::set orders by its comparator; only urd::Less (or std::less, where it agrees) on orderable elements is the
// order that `sorted` means (natural_order). Hash tables hash with urd::Hash and urd::EqualTo.
// References survive inserts and a hash table's rehash; iterators of a hash table do not. A tree frees the node of
// what it erases; a hash table frees its nodes but keeps its buckets, so it neither retains nor reclaims.

}  // namespace urd

namespace urd
{

namespace detail
{

/// std::set (balanced tree)
struct set_decl
{
    static constexpr std::string_view id = "set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/std_set.hpp" };
    using kind = set_kind;
    static constexpr property_set provides =
        property_set::of({ order::sorted, uniqueness::unique, referencing::stable_refs, keys::sparse, memory::reclaims, clearing::proportional_clear });
    static constexpr property_set relies_on = relies(traits<role::order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::log_n;
        static constexpr cost lookup = costs::log_n;
        static constexpr cost ordered_lookup = costs::log_n;
        static constexpr cost erase = costs::log_n;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr double words = 5;
    };
    static constexpr std::string_view description = "std::set (balanced tree)";
    static constexpr std::string_view holds = "a node per element; reclaims";
};
template<typename T, typename C, typename A>
struct declaration_of<std::set<T, C, A>>
{
    using declaration = set_decl;
    static constexpr property_set roles = element_roles<T, C>();
};

}  // namespace detail

namespace detail
{

/// std::unordered_set (hash table)
struct unordered_set_decl
{
    static constexpr std::string_view id = "unordered_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/std_set.hpp" };
    using kind = set_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, referencing::stable_refs, keys::sparse });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::one_ae;
        static constexpr cost lookup = costs::one_e;
        static constexpr cost erase = costs::one_e;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr double words = 4;
    };
    static constexpr std::string_view description = "std::unordered_set (hash table)";
    static constexpr std::string_view holds = "a node per element + buckets; stable references";
};
template<typename T, typename H, typename E, typename A>
struct declaration_of<std::unordered_set<T, H, E, A>>
{
    using declaration = unordered_set_decl;
    static constexpr property_set roles = element_roles<T>();
};

}  // namespace detail

}  // namespace urd

namespace urd
{

template<>
struct include<"urd/repr/values/std_set.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<unordered_set_decl, S>
{
    using type = std::unordered_set<typename S::element_type, urd::Hash<typename S::element_type>, urd::EqualTo<typename S::element_type>>;
};
template<typename S>
struct representation_of<set_decl, S>
{
    using type = std::set<typename S::element_type, element_order_t<S>>;
};

}  // namespace urd::detail
