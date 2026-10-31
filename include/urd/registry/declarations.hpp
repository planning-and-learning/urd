#pragma once

// Every container's declaration (urd/repr/*_fwd.hpp and the like), by node (the kind of Store it serves, each with its
// own request format), in the order of preference within the node, the first that serves a request being chosen
// (urd/registry/scan.hpp): the one source of that order. The order follows docs/BENCHMARKS.md:
// an open-addressing hash table is fastest at insert, lookup and clear-and-refill at every size, so it comes first
// wherever order is not asked for; node tables and segments remain for stable_refs; where order is asked for, a tree
// serves elements inserted over time. The dense containers come after the hash tables, and only a request that
// promises dense keys reaches them at all (dense is demanded); the indexed set, after them too, only a request for
// insertion order or positions. Frozen rows are flattened first; rows are kept for reuse only where their memory must
// be.

#include "urd/registry/declaration.hpp"

#include "urd/repr/wrappers/change_logged_fwd.hpp"
#include "urd/repr/values/dense_set_fwd.hpp"
#include "urd/repr/maps/dense_map_fwd.hpp"
#include "urd/repr/partitions/disjoint_sets_fwd.hpp"
#include "urd/repr/maps/entry_ordered_fwd.hpp"
#include "urd/repr/values/flat_set_fwd.hpp"
#include "urd/repr/maps/flat_map_fwd.hpp"
#include "urd/repr/values/unordered_flat_set_fwd.hpp"
#include "urd/repr/maps/unordered_flat_map_fwd.hpp"
#include "urd/repr/maps/grouped_fwd.hpp"
#include "urd/repr/values/indexed_fwd.hpp"
#include "urd/repr/wrappers/layered_fwd.hpp"
#include "urd/repr/maps/ordered_map_fwd.hpp"
#include "urd/repr/values/parallel_hash_set_fwd.hpp"
#include "urd/repr/maps/parallel_hash_map_fwd.hpp"
#include "urd/repr/pools/pools_fwd.hpp"
#include "urd/repr/slots/slots_fwd.hpp"
#include "urd/repr/queues/queues_fwd.hpp"
#include "urd/repr/records/records_fwd.hpp"
#include "urd/repr/wrappers/recycling_fwd.hpp"
#include "urd/repr/sequences/rows_fwd.hpp"
#include "urd/repr/wrappers/scoped_fwd.hpp"
#include "urd/repr/sequences/segmented_fwd.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/values/std_set.hpp"

#include <array>

namespace urd::detail
{

/// Sequences: the declarations, in order of preference.
struct sequence_node
{
    using declarations = type_list<jagged_decl,
                                   fixed_jagged_decl,
                                   tracked_jagged_decl,
                                   vector_decl,
                                   segmented_vector_decl,
                                   concurrent_segmented_vector_decl,
                                   row_pool_decl>;
};

/// Sets and multisets: values kept by value: the declarations, in order of preference.
struct values_node
{
    using declarations = type_list<unordered_flat_set_decl,
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
                                   layered_decl<unordered_flat_set_decl, false>,
                                   layered_decl<unordered_set_decl, false>,
                                   change_logged_decl<unordered_flat_set_decl>,
                                   change_logged_decl<tracked_dense_set_decl>,
                                   change_logged_decl<unordered_set_decl>,
                                   change_logged_decl<set_decl>,
                                   parallel_hash_set_decl,
                                   parallel_node_hash_set_decl,
                                   concurrent_indexed_set_decl>;
};

/// Maps and multimaps: the declarations, in order of preference.
struct map_node
{
    using declarations = type_list<unordered_flat_map_decl,
                                   unordered_map_decl,
                                   posting_map_decl,
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
                                   layered_decl<unordered_flat_map_decl, false>,
                                   layered_decl<unordered_map_decl, false>,
                                   layered_decl<dense_map_decl, false>,
                                   layered_decl<generation_map_decl, false>,
                                   flat_entry_map_decl>;
};

/// Queues: the declarations, in order of preference.
struct queue_node
{
    using declarations = type_list<fifo_decl,
                                   ring_fifo_decl,
                                   lifo_decl,
                                   bucket_queue_decl,
                                   heap_decl,
                                   addressable_heap_decl>;
};

/// Pools interning rows: the declarations, in order of preference.
struct pool_node
{
    using declarations = type_list<row_interner_decl,
                                   tree_pool_decl<false, false>,
                                   tree_pool_decl<true, false>,
                                   thread_safe_row_interner_decl,
                                   tree_pool_decl<false, true>,
                                   tree_pool_decl<true, true>,
                                   layered_decl<row_interner_decl, true>,
                                   layered_decl<thread_safe_row_interner_decl, true>>;
};

/// Pools of slots: the declarations, in order of preference.
struct slots_node
{
    using declarations = type_list<slot_vector_decl,
                                   thread_safe_slot_vector_decl>;
};

/// Pools of records stored flat: the declarations, in order of preference.
struct records_node
{
    using declarations = type_list<record_interner_decl,
                                   arena_interner_decl,
                                   stable_record_interner_decl,
                                   thread_safe_record_interner_decl,
                                   thread_safe_arena_interner_decl,
                                   layered_decl<record_interner_decl, true>,
                                   layered_decl<arena_interner_decl, true>,
                                   layered_decl<stable_record_interner_decl, true>,
                                   layered_decl<thread_safe_record_interner_decl, true>,
                                   layered_decl<thread_safe_arena_interner_decl, true>>;
};

/// Partitions: the declarations, in order of preference.
struct partition_node
{
    using declarations = type_list<disjoint_sets_decl>;
};

/// The node a request of kind K is found in: a set and a multiset are both values kept by value.
template<urd::kind K>
struct node_of_kind;
template<>
struct node_of_kind<urd::kind::sequence>
{
    using type = sequence_node;
};
template<>
struct node_of_kind<urd::kind::set>
{
    using type = values_node;
};
template<>
struct node_of_kind<urd::kind::multiset>
{
    using type = values_node;
};
template<>
struct node_of_kind<urd::kind::map>
{
    using type = map_node;
};
template<>
struct node_of_kind<urd::kind::queue>
{
    using type = queue_node;
};
template<>
struct node_of_kind<urd::kind::pool>
{
    using type = pool_node;
};
template<>
struct node_of_kind<urd::kind::slots>
{
    using type = slots_node;
};
template<>
struct node_of_kind<urd::kind::records>
{
    using type = records_node;
};
template<>
struct node_of_kind<urd::kind::partition>
{
    using type = partition_node;
};
template<typename S>
using node_t = typename node_of_kind<S::kind>::type;

/// Every node, for what walks them all (the tables and tests).
using nodes = type_list<sequence_node, values_node, map_node, queue_node, pool_node, slots_node, records_node, partition_node>;

}  // namespace urd::detail
