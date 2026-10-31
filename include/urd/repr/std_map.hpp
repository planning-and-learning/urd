#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/traits.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/registry/guarantees.hpp"

#include <functional>
#include <map>
#include <unordered_map>

namespace urd
{

// The standard library's maps, as its sets: a tree is sorted by its natural order on orderable keys and frees the node
// of what it erases; a hash table on hashable keys frees its nodes but keeps its buckets. References survive
// inserts and rehashes.





}  // namespace urd

namespace urd
{





}  // namespace urd

namespace urd
{

namespace detail
{

/// std::map (balanced tree)
struct map_decl
{
    static constexpr std::string_view id = "map";
    static constexpr auto headers = header_list<1> { "urd/repr/std_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(element_orders, f.by))
            return {};
        const auto held = guarantees::tree_map(f.key, f);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
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
    static constexpr std::string_view description = "std::map (balanced tree)";
    static constexpr std::string_view holds = "a node per entry; reclaims";
};
template<typename K, typename V, typename C, typename A>
struct declaration_of<std::map<K, V, C, A>>
{
    using declaration = map_decl;
    static constexpr request_features features = map_facts<K, V, C>();
};

}  // namespace detail

namespace detail
{

/// std::multimap (balanced tree)
struct multimap_decl
{
    static constexpr std::string_view id = "multimap";
    static constexpr auto headers = header_list<1> { "urd/repr/std_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(element_orders, f.by))
            return {};
        const auto held = guarantees::tree_multimap(f.key, f);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
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
    static constexpr std::string_view description = "std::multimap (balanced tree)";
    static constexpr std::string_view holds = "a node per entry; reclaims";
};
template<typename K, typename V, typename C, typename A>
struct declaration_of<std::multimap<K, V, C, A>>
{
    using declaration = multimap_decl;
    static constexpr request_features features = map_facts<K, V, C>();
};

}  // namespace detail

namespace detail
{

/// std::unordered_map (hash table)
struct unordered_map_decl
{
    static constexpr std::string_view id = "unordered_map";
    static constexpr auto headers = header_list<1> { "urd/repr/std_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by) || !f.hash_natural)
            return {};
        const auto held = guarantees::hash_map(f.key);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
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
    static constexpr std::string_view description = "std::unordered_map (hash table)";
    static constexpr std::string_view holds = "a node per entry + buckets; stable references";
};
template<typename K, typename V, typename H, typename E, typename A>
struct declaration_of<std::unordered_map<K, V, H, E, A>>
{
    using declaration = unordered_map_decl;
    static constexpr request_features features = hashed_map_facts<K, V, H, E>();
};

}  // namespace detail

namespace detail
{

/// std::unordered_multimap (hash table)
struct unordered_multimap_decl
{
    static constexpr std::string_view id = "unordered_multimap";
    static constexpr auto headers = header_list<1> { "urd/repr/std_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by) || !f.hash_natural)
            return {};
        const auto held = guarantees::hash_multimap(f.key);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_ae;
        static constexpr cost lookup = costs::one_e;
        static constexpr cost erase = costs::lin_row_e;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr double words = 4;
    };
    static constexpr std::string_view description = "std::unordered_multimap (hash table)";
    static constexpr std::string_view holds = "a node per entry + buckets; stable references";
};
template<typename K, typename V, typename H, typename E, typename A>
struct declaration_of<std::unordered_multimap<K, V, H, E, A>>
{
    using declaration = unordered_multimap_decl;
    static constexpr request_features features = hashed_map_facts<K, V, H, E>();
};

}  // namespace detail

}  // namespace urd

namespace urd
{

template<>
struct include<"urd/repr/std_map.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<unordered_map_decl, S>
{
    using type = std::unordered_map<typename S::key_type, typename S::mapped_type, urd::Hash<typename S::key_type>, urd::EqualTo<typename S::key_type>>;
};
template<typename S>
struct representation_of<unordered_multimap_decl, S>
{
    using type = std::unordered_multimap<typename S::key_type, typename S::mapped_type, urd::Hash<typename S::key_type>, urd::EqualTo<typename S::key_type>>;
};
template<typename S>
struct representation_of<map_decl, S>
{
    using type = std::map<typename S::key_type, typename S::mapped_type, key_order_t<S>>;
};
template<typename S>
struct representation_of<multimap_decl, S>
{
    using type = std::multimap<typename S::key_type, typename S::mapped_type, key_order_t<S>>;
};

}  // namespace urd::detail
