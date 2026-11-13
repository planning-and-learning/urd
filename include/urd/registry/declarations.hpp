#pragma once

// Every container's declaration (urd/repr/*_fwd.hpp and the like), by kind (urd/kinds.hpp: declarations_of<Kind>,
// which a kind added outside urd specializes too), in the order of preference within the kind, the first that serves
// a request being chosen
// (urd/registry/scan.hpp): the one source of that order. The order follows docs/BENCHMARKS.md:
// an open-addressing hash table is fastest at insert, lookup and clear-and-refill at every size, so it comes first
// wherever order is not asked for; node tables and segments remain for stable_refs; where order is asked for, a tree
// serves elements inserted over time. The dense containers come after the hash tables, and only a request that
// promises dense keys reaches them at all (dense is demanded); the indexed set, after them too, only a request for
// insertion order or positions. Frozen rows are flattened first; rows are kept for reuse only where their memory must
// be.

#include "urd/registry/declaration.hpp"
#include "urd/repr/graphs/graphs_fwd.hpp"
#include "urd/repr/maps/dense_map_fwd.hpp"
#include "urd/repr/maps/flat_map_fwd.hpp"
#include "urd/repr/maps/grouped_fwd.hpp"
#include "urd/repr/maps/ordered_map_fwd.hpp"
#include "urd/repr/maps/parallel_hash_map_fwd.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/maps/unordered_flat_map_fwd.hpp"
#include "urd/repr/partitions/disjoint_sets_fwd.hpp"
#include "urd/repr/pools/pools_fwd.hpp"
#include "urd/repr/queues/queues_fwd.hpp"
#include "urd/repr/records/records_fwd.hpp"
#include "urd/repr/sequences/rows_fwd.hpp"
#include "urd/repr/sequences/segmented_fwd.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/slots/slots_fwd.hpp"
#include "urd/repr/values/dense_set_fwd.hpp"
#include "urd/repr/values/flat_set_fwd.hpp"
#include "urd/repr/values/indexed_fwd.hpp"
#include "urd/repr/values/parallel_hash_set_fwd.hpp"
#include "urd/repr/values/std_set.hpp"
#include "urd/repr/values/unordered_flat_set_fwd.hpp"
#include "urd/repr/wrappers/layered_fwd.hpp"
#include "urd/repr/wrappers/recycling_fwd.hpp"
#include "urd/repr/wrappers/scoped_fwd.hpp"

#include <array>

namespace urd::detail
{

/// The containers of a kind (urd/kinds.hpp), in order of preference: specialized for each kind, a kind added outside
/// urd included.
template<typename Kind>
struct declarations_of;
template<typename Kind>
using declarations_t = typename declarations_of<Kind>::type;

/// Sequences: the declarations, in order of preference.
template<>
struct declarations_of<sequence_kind>
{
    using type = boost::mp11::
        mp_list<jagged_decl, fixed_jagged_decl, tracked_jagged_decl, vector_decl, segmented_vector_decl, concurrent_segmented_vector_decl, row_pool_decl>;
};

/// Sets and multisets: values kept by value, the same containers (a set requires unique, which no multiset container provides).
using values_declarations = boost::mp11::mp_list<unordered_flat_set_decl,
                                                 unordered_set_decl,
                                                 set_decl,
                                                 flat_multiset_decl,
                                                 indexed_set_decl,
                                                 flat_set_decl,
                                                 recycling_hash_set_decl,
                                                 recycling_tree_set_decl,
                                                 dense_set_decl,
                                                 tracked_dense_set_decl,
                                                 sparse_set_decl,
                                                 stable_indexed_set_decl,
                                                 ordered_sparse_set_decl,
                                                 parallel_hash_set_decl,
                                                 parallel_node_hash_set_decl,
                                                 concurrent_indexed_set_decl>;
template<>
struct declarations_of<set_kind>
{
    using type = values_declarations;
};
template<>
struct declarations_of<multiset_kind>
{
    using type = values_declarations;
};

/// Maps and multimaps: the declarations, in order of preference.
template<>
struct declarations_of<map_kind>
{
    using type = boost::mp11::mp_list<unordered_flat_map_decl,
                                      unordered_map_decl,
                                      posting_map_decl,
                                      chained_posting_map_decl,
                                      unordered_multimap_decl,
                                      parallel_hash_map_decl,
                                      parallel_node_hash_map_decl,
                                      map_decl,
                                      multimap_decl,
                                      flat_multimap_decl,
                                      flat_map_decl,
                                      recycling_hash_map_decl,
                                      recycling_map_decl,
                                      dense_map_decl,
                                      stable_dense_map_decl,
                                      generation_map_decl,
                                      stable_generation_map_decl,
                                      thread_safe_generation_map_decl,
                                      ordered_map_decl,
                                      dense_ordered_map_decl,
                                      csr_multimap_decl,
                                      bidirectional_csr_decl,
                                      dense_multimap_decl,
                                      scoped_map_decl,
                                      scoped_tree_map_decl,
                                      scoped_dense_map_decl,
                                      layered_decl<dense_map_decl, false>,
                                      layered_decl<generation_map_decl, false>,
                                      layered_decl<thread_safe_generation_map_decl, false>>;
};

/// Queues, by arrival and by priority: the declarations, in order of preference (each serves one access).
template<>
struct declarations_of<queue_kind>
{
    using type = boost::mp11::mp_list<bucket_queue_decl, heap_decl, addressable_heap_decl, fifo_decl, ring_fifo_decl, lifo_decl>;
};

/// Pools interning values by handle: rows first (a Store or a registered container is interned as a row), then values
/// interned whole, then records stored flat (which a request asks for by flat); in order of preference.
template<>
struct declarations_of<pool_kind>
{
    using type = boost::mp11::mp_list<schema_row_interner_decl<false>,
                                      bit_row_interner_decl<false>,
                                      row_interner_decl,
                                      tree_pool_decl<false, false>,
                                      tree_pool_decl<true, false>,
                                      schema_row_interner_decl<true>,
                                      bit_row_interner_decl<true>,
                                      thread_safe_row_interner_decl,
                                      tree_pool_decl<false, true>,
                                      tree_pool_decl<true, true>,
                                      layered_decl<schema_row_interner_decl<false>, true>,
                                      layered_decl<bit_row_interner_decl<false>, true>,
                                      layered_decl<row_interner_decl, true>,
                                      layered_decl<schema_row_interner_decl<true>, true>,
                                      layered_decl<bit_row_interner_decl<true>, true>,
                                      layered_decl<thread_safe_row_interner_decl, true>,
                                      value_interner_decl,
                                      stable_value_interner_decl,
                                      thread_safe_value_interner_decl,
                                      record_interner_decl,
                                      arena_interner_decl,
                                      stable_record_interner_decl,
                                      thread_safe_record_interner_decl,
                                      thread_safe_arena_interner_decl,
                                      layered_decl<arena_interner_decl, true>,
                                      layered_decl<stable_record_interner_decl, true>,
                                      layered_decl<thread_safe_record_interner_decl, true>,
                                      layered_decl<thread_safe_arena_interner_decl, true>>;
};

/// Pools of slots: the declarations, in order of preference.
template<>
struct declarations_of<slots_kind>
{
    using type = boost::mp11::mp_list<slot_vector_decl, thread_safe_slot_vector_decl>;
};

/// Partitions: the declarations, in order of preference.
template<>
struct declarations_of<partition_kind>
{
    using type = boost::mp11::mp_list<disjoint_sets_decl>;
};

/// Graphs: the declarations, in order of preference. A frozen graph is compressed sparse rows, plain before the variants
/// that cost more, or with constant_adjacency rows of bits stored once; one that grows with constant_adjacency is a
/// matrix of bits; anything else is lists. Rows of bits serve only requests of their shape (partitioned, undirected).
template<>
struct declarations_of<graph_kind>
{
    using type = boost::mp11::mp_list<csr_graph_decl<false>,
                                      csr_graph_decl<true>,
                                      bit_rows_decl<false, false, false>,
                                      bit_rows_decl<false, false, true>,
                                      bit_rows_decl<false, true, false>,
                                      bit_rows_decl<true, false, false>,
                                      bit_rows_decl<true, false, true>,
                                      bit_rows_decl<true, true, false>,
                                      bit_matrix_decl<false, false, false>,
                                      bit_matrix_decl<false, false, true>,
                                      bit_matrix_decl<false, true, false>,
                                      bit_matrix_decl<true, false, false>,
                                      bit_matrix_decl<true, false, true>,
                                      bit_matrix_decl<true, true, false>,
                                      adjacency_lists_decl>;
};

}  // namespace urd::detail
