#pragma once

// Every registered container as a row (urd/registry/row.hpp), in the registry's order of preference: the same lists
// as urd/registry/preferences.hpp, entry for entry, so that choosing by value agrees with choosing by type
// (tests/registry.cpp checks it for every request of the grid). A row's properties are those its container_traits
// declare, for the elements its constraints accept: a primitive row reads the features of the type it takes, a
// wrapper's row its part's. One header for now; with the declarations folded into each container's own header
// (claude.layout.md, step 5), each row moves beside its container.

#include "urd/registry/row.hpp"

#include <array>
#include <optional>

namespace urd::detail::rows
{

using P = std::optional<property_set>;
using T = type_features;
using F = request_features;

constexpr tag threads(bool concurrent_) { return concurrent_ ? tag::concurrent : tag::single_threaded; }
constexpr tag references(bool stable) { return stable ? tag::stable_refs : tag::moving_refs; }

// Sets and maps over urd::Less (`ordered` false) or over the request's comparator (true).
constexpr bool orders(const T& t, bool ordered) { return ordered ? t.ordered_by_request : t.orderable; }
constexpr P tree_set(const T& t, bool ordered)
{
    if (!orders(t, ordered))
        return {};
    return props({ tag::sorted, tag::unique, tag::stable_refs, tag::sparse, tag::shrinkable, tag::reclaims, tag::proportional_clear });
}
constexpr P hash_set(const T& t)
{
    if (!t.hashable)
        return {};
    return props({ tag::unique, tag::stable_refs, tag::sparse, tag::shrinkable });
}
constexpr P flat_set(const T& t, bool ordered)
{
    if (!orders(t, ordered))
        return {};
    return props(
        { tag::sorted, tag::unique, tag::positional, tag::sparse, tag::contiguous, tag::canonical, tag::shrinkable, tag::retains, tag::proportional_clear });
}
constexpr P flat_multiset(const T& t, bool ordered)
{
    if (!orders(t, ordered))
        return {};
    return props({ tag::sorted, tag::positional, tag::contiguous, tag::canonical, tag::shrinkable, tag::retains, tag::proportional_clear });
}
constexpr P flat_hash_set(const T& t)
{
    if (!t.hashable)
        return {};
    return props({ tag::unique, tag::sparse, tag::shrinkable, tag::retains });
}
constexpr P parallel_hash_set(const T& t, bool nodes)
{
    if (!t.hashable)
        return {};
    return with(props({ tag::unique, tag::sparse, tag::shrinkable, tag::concurrent }), { references(nodes) });
}
constexpr P tree_map(const T& key, bool ordered)
{
    if (!orders(key, ordered))
        return {};
    return props({ tag::sorted, tag::unique, tag::functional, tag::stable_refs, tag::sparse, tag::shrinkable, tag::reclaims, tag::proportional_clear });
}
constexpr P tree_multimap(const T& key, bool ordered)
{
    if (!orders(key, ordered))
        return {};
    return props({ tag::sorted, tag::multi, tag::stable_refs, tag::shrinkable, tag::reclaims, tag::proportional_clear });
}
constexpr P hash_map(const T& key)
{
    if (!key.hashable)
        return {};
    return props({ tag::unique, tag::functional, tag::stable_refs, tag::sparse, tag::shrinkable });
}
constexpr P hash_multimap(const T& key)
{
    if (!key.hashable)
        return {};
    return props({ tag::multi, tag::stable_refs, tag::shrinkable });
}
constexpr P flat_map(const T& key, bool ordered)
{
    if (!orders(key, ordered))
        return {};
    return props({ tag::sorted,
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
constexpr P flat_multimap(const T& key, bool ordered)
{
    if (!orders(key, ordered))
        return {};
    return props({ tag::sorted, tag::multi, tag::positional, tag::contiguous, tag::shrinkable, tag::retains, tag::proportional_clear });
}
constexpr P flat_hash_map(const T& key)
{
    if (!key.hashable)
        return {};
    return props({ tag::unique, tag::functional, tag::sparse, tag::shrinkable, tag::retains });
}
constexpr P parallel_hash_map(const T& key, bool nodes)
{
    if (!key.hashable)
        return {};
    return with(props({ tag::unique, tag::functional, tag::sparse, tag::shrinkable, tag::concurrent }), { references(nodes) });
}

// Sequences.
constexpr P vector() { return props({ tag::dense, tag::positional, tag::contiguous, tag::shrinkable, tag::retains, tag::proportional_clear }); }
constexpr P segmented_vector(bool concurrent_)
{ return props({ tag::dense, tag::positional, tag::stable_refs, tag::growable, tag::retains, tag::proportional_clear, threads(concurrent_) }); }

// Queues.
constexpr P fifo() { return props({ tag::fifo, tag::stable_refs, tag::shrinkable }); }
constexpr P lifo() { return props({ tag::lifo, tag::shrinkable, tag::retains }); }
constexpr P ring_fifo() { return props({ tag::fifo, tag::shrinkable, tag::retains }); }
constexpr P heap(const T& t, bool ordered)
{
    if (!orders(t, ordered))
        return {};
    return props({ tag::by_priority, tag::shrinkable, tag::retains });
}
constexpr P addressable_heap(const T& t, bool ordered)
{
    if (!orders(t, ordered))
        return {};
    return props({ tag::by_priority, tag::handles, tag::shrinkable, tag::retains });
}
constexpr P bucket_queue(const T& t, bool ordered)
{
    if (!orders(t, ordered) || !(ordered ? t.integral_priority_by_request : t.integral_priority))
        return {};
    return props({ tag::by_priority, tag::shrinkable, tag::retains });
}

// Dense keys.
constexpr P dense_set(const T& t, bool track)
{
    if (!t.indexable)
        return {};
    return props({ tag::sorted, tag::unique, tag::dense, tag::canonical, tag::shrinkable, tag::retains, track ? tag::proportional_clear : tag::any_clear });
}
constexpr P sparse_set(const T& t, bool keep_order)
{
    if (!t.indexable)
        return {};
    return props({ tag::unique,
                   tag::dense,
                   tag::positional,
                   tag::contiguous,
                   tag::shrinkable,
                   tag::retains,
                   t.trivially_destructible ? tag::constant_clear : tag::proportional_clear,
                   keep_order ? tag::insertion_ordered : tag::any_order });
}
constexpr P index_map(const T& key, bool stable)
{
    if (!key.indexable)
        return {};
    return props({ tag::sorted, tag::unique, tag::functional, tag::dense, tag::shrinkable, tag::retains, references(stable) });
}
constexpr P generation_map(const T& key, const T& mapped, bool concurrent_, bool stable)
{
    if (!key.indexable || !mapped.slot_value)
        return {};
    return props({ tag::sorted,
                   tag::unique,
                   tag::functional,
                   tag::dense,
                   tag::shrinkable,
                   tag::retains,
                   tag::constant_clear,
                   references(stable),
                   threads(concurrent_) });
}
constexpr P csr_multimap(const T& key)
{
    if (!key.indexable)
        return {};
    return props({ tag::sorted, tag::multi, tag::dense, tag::canonical });
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
    return props({ tag::sorted, tag::multi, tag::dense, tag::shrinkable, tag::retains });
}
constexpr P disjoint_sets(const T& t)
{
    if (!t.indexable)
        return {};
    return props({ tag::partition, tag::dense, tag::shrinkable, tag::retains });
}

// Hashed keys in insertion order, and postings.
constexpr P indexed_set(const T& t)
{
    if (!t.hashable)
        return {};
    return props({ tag::insertion_ordered, tag::unique, tag::positional, tag::contiguous, tag::shrinkable, tag::retains });
}
constexpr P stable_indexed_set(const T& t, bool concurrent_)
{
    if (!t.hashable)
        return {};
    return with(props({ tag::insertion_ordered, tag::unique, tag::positional, tag::stable_refs, tag::growable, tag::retains }), { threads(concurrent_) });
}
constexpr P posting_map(const T& key)
{
    if (!key.hashable)
        return {};
    return props({ tag::multi, tag::sparse, tag::shrinkable });
}
constexpr P flat_entry_map(const T& key)
{
    if (!key.orderable)
        return {};
    return props({ tag::sorted, tag::unique, tag::functional, tag::positional, tag::contiguous, tag::shrinkable, tag::retains });
}

// Pools.
constexpr P record_interner(const T& t, bool concurrent_, bool stable)
{
    if (!t.trivially_copyable || !t.hashable)
        return {};
    return props({ tag::handles, tag::unique, tag::flat, tag::shrinkable, tag::retains, references(stable), threads(concurrent_) });
}
constexpr P arena_interner(const T& t, bool concurrent_)
{
    if (!t.flat_record)
        return {};
    return props({ tag::handles, tag::unique, tag::flat, tag::stable_refs, tag::shrinkable, tag::retains, threads(concurrent_) });
}
constexpr P row_interner(const T& row, bool concurrent_)
{
    if (!row.row_vector_hashable)
        return {};
    return props({ tag::handles, tag::unique, tag::shrinkable, tag::retains, threads(concurrent_) });
}
constexpr P tree_vector_set(const T& row, bool concurrent_)
{
    if (!row.row_element_hashable || keeps_by_value(row.store_kind))
        return {};
    return props({ tag::handles, tag::unique, tag::structural_sharing, tag::shrinkable, tag::retains, threads(concurrent_) });
}
constexpr P tree_bitset_set(const T& row, bool concurrent_)
{
    if (!row.row_element_indexable || !row.store_properties.has(tag::unique) || !row.store_properties.has(tag::dense))
        return {};
    return props({ tag::handles, tag::unique, tag::structural_sharing, tag::shrinkable, tag::retains, threads(concurrent_) });
}
constexpr P slot_vector(bool concurrent_) { return props({ tag::handles, tag::stable_refs, tag::shrinkable, tag::retains, threads(concurrent_) }); }
/// What an interning pool of rows gives its rows: their own properties.
constexpr P own_rows(const T& row) { return row.store ? P(row.store_properties) : P(); }

// Rows.
constexpr P jagged(const T& inner, bool track)
{
    if (!track && inner.flattenable)
        return props({ tag::dense, tag::positional, tag::shrinkable, tag::retains, tag::proportional_clear });
    if (inner.fixed_rows)
        return props({ tag::dense, tag::positional, tag::fixed_shape, tag::retains, track ? tag::proportional_clear : tag::any_clear });
    return {};
}
constexpr P jagged_rows(const T& inner, bool track)
{
    if (!track && inner.flattenable)
        return with(inner.store_properties, { inner.store_of_stores ? tag::any_layout : tag::contiguous, tag::moving_refs, tag::retains });
    if (inner.fixed_rows)
        return with(inner.store_properties,
                    { (!inner.store_of_stores && !keeps_by_value(inner.store_kind)) ? tag::contiguous : tag::any_layout, tag::moving_refs, tag::retains });
    return {};
}
constexpr P row_pool(const T& inner)
{
    if (!inner.store || !inner.store_properties.has(tag::shrinkable))
        return {};
    return props({ tag::dense, tag::positional, tag::contiguous, tag::shrinkable, tag::retains, tag::proportional_clear });
}

// Wrappers, over their part's properties.
constexpr P layered(const P& part)
{
    if (!part || !part->has(tag::unique))
        return {};
    return with(*part, { tag::layered, tag::any_order });
}
constexpr P layered_pool(const P& part)
{
    if (!part || !part->has(tag::handles) || !part->has(tag::unique))
        return {};
    return with(*part, { tag::layered });
}
constexpr P change_logged(const P& part)
{
    if (!part || !part->has(tag::unique))
        return {};
    return part->has(tag::dense) ? with(*part, { tag::change_log, tag::any_clear }) : with(*part, { tag::change_log });
}
constexpr P scoped(const P& part)
{
    if (!part || !part->has(tag::unique) || !part->has(tag::functional))
        return {};
    return with(*part, { tag::scoped });
}
constexpr P recycling(const P& part)
{
    if (!part)
        return {};
    return with(*part, { tag::retains });
}
constexpr P recycling_set(const P& part)
{
    if (!part || !part->has(tag::unique) || !part->has(tag::stable_refs))
        return {};
    return with(*part, { tag::retains });
}
constexpr P ordered_map(const P& keys, const T& mapped)
{
    if (!keys || !keys->has(tag::insertion_ordered) || !keys->has(tag::unique))
        return {};
    const auto clear = (mapped.trivially_destructible && keys->has(tag::constant_clear)) ? tag::constant_clear :
                       keys->has(tag::proportional_clear)                                ? tag::proportional_clear :
                                                                                           tag::any_clear;
    return with(*keys, { tag::functional, tag::by_key, tag::any_layout, clear });
}

}  // namespace urd::detail::rows

namespace urd::detail
{

/// Every registered container, in the registry's order of preference (the lists of urd/registry/preferences.hpp,
/// entry for entry: element_preference, then map_preference, then value_ordered_map_preference). A row whose
/// container takes a comparator serves both urd::Less and an order_by over its elements.
inline constexpr auto registry_table = std::to_array<registry_row>({
    { "jagged",
      { "urd/repr/jagged.hpp" },
      kind::sequence,
      natural_order_only,
      [](const request_features& f) { return rows::jagged(f.element, false); },
      [](const request_features& f) { return rows::jagged_rows(f.element, false); } },
    { "tracked_jagged",
      { "urd/repr/jagged.hpp" },
      kind::sequence,
      natural_order_only,
      [](const request_features& f) { return rows::jagged(f.element, true); },
      [](const request_features& f) { return rows::jagged_rows(f.element, true); } },
    { "vector", { "urd/repr/sequence.hpp" }, kind::sequence, natural_order_only, [](const request_features&) { return rows::vector(); } },
    { "unordered_flat_set",
      { "urd/repr/unordered_flat_set.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::flat_hash_set(f.element); } },
    { "unordered_set", { "urd/repr/std_set.hpp" }, kind::set, natural_order_only, [](const request_features& f) { return rows::hash_set(f.element); } },
    { "set", { "urd/repr/std_set.hpp" }, kind::set, element_orders, [](const request_features& f) { return rows::tree_set(f.element, f.by != order::none); } },
    { "flat_multiset",
      { "urd/repr/flat_set.hpp" },
      kind::multiset,
      element_orders,
      [](const request_features& f) { return rows::flat_multiset(f.element, f.by != order::none); } },
    { "indexed_set", { "urd/repr/indexed.hpp" }, kind::set, natural_order_only, [](const request_features& f) { return rows::indexed_set(f.element); } },
    { "flat_set",
      { "urd/repr/flat_set.hpp" },
      kind::set,
      element_orders,
      [](const request_features& f) { return rows::flat_set(f.element, f.by != order::none); } },
    { "recycling_hash_set",
      { "urd/repr/std_set.hpp", "urd/repr/recycling.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::recycling_set(rows::hash_set(f.element)); } },
    { "recycling_tree_set",
      { "urd/repr/std_set.hpp", "urd/repr/recycling.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::recycling_set(rows::tree_set(f.element, f.by != order::none)); } },
    { "dense_set", { "urd/repr/dense_set.hpp" }, kind::set, natural_order_only, [](const request_features& f) { return rows::dense_set(f.element, false); } },
    { "tracked_dense_set",
      { "urd/repr/dense_set.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::dense_set(f.element, true); } },
    { "sparse_set",
      { "urd/repr/sparse_set.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::sparse_set(f.element, false); } },
    { "stable_indexed_set",
      { "urd/repr/indexed.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::stable_indexed_set(f.element, false); } },
    { "ordered_sparse_set",
      { "urd/repr/sparse_set.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::sparse_set(f.element, true); } },
    { "segmented_vector",
      { "urd/repr/segmented.hpp" },
      kind::sequence,
      natural_order_only,
      [](const request_features&) { return rows::segmented_vector(false); } },
    { "concurrent_segmented_vector",
      { "urd/repr/segmented.hpp" },
      kind::sequence,
      natural_order_only,
      [](const request_features&) { return rows::segmented_vector(true); } },
    { "row_pool",
      { "urd/repr/row_pool.hpp" },
      kind::sequence,
      natural_order_only,
      [](const request_features& f) { return rows::row_pool(f.element); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "fifo", { "urd/repr/queue.hpp" }, kind::queue, natural_order_only, [](const request_features&) { return rows::fifo(); } },
    { "ring_fifo", { "urd/repr/ring_fifo.hpp" }, kind::queue, natural_order_only, [](const request_features&) { return rows::ring_fifo(); } },
    { "lifo", { "urd/repr/queue.hpp" }, kind::queue, natural_order_only, [](const request_features&) { return rows::lifo(); } },
    { "bucket_queue",
      { "urd/repr/bucket_queue.hpp" },
      kind::queue,
      element_orders,
      [](const request_features& f) { return rows::bucket_queue(f.element, f.by != order::none); },
      nullptr,
      property_set::of({ tag::monotone_priority }) },
    { "heap", { "urd/repr/queue.hpp" }, kind::queue, element_orders, [](const request_features& f) { return rows::heap(f.element, f.by != order::none); } },
    { "addressable_heap",
      { "urd/repr/addressable_heap.hpp" },
      kind::queue,
      element_orders,
      [](const request_features& f) { return rows::addressable_heap(f.element, f.by != order::none); } },
    { "layered_unordered_flat_set",
      { "urd/repr/unordered_flat_set.hpp", "urd/repr/layer.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::layered(rows::flat_hash_set(f.element)); } },
    { "layered_unordered_set",
      { "urd/repr/std_set.hpp", "urd/repr/layer.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::layered(rows::hash_set(f.element)); } },
    { "change_logged_unordered_flat_set",
      { "urd/repr/unordered_flat_set.hpp", "urd/repr/change_logged.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::change_logged(rows::flat_hash_set(f.element)); } },
    { "change_logged_tracked_dense_set",
      { "urd/repr/dense_set.hpp", "urd/repr/change_logged.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::change_logged(rows::dense_set(f.element, true)); } },
    { "change_logged_unordered_set",
      { "urd/repr/std_set.hpp", "urd/repr/change_logged.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::change_logged(rows::hash_set(f.element)); } },
    { "change_logged_set",
      { "urd/repr/std_set.hpp", "urd/repr/change_logged.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::change_logged(rows::tree_set(f.element, f.by != order::none)); } },
    { "row_interner",
      { "urd/repr/pools.hpp" },
      kind::pool,
      natural_order_only,
      [](const request_features& f) { return rows::row_interner(f.element, false); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "tree_vector_set",
      { "urd/repr/tree_vector.hpp" },
      kind::pool,
      natural_order_only,
      [](const request_features& f) { return rows::tree_vector_set(f.element, false); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "tree_bitset_set",
      { "urd/repr/tree_bitset.hpp" },
      kind::pool,
      natural_order_only,
      [](const request_features& f) { return rows::tree_bitset_set(f.element, false); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "thread_safe_row_interner",
      { "urd/repr/pools.hpp" },
      kind::pool,
      natural_order_only,
      [](const request_features& f) { return rows::row_interner(f.element, true); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "thread_safe_tree_vector_set",
      { "urd/repr/tree_vector.hpp" },
      kind::pool,
      natural_order_only,
      [](const request_features& f) { return rows::tree_vector_set(f.element, true); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "thread_safe_tree_bitset_set",
      { "urd/repr/tree_bitset.hpp" },
      kind::pool,
      natural_order_only,
      [](const request_features& f) { return rows::tree_bitset_set(f.element, true); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "record_interner",
      { "urd/repr/records.hpp" },
      kind::records,
      natural_order_only,
      [](const request_features& f) { return rows::record_interner(f.element, false, false); } },
    { "arena_interner",
      { "urd/repr/arena.hpp" },
      kind::records,
      natural_order_only,
      [](const request_features& f) { return rows::arena_interner(f.element, false); } },
    { "stable_record_interner",
      { "urd/repr/records.hpp" },
      kind::records,
      natural_order_only,
      [](const request_features& f) { return rows::record_interner(f.element, false, true); } },
    { "thread_safe_record_interner",
      { "urd/repr/records.hpp" },
      kind::records,
      natural_order_only,
      [](const request_features& f) { return rows::record_interner(f.element, true, false); } },
    { "thread_safe_arena_interner",
      { "urd/repr/arena.hpp" },
      kind::records,
      natural_order_only,
      [](const request_features& f) { return rows::arena_interner(f.element, true); } },
    { "layered_row_interner",
      { "urd/repr/pools.hpp", "urd/repr/layered_pool.hpp" },
      kind::pool,
      natural_order_only,
      [](const request_features& f) { return rows::layered_pool(rows::row_interner(f.element, false)); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "layered_record_interner",
      { "urd/repr/records.hpp", "urd/repr/layered_pool.hpp" },
      kind::records,
      natural_order_only,
      [](const request_features& f) { return rows::layered_pool(rows::record_interner(f.element, false, false)); } },
    { "layered_arena_interner",
      { "urd/repr/arena.hpp", "urd/repr/layered_pool.hpp" },
      kind::records,
      natural_order_only,
      [](const request_features& f) { return rows::layered_pool(rows::arena_interner(f.element, false)); } },
    { "layered_stable_record_interner",
      { "urd/repr/records.hpp", "urd/repr/layered_pool.hpp" },
      kind::records,
      natural_order_only,
      [](const request_features& f) { return rows::layered_pool(rows::record_interner(f.element, false, true)); } },
    { "layered_thread_safe_row_interner",
      { "urd/repr/pools.hpp", "urd/repr/layered_pool.hpp" },
      kind::pool,
      natural_order_only,
      [](const request_features& f) { return rows::layered_pool(rows::row_interner(f.element, true)); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "layered_thread_safe_record_interner",
      { "urd/repr/records.hpp", "urd/repr/layered_pool.hpp" },
      kind::records,
      natural_order_only,
      [](const request_features& f) { return rows::layered_pool(rows::record_interner(f.element, true, false)); } },
    { "layered_thread_safe_arena_interner",
      { "urd/repr/arena.hpp", "urd/repr/layered_pool.hpp" },
      kind::records,
      natural_order_only,
      [](const request_features& f) { return rows::layered_pool(rows::arena_interner(f.element, true)); } },
    { "parallel_hash_set",
      { "urd/repr/parallel_hash.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::parallel_hash_set(f.element, false); } },
    { "parallel_node_hash_set",
      { "urd/repr/parallel_hash.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::parallel_hash_set(f.element, true); } },
    { "concurrent_indexed_set",
      { "urd/repr/indexed.hpp" },
      kind::set,
      natural_order_only,
      [](const request_features& f) { return rows::stable_indexed_set(f.element, true); } },
    { "slot_vector", { "urd/repr/slots.hpp" }, kind::pool, natural_order_only, [](const request_features&) { return rows::slot_vector(false); } },
    { "thread_safe_slot_vector", { "urd/repr/slots.hpp" }, kind::pool, natural_order_only, [](const request_features&) { return rows::slot_vector(true); } },
    { "disjoint_sets",
      { "urd/repr/disjoint_sets.hpp" },
      kind::partition,
      natural_order_only,
      [](const request_features& f) { return rows::disjoint_sets(f.element); } },
    { "unordered_flat_map",
      { "urd/repr/unordered_flat_map.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::flat_hash_map(f.key); } },
    { "unordered_map", { "urd/repr/std_map.hpp" }, kind::map, natural_order_only, [](const request_features& f) { return rows::hash_map(f.key); } },
    { "posting_map", { "urd/repr/postings.hpp" }, kind::map, natural_order_only, [](const request_features& f) { return rows::posting_map(f.key); } },
    { "unordered_multimap", { "urd/repr/std_map.hpp" }, kind::map, natural_order_only, [](const request_features& f) { return rows::hash_multimap(f.key); } },
    { "parallel_hash_map",
      { "urd/repr/parallel_hash.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::parallel_hash_map(f.key, false); } },
    { "parallel_node_hash_map",
      { "urd/repr/parallel_hash.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::parallel_hash_map(f.key, true); } },
    { "map", { "urd/repr/std_map.hpp" }, kind::map, element_orders, [](const request_features& f) { return rows::tree_map(f.key, f.by != order::none); } },
    { "multimap",
      { "urd/repr/std_map.hpp" },
      kind::map,
      element_orders,
      [](const request_features& f) { return rows::tree_multimap(f.key, f.by != order::none); } },
    { "flat_multimap",
      { "urd/repr/flat_map.hpp" },
      kind::map,
      element_orders,
      [](const request_features& f) { return rows::flat_multimap(f.key, f.by != order::none); } },
    { "flat_map",
      { "urd/repr/flat_map.hpp" },
      kind::map,
      element_orders,
      [](const request_features& f) { return rows::flat_map(f.key, f.by != order::none); } },
    { "recycling_hash_map",
      { "urd/repr/std_map.hpp", "urd/repr/recycling.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::recycling(rows::hash_map(f.key)); },
      [](const request_features& f) { return rows::own_rows(f.mapped); } },
    { "recycling_map",
      { "urd/repr/std_map.hpp", "urd/repr/recycling.hpp" },
      kind::map,
      element_orders,
      [](const request_features& f) { return rows::recycling(rows::tree_map(f.key, f.by != order::none)); },
      [](const request_features& f) { return rows::own_rows(f.mapped); } },
    { "dense_map", { "urd/repr/index_map.hpp" }, kind::map, natural_order_only, [](const request_features& f) { return rows::index_map(f.key, false); } },
    { "stable_dense_map", { "urd/repr/index_map.hpp" }, kind::map, natural_order_only, [](const request_features& f) { return rows::index_map(f.key, true); } },
    { "generation_map",
      { "urd/repr/index_map.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::generation_map(f.key, f.mapped, false, false); } },
    { "stable_generation_map",
      { "urd/repr/index_map.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::generation_map(f.key, f.mapped, false, true); } },
    { "thread_safe_generation_map",
      { "urd/repr/index_map.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::generation_map(f.key, f.mapped, true, true); } },
    { "ordered_map",
      { "urd/repr/indexed.hpp", "urd/repr/ordered_map.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::ordered_map(rows::indexed_set(f.key), f.mapped); } },
    { "dense_ordered_map",
      { "urd/repr/sparse_set.hpp", "urd/repr/ordered_map.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::ordered_map(rows::sparse_set(f.key, true), f.mapped); } },
    { "csr_multimap", { "urd/repr/csr_multimap.hpp" }, kind::map, natural_order_only, [](const request_features& f) { return rows::csr_multimap(f.key); } },
    { "bidirectional_csr",
      { "urd/repr/bidirectional_csr.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::bidirectional_csr(f.key, f.mapped); } },
    { "dense_multimap",
      { "urd/repr/dense_multimap.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::dense_multimap(f.key); } },
    { "scoped_map",
      { "urd/repr/unordered_flat_map.hpp", "urd/repr/scoped.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::scoped(rows::flat_hash_map(f.key)); } },
    { "scoped_tree_map",
      { "urd/repr/std_map.hpp", "urd/repr/scoped.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::scoped(rows::tree_map(f.key, f.by != order::none)); } },
    { "scoped_dense_map",
      { "urd/repr/index_map.hpp", "urd/repr/scoped.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::scoped(rows::index_map(f.key, false)); } },
    { "layered_unordered_flat_map",
      { "urd/repr/unordered_flat_map.hpp", "urd/repr/layer.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::layered(rows::flat_hash_map(f.key)); } },
    { "layered_unordered_map",
      { "urd/repr/std_map.hpp", "urd/repr/layer.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::layered(rows::hash_map(f.key)); } },
    { "layered_dense_map",
      { "urd/repr/index_map.hpp", "urd/repr/layer.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::layered(rows::index_map(f.key, false)); } },
    { "layered_generation_map",
      { "urd/repr/index_map.hpp", "urd/repr/layer.hpp" },
      kind::map,
      natural_order_only,
      [](const request_features& f) { return rows::layered(rows::generation_map(f.key, f.mapped, false, false)); } },
    { "flat_entry_map", { "urd/repr/entry_ordered.hpp" }, kind::map, entry_orders, [](const request_features& f) { return rows::flat_entry_map(f.key); } },
});

}  // namespace urd::detail

namespace urd::detail
{

/// What a request is, as selection reads it: its kind, its properties, and what a container must guarantee for them
/// (requirement<S>).
struct request_shape
{
    urd::kind kind = urd::kind::sequence;
    property_set stated;
    property_set required;
};

/// Whether `row` serves a request of shape `s` with features `f`: serves<R, S> (urd/registry/candidates.hpp), as values.
constexpr bool row_serves(const registry_row& row, const request_shape& s, const request_features& f)
{
    if (!holds_kind(row.kind, s.kind) || !accepts_order(row.orders, f.by))
        return false;
    const auto traits = row.traits(f);
    if (!traits)
        return false;
    const auto& have = *traits;
    if (!satisfies(have, s.required) || !demands_met(have, s.stated) || have.has(tag::unique) != s.stated.has(tag::unique)
        || have.has(tag::handles) != s.stated.has(tag::handles) || (s.kind == urd::kind::multiset && !s.stated.has(tag::sorted))
        || !promises(s.stated, row.relies_on) || (s.stated.has(tag::positional) && !have.has(tag::positional)))
        return false;
    const auto& held = s.kind == urd::kind::map ? f.mapped : f.element;
    if (!held.store)
        return true;
    const auto given = row.rows ? row.rows(f) : std::optional<property_set>(with(held.store_properties, { tag::reclaims }));
    return given && satisfies(*given, held.store_requirement);
}

/// The first row of `table` serving the request, or none. Where elements are few or never
/// change, rows with stable storage come last (reordered<> in urd/registry/candidates.hpp).
template<std::size_t N>
constexpr const registry_row* first_serving_row(const std::array<registry_row, N>& table, const request_shape& s, const request_features& f)
{
    const bool compact = s.stated.has(tag::frozen) || s.stated.has(tag::small);
#if defined(URD_REGISTRY_COPIES)
    // For tests/scaling only: the registry grown by URD_REGISTRY_COPIES copies of every row, which are checked as their
    // rows are and serve nothing. Every request resolves as before, at the cost a registry that many times larger has.
    for (int copy = 0; copy < URD_REGISTRY_COPIES; ++copy)
        for (const auto& row : table)
            if (row_serves(row, s, f) && row.id.empty())
                return &row;
#endif
    for (const int pass : { 0, 1 })
        for (const auto& row : table)
        {
            const auto traits = row.traits(f);
            const bool stable = traits && traits->has(tag::stable_refs);
            if ((compact && stable) != (pass == 1))
                continue;
            if (row_serves(row, s, f))
                return &row;
        }
    return nullptr;
}

}  // namespace urd::detail
