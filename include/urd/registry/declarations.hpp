#pragma once

// Every container's declaration (urd/repr/*_fwd.hpp and the like), in the order of preference, the first that serves a
// request being chosen (urd/registry/scan.hpp): the one source of that order. The order follows docs/BENCHMARKS.md:
// an open-addressing hash table is fastest at insert, lookup and clear-and-refill at every size, so it comes first
// wherever order is not asked for; node tables and segments remain for stable_refs; where order is asked for, a tree
// serves elements inserted over time. The dense containers come after the hash tables, and only a request that
// promises dense keys reaches them at all (dense is demanded); the indexed set, after them too, only a request for
// insertion order or positions. Frozen rows are flattened first; rows are kept for reuse only where their memory must
// be.

#include "urd/registry/declaration.hpp"

#include "urd/repr/change_logged_fwd.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/repr/disjoint_sets_fwd.hpp"
#include "urd/repr/entry_ordered_fwd.hpp"
#include "urd/repr/flat.hpp"
#include "urd/repr/flat_hash.hpp"
#include "urd/repr/grouped_fwd.hpp"
#include "urd/repr/indexed_fwd.hpp"
#include "urd/repr/layered_fwd.hpp"
#include "urd/repr/ordered_map_fwd.hpp"
#include "urd/repr/parallel.hpp"
#include "urd/repr/pools_fwd.hpp"
#include "urd/repr/queues_fwd.hpp"
#include "urd/repr/records_fwd.hpp"
#include "urd/repr/recycling_fwd.hpp"
#include "urd/repr/rows_fwd.hpp"
#include "urd/repr/scoped_fwd.hpp"
#include "urd/repr/segmented_fwd.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"

#include <array>

namespace urd::detail
{

/// The registered declarations, in order of preference.
using declarations = type_list<jagged_decl,
                               tracked_jagged_decl,
                               vector_decl,
                               unordered_flat_set_decl,
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
                               segmented_vector_decl,
                               concurrent_segmented_vector_decl,
                               row_pool_decl,
                               fifo_decl,
                               ring_fifo_decl,
                               lifo_decl,
                               bucket_queue_decl,
                               heap_decl,
                               addressable_heap_decl,
                               layered_decl<unordered_flat_set_decl, false>,
                               layered_decl<unordered_set_decl, false>,
                               change_logged_decl<unordered_flat_set_decl>,
                               change_logged_decl<tracked_dense_set_decl>,
                               change_logged_decl<unordered_set_decl>,
                               change_logged_decl<set_decl>,
                               row_interner_decl,
                               tree_pool_decl<false, false>,
                               tree_pool_decl<true, false>,
                               thread_safe_row_interner_decl,
                               tree_pool_decl<false, true>,
                               tree_pool_decl<true, true>,
                               record_interner_decl,
                               arena_interner_decl,
                               stable_record_interner_decl,
                               thread_safe_record_interner_decl,
                               thread_safe_arena_interner_decl,
                               layered_decl<row_interner_decl, true>,
                               layered_decl<record_interner_decl, true>,
                               layered_decl<arena_interner_decl, true>,
                               layered_decl<stable_record_interner_decl, true>,
                               layered_decl<thread_safe_row_interner_decl, true>,
                               layered_decl<thread_safe_record_interner_decl, true>,
                               layered_decl<thread_safe_arena_interner_decl, true>,
                               parallel_hash_set_decl,
                               parallel_node_hash_set_decl,
                               concurrent_indexed_set_decl,
                               slot_vector_decl,
                               thread_safe_slot_vector_decl,
                               disjoint_sets_decl,
                               unordered_flat_map_decl,
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

}  // namespace urd::detail
