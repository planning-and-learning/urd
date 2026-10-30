#pragma once

// What each operation costs each representation, as data: the complexity sections of docs/kinds/*.rst are written
// from it (share/urd/complexity.json, tests/complexity_table.cpp), the registry's choices are classified by it
// (tests/comparisons.hpp), and tests/complexity_verify.cpp checks it by counting the work done.

#include "names.hpp"
#include "urd/store/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace urd::test
{

/// The operations compared, the same for every kind; which a Store offers follows from its facets.
enum class op : std::uint8_t
{
    insert,          ///< a set's or map's insert, a sequence's push_back
    lookup,          ///< find, contains, a map's value by key
    ordered_lookup,  ///< lower_bound
    erase,           ///< by key, or by position
    clear,
    iterate,
    access,  ///< by position or handle
    push,    ///< a queue's
    pop,
    top,
    merge,       ///< a partition's
    insert_row,  ///< a pool's insert
    find_row,    ///< a pool's find
    scope,       ///< push_scope, changes, pop_scope
    count
};
inline constexpr std::array<std::string_view, static_cast<std::size_t>(op::count)> op_names {
    "insert", "lookup", "ordered_lookup", "erase", "clear", "iterate", "access", "push", "pop", "top", "merge", "insert_row", "find_row", "scope",
};

/// How a cost grows, in which quantity.
enum class growth : std::uint8_t
{
    one,
    log,
    linear,
    n_log_n,
};
enum class in : std::uint8_t
{
    n,         ///< the elements
    keys,      ///< U, the key range (dense keys fill it: about n)
    capacity,  ///< the slots held (about n over the load factor)
    row,       ///< r, a row's length
    delta,     ///< k, a delta's changes
    changes,   ///< the changes since a scope began
};

struct cost
{
    growth grows = growth::one;
    in of = in::n;
    bool amortized = false;
    bool expected = false;
    /// Verified from measured times (benchmarks/record.py) where no element operation shows the work: index and bit
    /// containers, clearing and iterating.
    bool timed = false;
    /// The cost of the representation this one wraps (urd::parts), for an operation it passes on.
    bool as_part = false;
};

namespace costs
{
inline constexpr cost one {};
inline constexpr cost one_a { .amortized = true };
inline constexpr cost one_e { .expected = true };
inline constexpr cost one_ae { .amortized = true, .expected = true };
inline constexpr cost one_t { .timed = true };
inline constexpr cost one_at { .amortized = true, .timed = true };
inline constexpr cost log_n { .grows = growth::log };
inline constexpr cost log_na { .grows = growth::log, .amortized = true };
inline constexpr cost lin_n { .grows = growth::linear };
inline constexpr cost lin_nt { .grows = growth::linear, .timed = true };
inline constexpr cost lin_keys { .grows = growth::linear, .of = in::keys, .timed = true };
inline constexpr cost lin_capacity { .grows = growth::linear, .of = in::capacity, .timed = true };
inline constexpr cost lin_row_e { .grows = growth::linear, .of = in::row, .expected = true };
inline constexpr cost lin_row_t { .grows = growth::linear, .of = in::row, .timed = true };
inline constexpr cost lin_changes { .grows = growth::linear, .of = in::changes };
inline constexpr cost part { .as_part = true };
}  // namespace costs

struct cost_entry
{
    std::string_view representation;  ///< name<R>::id
    op operation;
    cost how;
};

/// Every representation's costs, by its name (tests/names.hpp). An operation a representation lacks has no entry.
/// From the complexity sections of docs/kinds/*.rst; amortized and expected bounds count as their class.
inline constexpr auto cost_table = []
{
    using namespace costs;
    using enum op;
    return std::to_array<cost_entry>({
        // Hash tables: open addressing (Boost, gtl's shards) and nodes (the standard's).
        { "unordered_flat_set", insert, one_ae },
        { "unordered_flat_set", lookup, one_e },
        { "unordered_flat_set", erase, one_e },
        { "unordered_flat_set", clear, lin_capacity },
        { "unordered_flat_set", iterate, lin_capacity },
        { "unordered_flat_map", insert, one_ae },
        { "unordered_flat_map", lookup, one_e },
        { "unordered_flat_map", erase, one_e },
        { "unordered_flat_map", clear, lin_capacity },
        { "unordered_flat_map", iterate, lin_capacity },
        { "parallel_hash_set", insert, one_ae },
        { "parallel_hash_set", lookup, one_e },
        { "parallel_hash_set", erase, one_e },
        { "parallel_hash_set", clear, lin_capacity },
        { "parallel_hash_set", iterate, lin_capacity },
        { "parallel_hash_map", insert, one_ae },
        { "parallel_hash_map", lookup, one_e },
        { "parallel_hash_map", erase, one_e },
        { "parallel_hash_map", clear, lin_capacity },
        { "parallel_hash_map", iterate, lin_capacity },
        { "parallel_node_hash_set", insert, one_ae },
        { "parallel_node_hash_set", lookup, one_e },
        { "parallel_node_hash_set", erase, one_e },
        { "parallel_node_hash_set", clear, lin_capacity },
        { "parallel_node_hash_set", iterate, lin_capacity },
        { "parallel_node_hash_map", insert, one_ae },
        { "parallel_node_hash_map", lookup, one_e },
        { "parallel_node_hash_map", erase, one_e },
        { "parallel_node_hash_map", clear, lin_capacity },
        { "parallel_node_hash_map", iterate, lin_capacity },
        { "unordered_set", insert, one_ae },
        { "unordered_set", lookup, one_e },
        { "unordered_set", erase, one_e },
        { "unordered_set", clear, lin_nt },
        { "unordered_set", iterate, lin_nt },
        { "unordered_map", insert, one_ae },
        { "unordered_map", lookup, one_e },
        { "unordered_map", erase, one_e },
        { "unordered_map", clear, lin_nt },
        { "unordered_map", iterate, lin_nt },
        { "unordered_multimap", insert, one_ae },
        { "unordered_multimap", lookup, one_e },
        { "unordered_multimap", erase, one_e },
        { "unordered_multimap", clear, lin_nt },
        { "unordered_multimap", iterate, lin_nt },
        // Trees.
        { "set", insert, log_n },
        { "set", lookup, log_n },
        { "set", ordered_lookup, log_n },
        { "set", erase, log_n },
        { "set", clear, lin_nt },
        { "set", iterate, lin_nt },
        { "map", insert, log_n },
        { "map", lookup, log_n },
        { "map", ordered_lookup, log_n },
        { "map", erase, log_n },
        { "map", clear, lin_nt },
        { "map", iterate, lin_nt },
        { "multimap", insert, log_n },
        { "multimap", lookup, log_n },
        { "multimap", ordered_lookup, log_n },
        { "multimap", erase, log_n },
        { "multimap", clear, lin_nt },
        { "multimap", iterate, lin_nt },
        // Sorted vectors: an insert or erase inside shifts the elements after it.
        { "flat_set", insert, lin_n },
        { "flat_set", lookup, log_n },
        { "flat_set", ordered_lookup, log_n },
        { "flat_set", erase, lin_n },
        { "flat_set", clear, lin_nt },
        { "flat_set", iterate, lin_nt },
        { "flat_set", access, one_t },
        { "flat_multiset", insert, lin_n },
        { "flat_multiset", lookup, log_n },
        { "flat_multiset", ordered_lookup, log_n },
        { "flat_multiset", erase, lin_n },
        { "flat_multiset", clear, lin_nt },
        { "flat_multiset", iterate, lin_nt },
        { "flat_multiset", access, one_t },
        { "flat_map", insert, lin_n },
        { "flat_map", lookup, log_n },
        { "flat_map", ordered_lookup, log_n },
        { "flat_map", erase, lin_n },
        { "flat_map", clear, lin_nt },
        { "flat_map", iterate, lin_nt },
        { "flat_multimap", insert, lin_n },
        { "flat_multimap", lookup, log_n },
        { "flat_multimap", ordered_lookup, log_n },
        { "flat_multimap", erase, lin_n },
        { "flat_multimap", clear, lin_nt },
        { "flat_multimap", iterate, lin_nt },
        // Entries in an order of their values, found by key: a scan, for few entries.
        { "flat_entry_map", insert, lin_n },
        { "flat_entry_map", lookup, lin_n },
        { "flat_entry_map", erase, lin_n },
        { "flat_entry_map", clear, lin_nt },
        { "flat_entry_map", iterate, lin_nt },
        // Interners: a hash table of positions over the elements in order.
        { "indexed_set", insert, one_ae },
        { "indexed_set", lookup, one_e },
        { "indexed_set", erase, lin_n },
        { "indexed_set", clear, lin_capacity },
        { "indexed_set", iterate, lin_nt },
        { "indexed_set", access, one_t },
        { "stable_indexed_set", insert, one_ae },
        { "stable_indexed_set", lookup, one_e },
        { "stable_indexed_set", iterate, lin_nt },
        { "stable_indexed_set", access, one_t },
        { "concurrent_indexed_set", insert, one_ae },
        { "concurrent_indexed_set", lookup, one_e },
        { "concurrent_indexed_set", iterate, lin_nt },
        { "concurrent_indexed_set", access, one_t },
        // Dense keys: a bit or a slot per index; no element is hashed or compared.
        { "dense_set", insert, one_at },
        { "dense_set", lookup, one_t },
        { "dense_set", ordered_lookup, lin_keys },
        { "dense_set", erase, one_t },
        { "dense_set", clear, lin_keys },
        { "dense_set", iterate, lin_keys },
        { "tracked_dense_set", insert, one_at },
        { "tracked_dense_set", lookup, one_t },
        { "tracked_dense_set", ordered_lookup, lin_keys },
        { "tracked_dense_set", erase, one_t },
        { "tracked_dense_set", clear, lin_nt },
        { "tracked_dense_set", iterate, lin_keys },
        { "sparse_set", insert, one_at },
        { "sparse_set", lookup, one_t },
        { "sparse_set", erase, one_t },
        { "sparse_set", clear, one_t },
        { "sparse_set", iterate, lin_nt },
        { "ordered_sparse_set", insert, one_at },
        { "ordered_sparse_set", lookup, one_t },
        { "ordered_sparse_set", erase, lin_nt },
        { "ordered_sparse_set", clear, one_t },
        { "ordered_sparse_set", iterate, lin_nt },
        { "ordered_sparse_set", access, one_t },
        { "dense_map", insert, one_at },
        { "dense_map", lookup, one_t },
        { "dense_map", ordered_lookup, lin_keys },
        { "dense_map", erase, one_t },
        { "dense_map", clear, lin_keys },
        { "dense_map", iterate, lin_keys },
        { "stable_dense_map", insert, one_at },
        { "stable_dense_map", lookup, one_t },
        { "stable_dense_map", ordered_lookup, lin_keys },
        { "stable_dense_map", erase, one_t },
        { "stable_dense_map", clear, lin_keys },
        { "stable_dense_map", iterate, lin_keys },
        { "generation_map", insert, one_at },
        { "generation_map", lookup, one_t },
        { "generation_map", ordered_lookup, lin_keys },
        { "generation_map", erase, one_t },
        { "generation_map", clear, one_t },
        { "generation_map", iterate, lin_keys },
        { "stable_generation_map", insert, one_at },
        { "stable_generation_map", lookup, one_t },
        { "stable_generation_map", ordered_lookup, lin_keys },
        { "stable_generation_map", erase, one_t },
        { "stable_generation_map", clear, one_t },
        { "stable_generation_map", iterate, lin_keys },
        { "thread_safe_generation_map", insert, one_at },
        { "thread_safe_generation_map", lookup, one_t },
        { "thread_safe_generation_map", ordered_lookup, lin_keys },
        { "thread_safe_generation_map", erase, one_t },
        { "thread_safe_generation_map", clear, one_t },
        { "thread_safe_generation_map", iterate, lin_keys },
        { "csr_multimap", lookup, one_t },
        { "csr_multimap", ordered_lookup, lin_keys },
        { "csr_multimap", iterate, lin_keys },
        { "bidirectional_csr", lookup, one_t },
        { "bidirectional_csr", ordered_lookup, lin_keys },
        { "bidirectional_csr", iterate, lin_keys },
        { "dense_multimap", insert, one_at },
        { "dense_multimap", lookup, one_t },
        { "dense_multimap", ordered_lookup, lin_keys },
        { "dense_multimap", erase, lin_row_t },
        { "dense_multimap", clear, lin_keys },
        { "dense_multimap", iterate, lin_keys },
        { "posting_map", insert, one_ae },
        { "posting_map", lookup, one_e },
        { "posting_map", erase, lin_row_e },
        { "posting_map", clear, lin_capacity },
        { "posting_map", iterate, lin_nt },
        { "disjoint_sets", lookup, one_at },
        { "disjoint_sets", merge, one_at },
        // Sequences.
        { "vector", insert, one_a },
        { "vector", erase, lin_n },
        { "vector", clear, lin_nt },
        { "vector", iterate, lin_nt },
        { "vector", access, one_t },
        { "segmented_vector", insert, one_a },
        { "segmented_vector", iterate, lin_nt },
        { "segmented_vector", access, one_t },
        { "concurrent_segmented_vector", insert, one_a },
        { "concurrent_segmented_vector", iterate, lin_nt },
        { "concurrent_segmented_vector", access, one_t },
        { "row_pool", insert, one_a },
        { "row_pool", erase, one_t },
        { "row_pool", clear, lin_nt },
        { "row_pool", iterate, lin_nt },
        { "row_pool", access, one_t },
        { "jagged", insert, lin_row_t },
        { "jagged", erase, lin_nt },
        { "jagged", clear, lin_nt },
        { "jagged", iterate, lin_nt },
        { "jagged", access, one_t },
        { "tracked_jagged", iterate, lin_nt },
        { "tracked_jagged", access, one_t },
        // Queues.
        { "fifo", push, one_a },
        { "fifo", pop, one },
        { "fifo", top, one_t },
        { "fifo", clear, lin_nt },
        { "fifo", iterate, lin_nt },
        { "ring_fifo", push, one_a },
        { "ring_fifo", pop, one },
        { "ring_fifo", top, one_t },
        { "ring_fifo", clear, lin_nt },
        { "ring_fifo", iterate, lin_nt },
        { "lifo", push, one_a },
        { "lifo", pop, one },
        { "lifo", top, one_t },
        { "lifo", clear, lin_nt },
        { "lifo", iterate, lin_nt },
        { "heap", push, log_na },
        { "heap", pop, log_n },
        { "heap", top, one_t },
        { "heap", clear, lin_nt },
        { "heap", iterate, lin_nt },
        { "heap_by", push, log_na },
        { "heap_by", pop, log_n },
        { "heap_by", top, one_t },
        { "heap_by", clear, lin_nt },
        { "heap_by", iterate, lin_nt },
        { "addressable_heap", push, log_na },
        { "addressable_heap", pop, log_n },
        { "addressable_heap", top, one_t },
        { "addressable_heap", clear, lin_nt },
        { "addressable_heap", iterate, lin_nt },
        { "bucket_queue", push, one_at },
        { "bucket_queue", pop, one_at },
        { "bucket_queue", top, one_t },
        { "bucket_queue", clear, lin_keys },
        // Pools: a row is hashed and compared in full; a tree pool's row is its nodes.
        { "row_interner", insert_row, lin_row_e },
        { "row_interner", find_row, lin_row_e },
        { "row_interner", access, one_t },
        { "thread_safe_row_interner", insert_row, lin_row_e },
        { "thread_safe_row_interner", find_row, lin_row_e },
        { "thread_safe_row_interner", access, one_t },
        { "tree_vector_set", insert_row, lin_row_e },
        { "tree_vector_set", find_row, lin_row_e },
        { "tree_vector_set", access, lin_row_t },
        { "tree_bitset_set", insert_row, lin_row_e },
        { "tree_bitset_set", find_row, lin_row_e },
        { "tree_bitset_set", access, lin_row_t },
        { "record_interner", insert_row, lin_row_e },
        { "record_interner", find_row, lin_row_e },
        { "record_interner", access, one_t },
        { "stable_record_interner", insert_row, lin_row_e },
        { "stable_record_interner", find_row, lin_row_e },
        { "stable_record_interner", access, one_t },
        { "thread_safe_record_interner", insert_row, lin_row_e },
        { "thread_safe_record_interner", find_row, lin_row_e },
        { "thread_safe_record_interner", access, one_t },
        { "arena_interner", insert_row, lin_row_e },
        { "arena_interner", find_row, lin_row_e },
        { "arena_interner", access, one_t },
        { "thread_safe_arena_interner", insert_row, lin_row_e },
        { "thread_safe_arena_interner", find_row, lin_row_e },
        { "thread_safe_arena_interner", access, one_t },
        { "slot_vector", insert_row, one_at },
        { "slot_vector", access, one_t },
        { "thread_safe_slot_vector", insert_row, one_at },
        { "thread_safe_slot_vector", access, one_t },
        // Wrappers: what they pass on costs what their part costs; what they add is listed.
        { "layered", insert, part },
        { "layered", lookup, part },
        { "layered", erase, part },
        { "layered", clear, part },
        { "layered", iterate, part },
        { "layered_node_set", insert, part },
        { "layered_node_set", lookup, part },
        { "layered_node_set", erase, part },
        { "layered_node_set", clear, part },
        { "layered_node_set", iterate, part },
        { "layered_node_map", insert, part },
        { "layered_node_map", lookup, part },
        { "layered_node_map", erase, part },
        { "layered_node_map", clear, part },
        { "layered_node_map", iterate, part },
        { "layered_generation_map", insert, part },
        { "layered_generation_map", lookup, part },
        { "layered_generation_map", erase, part },
        { "layered_generation_map", clear, part },
        { "layered_generation_map", iterate, part },
        { "layered_pool", insert_row, part },
        { "layered_pool", find_row, part },
        { "layered_pool", access, part },
        { "change_logged", insert, part },
        { "change_logged", lookup, part },
        { "change_logged", erase, part },
        { "change_logged", clear, lin_nt },
        { "change_logged", iterate, part },
        { "change_logged_dense_set", insert, part },
        { "change_logged_dense_set", lookup, part },
        { "change_logged_dense_set", ordered_lookup, part },
        { "change_logged_dense_set", erase, part },
        { "change_logged_dense_set", clear, lin_nt },
        { "change_logged_dense_set", iterate, part },
        { "change_logged_tree_set", insert, part },
        { "change_logged_tree_set", lookup, part },
        { "change_logged_tree_set", ordered_lookup, part },
        { "change_logged_tree_set", erase, part },
        { "change_logged_tree_set", clear, lin_nt },
        { "change_logged_tree_set", iterate, part },
        { "change_logged_node_set", insert, part },
        { "change_logged_node_set", lookup, part },
        { "change_logged_node_set", erase, part },
        { "change_logged_node_set", clear, lin_nt },
        { "change_logged_node_set", iterate, part },
        { "recycling_hash_set", insert, part },
        { "recycling_hash_set", lookup, part },
        { "recycling_hash_set", erase, part },
        { "recycling_hash_set", clear, lin_nt },
        { "recycling_hash_set", iterate, part },
        { "recycling_tree_set", insert, part },
        { "recycling_tree_set", lookup, part },
        { "recycling_tree_set", ordered_lookup, part },
        { "recycling_tree_set", erase, part },
        { "recycling_tree_set", clear, lin_nt },
        { "recycling_tree_set", iterate, part },
        { "recycling_hash_map", insert, part },
        { "recycling_hash_map", lookup, part },
        { "recycling_hash_map", erase, part },
        { "recycling_hash_map", clear, lin_nt },
        { "recycling_hash_map", iterate, part },
        { "recycling_map", insert, part },
        { "recycling_map", lookup, part },
        { "recycling_map", ordered_lookup, part },
        { "recycling_map", erase, part },
        { "recycling_map", clear, lin_nt },
        { "recycling_map", iterate, part },
        { "scoped_map", insert, one_ae },
        { "scoped_map", lookup, one_e },
        { "scoped_map", erase, one_e },
        { "scoped_map", clear, lin_capacity },
        { "scoped_map", iterate, lin_capacity },
        { "scoped_map", scope, lin_changes },
        { "scoped_tree_map", insert, log_n },
        { "scoped_tree_map", lookup, log_n },
        { "scoped_tree_map", ordered_lookup, log_n },
        { "scoped_tree_map", erase, log_n },
        { "scoped_tree_map", clear, lin_nt },
        { "scoped_tree_map", iterate, lin_nt },
        { "scoped_tree_map", scope, lin_changes },
        { "scoped_dense_map", insert, one_at },
        { "scoped_dense_map", lookup, one_t },
        { "scoped_dense_map", ordered_lookup, lin_keys },
        { "scoped_dense_map", erase, one_t },
        { "scoped_dense_map", clear, lin_keys },
        { "scoped_dense_map", iterate, lin_keys },
        { "scoped_dense_map", scope, lin_changes },
        // A map whose keys are a set of its own (an interner, or a sparse set), the values beside them in order.
        { "ordered_map", insert, part },
        { "ordered_map", lookup, part },
        { "ordered_map", erase, lin_n },
        { "ordered_map", clear, part },
        { "ordered_map", iterate, lin_nt },
    });
}();

/// What each representation holds besides its elements, for the docs.
struct memory_note
{
    std::string_view representation;
    std::string_view holds;
};
inline constexpr auto memory_table = std::to_array<memory_note>({
    { "unordered_flat_set", "~n/0.875 slots + 1 byte each; retains" },
    { "unordered_flat_map", "~n/0.875 slots + 1 byte each; retains" },
    { "parallel_hash_set", "16 shards of open addressing, a lock each" },
    { "parallel_hash_map", "16 shards of open addressing, a lock each" },
    { "parallel_node_hash_set", "16 shards, a node per element; stable references" },
    { "parallel_node_hash_map", "16 shards, a node per entry; stable references" },
    { "unordered_set", "a node per element + buckets; stable references" },
    { "unordered_map", "a node per entry + buckets; stable references" },
    { "unordered_multimap", "a node per entry + buckets; stable references" },
    { "set", "a node per element; reclaims" },
    { "map", "a node per entry; reclaims" },
    { "multimap", "a node per entry; reclaims" },
    { "flat_set", "n elements in one array; retains" },
    { "flat_multiset", "n elements in one array; retains" },
    { "flat_map", "n entries in one array; retains" },
    { "flat_multimap", "n entries in one array; retains" },
    { "flat_entry_map", "n entries in one array; for few" },
    { "indexed_set", "n elements in order + a table of positions" },
    { "stable_indexed_set", "segments that never move + a table of positions" },
    { "concurrent_indexed_set", "segments that never move + a table of positions; new elements under a lock" },
    { "dense_set", "U bits" },
    { "tracked_dense_set", "U bits + the indices of the words in use" },
    { "sparse_set", "n members + U positions" },
    { "ordered_sparse_set", "n members in insertion order + U positions" },
    { "dense_map", "a slot per index" },
    { "stable_dense_map", "a slot per index, in segments that never move" },
    { "generation_map", "a slot and a generation per index" },
    { "stable_generation_map", "a slot and a generation per index, in segments that never move" },
    { "thread_safe_generation_map", "a slot, a generation and a lock byte per index, in segments" },
    { "csr_multimap", "n values + U + 1 offsets" },
    { "bidirectional_csr", "n values + U + 1 offsets, both ways" },
    { "dense_multimap", "a vector per index; retains" },
    { "posting_map", "a vector of values per key" },
    { "disjoint_sets", "a parent and a size per element" },
    { "vector", "one array; retains" },
    { "segmented_vector", "doubling segments; stable references" },
    { "concurrent_segmented_vector", "doubling segments; stable references; appends from threads" },
    { "row_pool", "rows kept with their storage" },
    { "jagged", "one offsets array per level" },
    { "tracked_jagged", "one array, + the rows handed out to change" },
    { "fifo", "a deque; stable references" },
    { "ring_fifo", "one vector as a ring; retains" },
    { "lifo", "a vector; retains" },
    { "heap", "a vector as a binary heap; retains" },
    { "heap_by", "a vector as a binary heap; retains" },
    { "addressable_heap", "a heap of handles + a slot per handle" },
    { "bucket_queue", "a vector per priority" },
    { "row_interner", "each distinct row once, in full" },
    { "thread_safe_row_interner", "each distinct row once, in full; inserts under a lock" },
    { "tree_vector_set", "nodes shared across rows: a delta adds about k log r nodes" },
    { "tree_bitset_set", "nodes shared across rows; all-zero subtrees take nothing" },
    { "record_interner", "each distinct record once, as one block of bytes" },
    { "stable_record_interner", "each distinct record once, in segments that never move" },
    { "thread_safe_record_interner", "each distinct record once; inserts under a lock" },
    { "arena_interner", "each distinct record once, serialized into an arena" },
    { "thread_safe_arena_interner", "each distinct record once, serialized; inserts under a lock" },
    { "slot_vector", "stable slots; freed slots reused first" },
    { "thread_safe_slot_vector", "stable slots; freed slots reused first, under a lock" },
    { "layered", "its own elements; the parent is not owned" },
    { "layered_node_set", "its own elements in nodes; the parent is not owned" },
    { "layered_node_map", "its own entries in nodes; the parent is not owned" },
    { "layered_generation_map", "its own slots; the parent is not owned" },
    { "layered_pool", "its own rows, indices after the parent's" },
    { "change_logged", "+ the elements added and removed" },
    { "change_logged_dense_set", "+ the elements added and removed" },
    { "change_logged_tree_set", "+ the elements added and removed" },
    { "change_logged_node_set", "+ the elements added and removed" },
    { "recycling_hash_set", "erased nodes kept for reuse" },
    { "recycling_tree_set", "erased nodes kept for reuse" },
    { "recycling_hash_map", "erased nodes and their values kept for reuse" },
    { "recycling_map", "erased nodes and their values kept for reuse" },
    { "scoped_map", "+ a log entry per change; pop_scope undoes them" },
    { "scoped_tree_map", "+ a log entry per change; pop_scope undoes them" },
    { "scoped_dense_map", "+ a log entry per change; pop_scope undoes them" },
    { "ordered_map", "its key set + n values in order" },
});

/// The cost of `what` in the representation named `representation`, unresolved (as_part stays as_part); none where it
/// lacks the operation.
constexpr std::optional<cost> table_cost(std::string_view representation, op what)
{
    for (const auto& each : cost_table)
        if (each.representation == representation && each.operation == what)
            return each.how;
    return std::nullopt;
}

/// The representation a wrapper passes operations on to: the first of urd::parts<R>; an ordered map's key set.
template<typename R, typename List = typename urd::parts<R>::type>
struct first_part
{
};
template<typename R, typename P, typename... Ps>
struct first_part<R, urd::detail::type_list<P, Ps...>>
{
    using type = P;
};
template<typename Keys, typename V>
struct first_part<urd::repr::OrderedMap<Keys, V>, urd::detail::type_list<>>
{
    using type = Keys;
};

/// The cost of `what` in R, with what a wrapper passes on resolved to its part.
template<typename R>
constexpr std::optional<cost> cost_of(op what)
{
    const auto found = table_cost(name<R>::id, what);
    if (!found || !found->as_part)
        return found;
    if constexpr (requires { typename first_part<R>::type; })
        return cost_of<typename first_part<R>::type>(what);
    else
        return std::nullopt;
}

/// The operations a Store of R for S offers, read from its facets (urd::detail::facet_switches); the facets not
/// listed here (sizing, equality, set algebra, ...) are not compared.
template<typename R, typename S>
constexpr std::array<bool, static_cast<std::size_t>(op::count)> offered()
{
    using urd::detail::facet_id;
    using on = urd::detail::facet_switches<R, S>;
    auto result = std::array<bool, static_cast<std::size_t>(op::count)> {};
    const auto offer = [&](op what) { result[static_cast<std::size_t>(what)] = true; };
    if (on::on(facet_id::growth))
        offer(op::insert);
    if (on::on(facet_id::removal))
    {
        offer(op::erase);
        offer(op::clear);
    }
    if (on::on(facet_id::lookup) || on::on(facet_id::values_by_key) || on::on(facet_id::multi_values))
        offer(op::lookup);
    if (on::on(facet_id::ordered_lookup))
        offer(op::ordered_lookup);
    if (on::on(facet_id::iteration))
        offer(op::iterate);
    if (on::on(facet_id::positions) || on::on(facet_id::shape))
        offer(op::access);
    if (on::on(facet_id::queue_access))
    {
        offer(op::push);
        offer(op::pop);
        offer(op::top);
        offer(op::clear);
    }
    if (on::on(facet_id::grouping))
    {
        offer(op::merge);
        offer(op::lookup);
    }
    if (on::on(facet_id::pool) || on::on(facet_id::records))
    {
        offer(op::insert_row);
        offer(op::find_row);
        offer(op::access);
    }
    if (on::on(facet_id::slots))
    {
        offer(op::insert_row);
        offer(op::access);
    }
    if (on::on(facet_id::scopes))
        offer(op::scope);
    return result;
}

}  // namespace urd::test
