#pragma once

// What each container guarantees, as a function of the facts about the elements it is given (urd/registry/facts.hpp):
// the properties its declaration offers (urd/registry/declaration.hpp), or none where it does not take these elements.
// A wrapper's are its part's, changed.

#include "urd/registry/facts.hpp"

#include <optional>

namespace urd::detail::guarantees
{

using P = std::optional<property_set>;
using T = type_features;
using F = request_features;

constexpr tag threads(bool concurrent_) { return concurrent_ ? tag::concurrent : tag::single_threaded; }
constexpr tag references(bool stable) { return stable ? tag::stable_refs : tag::moving_refs; }

// Sets and maps over urd::Less (`ordered` false) or over the request's comparator (true).
/// Whether the request's order applies to `t`: urd::Less where it gives no order_by, else its comparator.
constexpr bool orders(const T& t, const F& f) { return f.by == order::none ? t.orderable : f.order_natural; }
constexpr P tree_set(const T& t, const F& f)
{
    if (!orders(t, f))
        return {};
    return props({ tag::sorted, tag::unique, tag::stable_refs, tag::sparse, tag::shrinkable, tag::reclaims, tag::proportional_clear });
}
constexpr P hash_set(const T& t)
{
    if (!t.hashable)
        return {};
    return props({ tag::unique, tag::stable_refs, tag::sparse, tag::shrinkable });
}
constexpr P flat_set(const T& t, const F& f)
{
    if (!orders(t, f))
        return {};
    return props(
        { tag::sorted, tag::unique, tag::positional, tag::sparse, tag::contiguous, tag::canonical, tag::shrinkable, tag::retains, tag::proportional_clear });
}
constexpr P flat_multiset(const T& t, const F& f)
{
    if (!orders(t, f))
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
constexpr P tree_map(const T& key, const F& f)
{
    if (!orders(key, f))
        return {};
    return props({ tag::sorted, tag::unique, tag::functional, tag::stable_refs, tag::sparse, tag::shrinkable, tag::reclaims, tag::proportional_clear });
}
constexpr P tree_multimap(const T& key, const F& f)
{
    if (!orders(key, f))
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
constexpr P flat_map(const T& key, const F& f)
{
    if (!orders(key, f))
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
constexpr P flat_multimap(const T& key, const F& f)
{
    if (!orders(key, f))
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
constexpr P heap(const T& t, const F& f)
{
    if (!orders(t, f))
        return {};
    return props({ tag::by_priority, tag::shrinkable, tag::retains });
}
constexpr P addressable_heap(const T& t, const F& f)
{
    if (!orders(t, f))
        return {};
    return props({ tag::by_priority, tag::handles, tag::shrinkable, tag::retains });
}
constexpr P bucket_queue(const T& t, const F& f)
{
    if (!orders(t, f) || !f.order_key.integral)
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
    if (!key.indexable || !mapped.default_initializable)
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
    if (!t.serializable || !t.hashable)
        return {};
    return props({ tag::handles, tag::unique, tag::flat, tag::stable_refs, tag::shrinkable, tag::retains, threads(concurrent_) });
}
constexpr P row_interner(const T& row, bool concurrent_)
{
    if (row.element == nullptr || !row.element->hashable)
        return {};
    return props({ tag::handles, tag::unique, tag::shrinkable, tag::retains, threads(concurrent_) });
}
constexpr P tree_vector_set(const T& row, bool concurrent_)
{
    if (row.element == nullptr || !row.element->hashable || keeps_by_value(row.store->kind))
        return {};
    return props({ tag::handles, tag::unique, tag::structural_sharing, tag::shrinkable, tag::retains, threads(concurrent_) });
}
constexpr P tree_bitset_set(const T& row, bool concurrent_)
{
    if (row.element == nullptr || !row.element->indexable || !row.store->properties.has(tag::unique) || !row.store->properties.has(tag::dense))
        return {};
    return props({ tag::handles, tag::unique, tag::structural_sharing, tag::shrinkable, tag::retains, threads(concurrent_) });
}
constexpr P slot_vector(bool concurrent_) { return props({ tag::handles, tag::stable_refs, tag::shrinkable, tag::retains, threads(concurrent_) }); }
/// What an interning pool of rows gives its rows: their own properties.
constexpr P own_rows(const T& row) { return row.store ? P(row.store->properties) : P(); }

// Rows.
/// Whether a jagged array holds rows `inner` flattened: frozen rows, of elements or (deeper) of rows in order.
constexpr bool flattenable(const T& inner)
{ return inner.store && inner.store->properties.has(tag::frozen) && (inner.element == nullptr || !inner.element->store || !keeps_by_value(inner.store->kind)); }
/// Whether a jagged array holds rows `inner` that change in place: rows of elements, as a sequence, or a dense set's
/// bits; or fixed-shape rows of such rows, at any depth.
constexpr bool fixed_rows(const T& inner)
{
    if (!inner.store || inner.store->properties.has(tag::frozen))
        return false;
    if (inner.element == nullptr || !inner.element->store)
        return !keeps_by_value(inner.store->kind)
               || (inner.store->kind == urd::kind::set && inner.store->properties.has(tag::unique) && inner.store->properties.has(tag::dense));
    return inner.store->properties.has(tag::fixed_shape) && fixed_rows(*inner.element);
}
/// Whether a row's elements are rows themselves.
constexpr bool of_stores(const T& inner) { return inner.element != nullptr && inner.element->store; }
constexpr P jagged(const T& inner, bool track)
{
    if (!track && flattenable(inner))
        return props({ tag::dense, tag::positional, tag::shrinkable, tag::retains, tag::proportional_clear });
    if (fixed_rows(inner))
        return props({ tag::dense, tag::positional, tag::fixed_shape, tag::retains, track ? tag::proportional_clear : tag::any_clear });
    return {};
}
constexpr P jagged_rows(const T& inner, bool track)
{
    if (!track && flattenable(inner))
        return with(inner.store->properties, { of_stores(inner) ? tag::any_layout : tag::contiguous, tag::moving_refs, tag::retains });
    if (fixed_rows(inner))
        return with(inner.store->properties,
                    { (!of_stores(inner) && !keeps_by_value(inner.store->kind)) ? tag::contiguous : tag::any_layout, tag::moving_refs, tag::retains });
    return {};
}
constexpr P row_pool(const T& inner)
{
    if (!inner.store || !inner.store->properties.has(tag::shrinkable))
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

}  // namespace urd::detail::guarantees
