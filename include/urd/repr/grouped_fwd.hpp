#pragma once

// Multimaps whose values lie together per key, declared with what the registry reads of them: compressed sparse rows
// (frozen, by key index, with or without the inverse), growing rows per index, and postings in a hash table. Defined
// in urd/repr/csr_multimap.hpp, bidirectional_csr.hpp, dense_multimap.hpp and postings.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P csr_multimap(const T& key)
{
    if (!key.indexable)
        return {};
    return property_set::of({ tag::sorted, tag::multi, tag::dense, tag::canonical });
}

constexpr P bidirectional_csr(const T& key, const T& mapped)
{
    if (!mapped.indexable)
        return {};
    const auto base = csr_multimap(key);
    return base ? P(with(*base, { tag::bidirectional })) : P();
}

constexpr P dense_multimap(const T& key)
{
    if (!key.indexable)
        return {};
    return property_set::of({ tag::sorted, tag::multi, tag::dense, tag::shrinkable, tag::retains });
}

constexpr P posting_map(const T& key)
{
    if (!key.hashable)
        return {};
    return property_set::of({ tag::multi, tag::sparse, tag::shrinkable });
}

}  // namespace urd::detail::guarantees


namespace urd::repr
{

/// The multimap interface, written once over how Groups keeps a key's values together.
template<typename Groups>
class GroupedMultiMap;
/// Groups by key index, as compressed sparse rows: frozen.
template<typename K, typename V>
class IndexGroups;
/// Groups as postings: a flat hash table from each key to the vector of its values.
template<typename K, typename V>
class HashGroups;
/// Groups by key index that grow: a vector of values per index.
template<typename K, typename V>
class DenseGroups;
/// A CsrMultiMap that also keeps the inverse, values to keys.
template<typename K, typename V>
class BidirectionalCsr;

/// A frozen multimap from indices to values, as compressed sparse rows.
template<typename K, typename V>
using CsrMultiMap = GroupedMultiMap<IndexGroups<K, V>>;
/// A multimap from indices to values that grows: a vector of values per index.
template<typename K, typename V>
using DenseMultiMap = GroupedMultiMap<DenseGroups<K, V>>;

/// A multimap as postings: each key's values together in a flat hash table.
template<typename K, typename V>
using PostingMap = GroupedMultiMap<HashGroups<K, V>>;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// CSR multimap (each key's values together, in key order)
struct csr_multimap_decl
{
    static constexpr std::string_view id = "csr_multimap";
    static constexpr auto headers = header_list<1> { "urd/repr/csr_multimap.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::csr_multimap(f.key);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost lookup = costs::one_t;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 1.2;
    };
    static constexpr std::string_view description = "CSR multimap (each key's values together, in key order)";
    static constexpr std::string_view holds = "n values + U + 1 offsets";
};
template<typename K, typename V>
struct declaration_of<repr::CsrMultiMap<K, V>>
{
    using declaration = csr_multimap_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// dense multimap (a vector of values per index, growing)
struct dense_multimap_decl
{
    static constexpr std::string_view id = "dense_multimap";
    static constexpr auto headers = header_list<1> { "urd/repr/dense_multimap.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::dense_multimap(f.key);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_at;
        static constexpr cost lookup = costs::one_t;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::lin_row_t;
        static constexpr cost clear = costs::lin_keys;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 4;
    };
    static constexpr std::string_view description = "dense multimap (a vector of values per index, growing)";
    static constexpr std::string_view holds = "a vector per index; retains";
};
template<typename K, typename V>
struct declaration_of<repr::DenseMultiMap<K, V>>
{
    using declaration = dense_multimap_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// postings (a flat hash table of value vectors)
struct posting_map_decl
{
    static constexpr std::string_view id = "posting_map";
    static constexpr auto headers = header_list<1> { "urd/repr/postings.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::posting_map(f.key);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_ae;
        static constexpr cost lookup = costs::one_e;
        static constexpr cost erase = costs::lin_row_t;
        static constexpr cost clear = costs::lin_capacity;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr double words = 4;
    };
    static constexpr std::string_view description = "postings (a flat hash table of value vectors)";
    static constexpr std::string_view holds = "a vector of values per key";
};
template<typename K, typename V>
struct declaration_of<repr::PostingMap<K, V>>
{
    using declaration = posting_map_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// bidirectional CSR (each key's values and each value's keys together)
struct bidirectional_csr_decl
{
    static constexpr std::string_view id = "bidirectional_csr";
    static constexpr auto headers = header_list<1> { "urd/repr/bidirectional_csr.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::bidirectional_csr(f.key, f.mapped);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost lookup = costs::one_t;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 2.4;
    };
    static constexpr std::string_view description = "bidirectional CSR (each key's values and each value's keys together)";
    static constexpr std::string_view holds = "n values + U + 1 offsets, both ways";
};
template<typename K, typename V>
struct declaration_of<repr::BidirectionalCsr<K, V>>
{
    using declaration = bidirectional_csr_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

}  // namespace urd
