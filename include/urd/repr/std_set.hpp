#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

#include <functional>
#include <set>
#include <unordered_set>

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P tree_set(const T& t, const F& f)
{
    if (!orders(t, f))
        return {};
    return props({ tag::sorted, tag::unique, tag::stable_refs, tag::sparse, tag::shrinkable, tag::reclaims, tag::proportional_clear });
}

constexpr P hash_set(const T& t)
{
    if (!t.hashable)
        return {};
    return props({ tag::unique, tag::stable_refs, tag::sparse, tag::shrinkable });
}

}  // namespace urd::detail::guarantees


namespace urd
{

// std::set orders by its comparator; only urd::Less (or std::less, where it agrees) on orderable elements is the
// order that `sorted` means (natural_order). Hash tables likewise need urd's hash and equality, or the
// standard's where a type has them (natural_hash).
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
    static constexpr auto headers = header_list<1> { "urd/repr/std_set.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(element_orders, f.by))
            return {};
        const auto held = guarantees::tree_set(f.element, f);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::log_n;
        static constexpr cost lookup = costs::log_n;
        static constexpr cost ordered_lookup = costs::log_n;
        static constexpr cost erase = costs::log_n;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr double words = 5;
    };
    static constexpr std::string_view description = "std::set (balanced tree)";
    static constexpr std::string_view holds = "a node per element; reclaims";
};
template<typename T, typename C, typename A>
struct declaration_of<std::set<T, C, A>>
{
    using declaration = set_decl;
    static constexpr request_features features = element_facts<T, C>();
};

}  // namespace detail

namespace detail
{

/// std::unordered_set (hash table)
struct unordered_set_decl
{
    static constexpr std::string_view id = "unordered_set";
    static constexpr auto headers = header_list<1> { "urd/repr/std_set.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by) || !f.hash_natural)
            return {};
        const auto held = guarantees::hash_set(f.element);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_ae;
        static constexpr cost lookup = costs::one_e;
        static constexpr cost erase = costs::one_e;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr double words = 4;
    };
    static constexpr std::string_view description = "std::unordered_set (hash table)";
    static constexpr std::string_view holds = "a node per element + buckets; stable references";
};
template<typename T, typename H, typename E, typename A>
struct declaration_of<std::unordered_set<T, H, E, A>>
{
    using declaration = unordered_set_decl;
    static constexpr request_features features = hashed_element_facts<T, H, E>();
};

}  // namespace detail

}  // namespace urd

namespace urd
{

template<>
struct include<"urd/repr/std_set.hpp">
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
