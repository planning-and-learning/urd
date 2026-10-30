#pragma once

// Multimaps whose values lie together per key, declared with what the registry reads of them: compressed sparse rows
// (frozen, by key index, with or without the inverse), growing rows per index, and postings in a hash table. The
// first three are defined in urd/repr/grouped.hpp (urd/use/dense.hpp), the postings in urd/repr/postings.hpp
// (urd/use/hash.hpp).

#include "urd/elements.hpp"
#include "urd/traits.hpp"

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

/// Values in key order, each key's in the order given; built once, never changed.
template<indexable K, typename V>
struct container_traits<repr::CsrMultiMap<K, V>> : properties_of<sorted, multi, dense, canonical>
{
};

/// As a CsrMultiMap, answering by value too.
template<indexable K, indexable V>
struct container_traits<repr::BidirectionalCsr<K, V>> : with_t<container_traits<repr::CsrMultiMap<K, V>>, bidirectional>
{
};

/// Values grouped by key, in no order of keys; erase and clear free what the removed keys' vectors hold.
template<hashable K, typename V>
struct container_traits<repr::PostingMap<K, V>> : properties_of<multi, sparse, shrinkable>
{
};

/// A row of values per index, in key order; erase and clear keep the rows and their storage.
template<indexable K, typename V>
struct container_traits<repr::DenseMultiMap<K, V>> : properties_of<sorted, multi, dense, shrinkable, retains>
{
};

template<typename K, typename V>
inline constexpr auto defined_in<repr::CsrMultiMap<K, V>> = detail::fixed_string("urd/use/dense.hpp");
template<typename K, typename V>
inline constexpr auto defined_in<repr::BidirectionalCsr<K, V>> = detail::fixed_string("urd/use/dense.hpp");
template<typename K, typename V>
inline constexpr auto defined_in<repr::DenseMultiMap<K, V>> = detail::fixed_string("urd/use/dense.hpp");
template<typename K, typename V>
inline constexpr auto defined_in<repr::PostingMap<K, V>> = detail::fixed_string("urd/use/hash.hpp");

}  // namespace urd
