#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

#include <boost/container/container_fwd.hpp>
#include <functional>

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P flat_set(const T& t, const F& f)
{
    if (!orders(t, f))
        return {};
    return property_set::of(
        { tag::sorted, tag::unique, tag::positional, tag::sparse, tag::contiguous, tag::canonical, tag::shrinkable, tag::retains, tag::proportional_clear });
}

constexpr P flat_multiset(const T& t, const F& f)
{
    if (!orders(t, f))
        return {};
    return property_set::of({ tag::sorted, tag::positional, tag::contiguous, tag::canonical, tag::shrinkable, tag::retains, tag::proportional_clear });
}

constexpr P flat_map(const T& key, const F& f)
{
    if (!orders(key, f))
        return {};
    return property_set::of({ tag::sorted,
                   tag::unique,
                   tag::functional,
                   tag::positional,
                   tag::sparse,
                   tag::contiguous,
                   tag::canonical,
                   tag::shrinkable,
                   tag::retains,
                   tag::proportional_clear });
}

constexpr P flat_multimap(const T& key, const F& f)
{
    if (!orders(key, f))
        return {};
    return property_set::of({ tag::sorted, tag::multi, tag::positional, tag::contiguous, tag::shrinkable, tag::retains, tag::proportional_clear });
}

}  // namespace urd::detail::guarantees


namespace urd
{

// Boost's flat sets are sorted, as std::set is, by a natural order on orderable elements.
// Their vector keeps its capacity when elements are erased. Declared here through Boost's forward header; defined by
// urd/repr/flat_set.hpp and urd/repr/flat_map.hpp.

// Their maps likewise, sorted by key. Their iterators would let a key be overwritten; a Store hands out only
// const ones.

// A multimap keeps equal keys' values in insertion order, so equal contents inserted in another order differ:
// it is not canonical.

}  // namespace urd

namespace urd
{

namespace detail
{

/// boost::container::flat_set (sorted vector)
struct flat_set_decl
{
    static constexpr std::string_view id = "flat_set";
    static constexpr auto headers = header_list<1> { "urd/repr/flat_set.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(element_orders, f.by))
            return {};
        const auto held = guarantees::flat_set(f.element, f);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::lin_n;
        static constexpr cost lookup = costs::log_n;
        static constexpr cost ordered_lookup = costs::log_n;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "boost::container::flat_set (sorted vector)";
    static constexpr std::string_view holds = "n elements in one array; retains";
};
template<typename T, typename C, typename A>
struct declaration_of<boost::container::flat_set<T, C, A>>
{
    using declaration = flat_set_decl;
    static constexpr request_features features = element_facts<T, C>();
};

}  // namespace detail

namespace detail
{

/// boost::container::flat_multiset (sorted vector)
struct flat_multiset_decl
{
    static constexpr std::string_view id = "flat_multiset";
    static constexpr auto headers = header_list<1> { "urd/repr/flat_set.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(element_orders, f.by))
            return {};
        const auto held = guarantees::flat_multiset(f.element, f);
        if (!held)
            return {};
        return offer_of(urd::kind::multiset, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::lin_n;
        static constexpr cost lookup = costs::log_n;
        static constexpr cost ordered_lookup = costs::log_n;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "boost::container::flat_multiset (sorted vector)";
    static constexpr std::string_view holds = "n elements in one array; retains";
};
template<typename T, typename C, typename A>
struct declaration_of<boost::container::flat_multiset<T, C, A>>
{
    using declaration = flat_multiset_decl;
    static constexpr request_features features = element_facts<T, C>();
};

}  // namespace detail

namespace detail
{

/// boost::container::flat_map (sorted vector)
struct flat_map_decl
{
    static constexpr std::string_view id = "flat_map";
    static constexpr auto headers = header_list<1> { "urd/repr/flat_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(element_orders, f.by))
            return {};
        const auto held = guarantees::flat_map(f.key, f);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::lin_n;
        static constexpr cost lookup = costs::log_n;
        static constexpr cost ordered_lookup = costs::log_n;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "boost::container::flat_map (sorted vector)";
    static constexpr std::string_view holds = "n entries in one array; retains";
};
template<typename K, typename V, typename C, typename A>
struct declaration_of<boost::container::flat_map<K, V, C, A>>
{
    using declaration = flat_map_decl;
    static constexpr request_features features = map_facts<K, V, C>();
};

}  // namespace detail

namespace detail
{

/// boost::container::flat_multimap (sorted vector)
struct flat_multimap_decl
{
    static constexpr std::string_view id = "flat_multimap";
    static constexpr auto headers = header_list<1> { "urd/repr/flat_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(element_orders, f.by))
            return {};
        const auto held = guarantees::flat_multimap(f.key, f);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::lin_n;
        static constexpr cost lookup = costs::log_n;
        static constexpr cost ordered_lookup = costs::log_n;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "boost::container::flat_multimap (sorted vector)";
    static constexpr std::string_view holds = "n entries in one array; retains";
};
template<typename K, typename V, typename C, typename A>
struct declaration_of<boost::container::flat_multimap<K, V, C, A>>
{
    using declaration = flat_multimap_decl;
    static constexpr request_features features = map_facts<K, V, C>();
};

}  // namespace detail

}  // namespace urd
