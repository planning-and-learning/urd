#pragma once

// Multimaps whose values lie together per key, declared with what the registry reads of them: compressed sparse rows
// (frozen, by key index, with or without the inverse), growing rows per index, and postings in a hash table. Defined
// in urd/repr/maps/csr_multimap.hpp, bidirectional_csr.hpp, dense_multimap.hpp and postings.hpp.

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

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
    static constexpr auto headers = header_list<1> { "urd/repr/maps/csr_multimap.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ order::sorted, multiplicity::multi, keys::dense, representation::canonical });
    static constexpr property_set relies_on =
        relies(stated({ change::frozen }), traits<role::element, trait::an_index>(), traits<role::order, trait::own_order>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::lookup, costs::one>,
                                       cost_entry<operation::ordered_lookup, costs::lin_keys>,
                                       cost_entry<operation::iterate, costs::lin_keys>>;
    static constexpr double words = 1.2;
    static constexpr std::string_view description = "CSR multimap (each key's values together, in key order)";
    static constexpr std::string_view holds = "n values + U + 1 offsets";
};
template<typename K, typename V>
struct declaration_of<repr::CsrMultiMap<K, V>>
{
    using declaration = csr_multimap_decl;
    static constexpr property_set roles = map_roles<K, V>();
};

}  // namespace detail

namespace detail
{

/// dense multimap (a vector of values per index, growing)
struct dense_multimap_decl
{
    static constexpr std::string_view id = "dense_multimap";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/dense_multimap.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ order::sorted, multiplicity::multi, keys::dense, memory::retains });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>(), traits<role::order, trait::own_order>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_a>,
                                       cost_entry<operation::lookup, costs::one>,
                                       cost_entry<operation::ordered_lookup, costs::lin_keys>,
                                       cost_entry<operation::erase, costs::lin_row>,
                                       cost_entry<operation::clear, costs::lin_keys>,
                                       cost_entry<operation::iterate, costs::lin_keys>>;
    static constexpr double words = 4;
    static constexpr std::string_view description = "dense multimap (a vector of values per index, growing)";
    static constexpr std::string_view holds = "a vector per index; retains";
};
template<typename K, typename V>
struct declaration_of<repr::DenseMultiMap<K, V>>
{
    using declaration = dense_multimap_decl;
    static constexpr property_set roles = map_roles<K, V>();
};

}  // namespace detail

namespace detail
{

/// postings (a flat hash table of value vectors)
struct posting_map_decl
{
    static constexpr std::string_view id = "posting_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/postings.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ multiplicity::multi, keys::sparse });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_ae>,
                                       cost_entry<operation::lookup, costs::one_e>,
                                       cost_entry<operation::erase, costs::lin_row>,
                                       cost_entry<operation::clear, costs::lin_capacity>,
                                       cost_entry<operation::iterate, costs::lin_n>>;
    static constexpr double words = 4;
    static constexpr std::string_view description = "postings (a flat hash table of value vectors)";
    static constexpr std::string_view holds = "a vector of values per key";
};
template<typename K, typename V>
struct declaration_of<repr::PostingMap<K, V>>
{
    using declaration = posting_map_decl;
    static constexpr property_set roles = map_roles<K, V>();
};

}  // namespace detail

namespace detail
{

/// bidirectional CSR (each key's values and each value's keys together)
struct bidirectional_csr_decl
{
    static constexpr std::string_view id = "bidirectional_csr";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/bidirectional_csr.hpp" };
    using kind = map_kind;
    static constexpr property_set provides =
        property_set::of({ order::sorted, multiplicity::multi, keys::dense, representation::canonical, direction::bidirectional });
    static constexpr property_set relies_on = relies(stated({ change::frozen }),
                                                     traits<role::element, trait::an_index>(),
                                                     traits<role::order, trait::own_order>(),
                                                     traits<role::mapped, trait::an_index>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::lookup, costs::one>,
                                       cost_entry<operation::ordered_lookup, costs::lin_keys>,
                                       cost_entry<operation::iterate, costs::lin_keys>>;
    static constexpr double words = 2.4;
    static constexpr std::string_view description = "bidirectional CSR (each key's values and each value's keys together)";
    static constexpr std::string_view holds = "n values + U + 1 offsets, both ways";
};
template<typename K, typename V>
struct declaration_of<repr::BidirectionalCsr<K, V>>
{
    using declaration = bidirectional_csr_decl;
    static constexpr property_set roles = map_roles<K, V>();
};

}  // namespace detail

}  // namespace urd
