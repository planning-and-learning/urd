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
constexpr P flat_entry_map(const T& key, bool entry_less)
{
    if (!key.orderable || !entry_less)
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
    if (!row.row_element_hashable || keeps_by_value(row.store_properties))
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
        return with(
            inner.store_properties,
            { (!inner.store_of_stores && !keeps_by_value(inner.store_properties)) ? tag::contiguous : tag::any_layout, tag::moving_refs, tag::retains });
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
    if (!part || !part->has(tag::unique) || is_map_properties(*part))
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

/// The rows of element_preference (urd/registry/preferences.hpp), in its order.
inline constexpr auto element_rows = std::to_array<registry_row>({
    { "jagged",
      [](const request_features& f) { return rows::jagged(f.element, false); },
      [](const request_features& f) { return rows::jagged_rows(f.element, false); } },
    { "tracked_jagged",
      [](const request_features& f) { return rows::jagged(f.element, true); },
      [](const request_features& f) { return rows::jagged_rows(f.element, true); } },
    { "vector", [](const request_features&) { return rows::vector(); } },
    { "unordered_flat_set", [](const request_features& f) { return rows::flat_hash_set(f.element); } },
    { "unordered_set", [](const request_features& f) { return rows::hash_set(f.element); } },
    { "set", [](const request_features& f) { return rows::tree_set(f.element, false); } },
    { "flat_multiset", [](const request_features& f) { return rows::flat_multiset(f.element, false); } },
    { "indexed_set", [](const request_features& f) { return rows::indexed_set(f.element); } },
    { "flat_set", [](const request_features& f) { return rows::flat_set(f.element, false); } },
    { "recycling_hash_set", [](const request_features& f) { return rows::recycling_set(rows::hash_set(f.element)); } },
    { "recycling_tree_set", [](const request_features& f) { return rows::recycling_set(rows::tree_set(f.element, false)); } },
    { "dense_set", [](const request_features& f) { return rows::dense_set(f.element, false); } },
    { "tracked_dense_set", [](const request_features& f) { return rows::dense_set(f.element, true); } },
    { "sparse_set", [](const request_features& f) { return rows::sparse_set(f.element, false); } },
    { "stable_indexed_set", [](const request_features& f) { return rows::stable_indexed_set(f.element, false); } },
    { "ordered_sparse_set", [](const request_features& f) { return rows::sparse_set(f.element, true); } },
    { "segmented_vector", [](const request_features&) { return rows::segmented_vector(false); } },
    { "concurrent_segmented_vector", [](const request_features&) { return rows::segmented_vector(true); } },
    { "row_pool", [](const request_features& f) { return rows::row_pool(f.element); }, [](const request_features& f) { return rows::own_rows(f.element); } },
    { "fifo", [](const request_features&) { return rows::fifo(); } },
    { "ring_fifo", [](const request_features&) { return rows::ring_fifo(); } },
    { "lifo", [](const request_features&) { return rows::lifo(); } },
    { "bucket_queue", [](const request_features& f) { return rows::bucket_queue(f.element, false); }, nullptr, property_set::of({ tag::monotone_priority }) },
    { "heap", [](const request_features& f) { return rows::heap(f.element, false); } },
    { "addressable_heap", [](const request_features& f) { return rows::addressable_heap(f.element, false); } },
    { "layered", [](const request_features& f) { return rows::layered(rows::flat_hash_set(f.element)); } },
    { "layered_node_set", [](const request_features& f) { return rows::layered(rows::hash_set(f.element)); } },
    { "change_logged", [](const request_features& f) { return rows::change_logged(rows::flat_hash_set(f.element)); } },
    { "change_logged_dense_set", [](const request_features& f) { return rows::change_logged(rows::dense_set(f.element, true)); } },
    { "change_logged_node_set", [](const request_features& f) { return rows::change_logged(rows::hash_set(f.element)); } },
    { "change_logged_tree_set", [](const request_features& f) { return rows::change_logged(rows::tree_set(f.element, false)); } },
    { "row_interner",
      [](const request_features& f) { return rows::row_interner(f.element, false); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "tree_vector_set",
      [](const request_features& f) { return rows::tree_vector_set(f.element, false); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "tree_bitset_set",
      [](const request_features& f) { return rows::tree_bitset_set(f.element, false); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "thread_safe_row_interner",
      [](const request_features& f) { return rows::row_interner(f.element, true); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "tree_vector_set",
      [](const request_features& f) { return rows::tree_vector_set(f.element, true); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "tree_bitset_set",
      [](const request_features& f) { return rows::tree_bitset_set(f.element, true); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "record_interner", [](const request_features& f) { return rows::record_interner(f.element, false, false); } },
    { "arena_interner", [](const request_features& f) { return rows::arena_interner(f.element, false); } },
    { "stable_record_interner", [](const request_features& f) { return rows::record_interner(f.element, false, true); } },
    { "thread_safe_record_interner", [](const request_features& f) { return rows::record_interner(f.element, true, false); } },
    { "thread_safe_arena_interner", [](const request_features& f) { return rows::arena_interner(f.element, true); } },
    { "layered_pool",
      [](const request_features& f) { return rows::layered_pool(rows::row_interner(f.element, false)); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "layered_pool", [](const request_features& f) { return rows::layered_pool(rows::record_interner(f.element, false, false)); } },
    { "layered_pool", [](const request_features& f) { return rows::layered_pool(rows::arena_interner(f.element, false)); } },
    { "layered_pool", [](const request_features& f) { return rows::layered_pool(rows::record_interner(f.element, false, true)); } },
    { "layered_pool",
      [](const request_features& f) { return rows::layered_pool(rows::row_interner(f.element, true)); },
      [](const request_features& f) { return rows::own_rows(f.element); } },
    { "layered_pool", [](const request_features& f) { return rows::layered_pool(rows::record_interner(f.element, true, false)); } },
    { "layered_pool", [](const request_features& f) { return rows::layered_pool(rows::arena_interner(f.element, true)); } },
#if defined(URD_HAS_GTL)
    { "parallel_hash_set", [](const request_features& f) { return rows::parallel_hash_set(f.element, false); } },
    { "parallel_node_hash_set", [](const request_features& f) { return rows::parallel_hash_set(f.element, true); } },
#endif
    { "concurrent_indexed_set", [](const request_features& f) { return rows::stable_indexed_set(f.element, true); } },
    { "slot_vector", [](const request_features&) { return rows::slot_vector(false); } },
    { "thread_safe_slot_vector", [](const request_features&) { return rows::slot_vector(true); } },
    { "disjoint_sets", [](const request_features& f) { return rows::disjoint_sets(f.element); } },
});

/// The rows of map_preference, in its order.
inline constexpr auto map_rows = std::to_array<registry_row>({
    { "unordered_flat_map", [](const request_features& f) { return rows::flat_hash_map(f.key); } },
    { "unordered_map", [](const request_features& f) { return rows::hash_map(f.key); } },
    { "posting_map", [](const request_features& f) { return rows::posting_map(f.key); } },
    { "unordered_multimap", [](const request_features& f) { return rows::hash_multimap(f.key); } },
#if defined(URD_HAS_GTL)
    { "parallel_hash_map", [](const request_features& f) { return rows::parallel_hash_map(f.key, false); } },
    { "parallel_node_hash_map", [](const request_features& f) { return rows::parallel_hash_map(f.key, true); } },
#endif
    { "map", [](const request_features& f) { return rows::tree_map(f.key, false); } },
    { "multimap", [](const request_features& f) { return rows::tree_multimap(f.key, false); } },
    { "flat_multimap", [](const request_features& f) { return rows::flat_multimap(f.key, false); } },
    { "flat_map", [](const request_features& f) { return rows::flat_map(f.key, false); } },
    { "recycling_hash_map",
      [](const request_features& f) { return rows::recycling(rows::hash_map(f.key)); },
      [](const request_features& f) { return rows::own_rows(f.mapped); } },
    { "recycling_map",
      [](const request_features& f) { return rows::recycling(rows::tree_map(f.key, false)); },
      [](const request_features& f) { return rows::own_rows(f.mapped); } },
    { "dense_map", [](const request_features& f) { return rows::index_map(f.key, false); } },
    { "stable_dense_map", [](const request_features& f) { return rows::index_map(f.key, true); } },
    { "generation_map", [](const request_features& f) { return rows::generation_map(f.key, f.mapped, false, false); } },
    { "stable_generation_map", [](const request_features& f) { return rows::generation_map(f.key, f.mapped, false, true); } },
    { "thread_safe_generation_map", [](const request_features& f) { return rows::generation_map(f.key, f.mapped, true, true); } },
    { "ordered_map", [](const request_features& f) { return rows::ordered_map(rows::indexed_set(f.key), f.mapped); } },
    { "ordered_map", [](const request_features& f) { return rows::ordered_map(rows::sparse_set(f.key, true), f.mapped); } },
    { "csr_multimap", [](const request_features& f) { return rows::csr_multimap(f.key); } },
    { "bidirectional_csr", [](const request_features& f) { return rows::bidirectional_csr(f.key, f.mapped); } },
    { "dense_multimap", [](const request_features& f) { return rows::dense_multimap(f.key); } },
    { "scoped_map", [](const request_features& f) { return rows::scoped(rows::flat_hash_map(f.key)); } },
    { "scoped_tree_map", [](const request_features& f) { return rows::scoped(rows::tree_map(f.key, false)); } },
    { "scoped_dense_map", [](const request_features& f) { return rows::scoped(rows::index_map(f.key, false)); } },
    { "layered", [](const request_features& f) { return rows::layered(rows::flat_hash_map(f.key)); } },
    { "layered_node_map", [](const request_features& f) { return rows::layered(rows::hash_map(f.key)); } },
    { "layered", [](const request_features& f) { return rows::layered(rows::index_map(f.key, false)); } },
    { "layered_generation_map", [](const request_features& f) { return rows::layered(rows::generation_map(f.key, f.mapped, false, false)); } },
});

/// The rows of ordered_element_preference (over the request's comparator), in its order.
inline constexpr auto ordered_element_rows = std::to_array<registry_row>({
    { "set", [](const request_features& f) { return rows::tree_set(f.element, true); } },
    { "flat_multiset", [](const request_features& f) { return rows::flat_multiset(f.element, true); } },
    { "flat_set", [](const request_features& f) { return rows::flat_set(f.element, true); } },
    { "bucket_queue", [](const request_features& f) { return rows::bucket_queue(f.element, true); }, nullptr, property_set::of({ tag::monotone_priority }) },
    { "heap_by", [](const request_features& f) { return rows::heap(f.element, true); } },
    { "addressable_heap", [](const request_features& f) { return rows::addressable_heap(f.element, true); } },
});

/// The rows of ordered_map_preference, in its order.
inline constexpr auto ordered_map_rows = std::to_array<registry_row>({
    { "map", [](const request_features& f) { return rows::tree_map(f.key, true); } },
    { "multimap", [](const request_features& f) { return rows::tree_multimap(f.key, true); } },
    { "flat_multimap", [](const request_features& f) { return rows::flat_multimap(f.key, true); } },
    { "flat_map", [](const request_features& f) { return rows::flat_map(f.key, true); } },
    { "recycling_map",
      [](const request_features& f) { return rows::recycling(rows::tree_map(f.key, true)); },
      [](const request_features& f) { return rows::own_rows(f.mapped); } },
});

/// The rows of value_ordered_map_preference.
inline constexpr auto value_ordered_map_rows = std::to_array<registry_row>({
    { "flat_entry_map", [](const request_features& f) { return rows::flat_entry_map(f.key, f.entry_less); } },
});

}  // namespace urd::detail

namespace urd::detail
{

/// What a request is, as selection reads it: its properties, what a container must guarantee for them
/// (requirement<S>), and its shape.
struct request_shape
{
    property_set stated;
    property_set required;
    bool is_sequence = false;
    bool is_queue = false;
    bool is_map = false;
};

/// Whether `row` serves a request of shape `s` with features `f`: serves<R, S> (urd/registry/select.hpp), as values.
constexpr bool row_serves(const registry_row& row, const request_shape& s, const request_features& f)
{
    const auto traits = row.traits(f);
    if (!traits)
        return false;
    const auto& have = *traits;
    if (!satisfies(have, s.required) || !demands_met(have, s.stated) || keeps_by_value(have) != keeps_by_value(s.stated)
        || have.has(tag::unique) != s.stated.has(tag::unique) || have.has(tag::handles) != s.stated.has(tag::handles)
        || have.has(tag::flat) != s.stated.has(tag::flat) || (s.is_sequence && keeps_by_value(s.stated) && !s.stated.has(tag::sorted))
        || is_queue_properties(have) != s.is_queue || !promises(s.stated, row.relies_on) || (s.stated.has(tag::positional) && !have.has(tag::positional)))
        return false;
    const auto& held = s.is_map ? f.mapped : f.element;
    if (!held.store)
        return true;
    const auto given = row.rows ? row.rows(f) : std::optional<property_set>(with(held.store_properties, { tag::reclaims }));
    return given && satisfies(*given, held.store_requirement);
}

/// The id of the first row of `table` serving the request, or "" where none does. Where elements are few or never
/// change, rows with stable storage come last (reordered<> in urd/registry/select.hpp).
template<std::size_t N>
constexpr std::string_view first_serving_row(const std::array<registry_row, N>& table, const request_shape& s, const request_features& f)
{
    const bool compact = s.stated.has(tag::frozen) || s.stated.has(tag::small);
    for (const int pass : { 0, 1 })
        for (const auto& row : table)
        {
            const auto traits = row.traits(f);
            const bool stable = traits && traits->has(tag::stable_refs);
            if ((compact && stable) != (pass == 1))
                continue;
            if (row_serves(row, s, f))
                return row.id;
        }
    return {};
}

}  // namespace urd::detail
