#pragma once

// The combinations no Store can keep, one rule each: a rule reads what a request is (request_facts) and fails with
// its own message where it is broken. The machinery (urd::detail::to_spec, urd/properties.hpp) computes the facts
// once and asks every rule in `incompatible_rules`; it knows none of them. docs/language/index.rst lists them for
// readers, and tests/compile_fail_store.cpp pins each message.

#include "urd/property_set.hpp"
#include "urd/tags.hpp"

namespace urd::detail
{

/// What a request is, as its checks read it: the tags it states (with those they state, urd/rules.hpp), its kind,
/// and what its key and value types and its order_by are.
struct request_facts
{
    property_set stated = property_set::roots();
    kind of = kind::sequence;
    bool keyed = false;
    bool valued = false;
    bool value_is_store = false;    ///< value<Store<…>>: rows
    bool value_iterates = false;    ///< the value is a range of elements (a pool's row)
    bool ordered_by = false;        ///< it gives order_by<P>
    bool entry_order = false;       ///< P orders a map's entries, key and value
    bool ordered_orderable = true;  ///< what P orders (its keys, else its values) is orderable by urd::Less
};

constexpr bool sequence_like(kind of) { return of == kind::sequence || of == kind::multiset; }
constexpr bool is_pool_kind(kind of) { return of == kind::pool || of == kind::records; }

template<request_facts R>
struct partition_groups_keys
{
    static_assert(!R.stated.has(tag::partition) || (!R.valued && !R.stated.has(tag::frozen)),
                  "urd::Store: a partition groups key<K>s, without values, and changes by insert and merge");
};
template<request_facts R>
struct pool_by_handle
{
    static_assert(!R.stated.has(tag::handles)
                      || (!R.keyed && (R.of != kind::queue || R.stated.has(tag::by_priority)) && !R.stated.has(tag::positional) && !R.stated.has(tag::frozen)),
                  "urd::Store: a pool (handles) holds value<V> rows, gains them by insert, and is reached by handle, not by key, "
                  "position or order; a priority queue with handles changes or erases an element by its handle");
};
template<request_facts R>
struct flat_interns
{
    static_assert(!R.stated.has(tag::flat) || (is_pool_kind(R.of) && R.stated.has(tag::unique)),
                  "urd::Store: flat stores each record of an interning pool as one block of bytes; ask for handles and unique too");
};
template<request_facts R>
struct records_are_flat
{
    static_assert(!(is_pool_kind(R.of) && R.stated.has(tag::unique)) || R.stated.has(tag::flat) || R.value_iterates,
                  "urd::Store: a pool of records (value<V>, V no range of elements) stores them flat: ask for flat");
};
template<request_facts R>
struct monotone_priority_is_a_priority_queue
{
    static_assert(!R.stated.has(tag::monotone_priority) || R.stated.has(tag::by_priority),
                  "urd::Store: monotone_priority says how a priority queue is pushed to, so it makes a priority queue; it cannot go "
                  "with fifo or lifo");
};
template<request_facts R>
struct queue_by_front
{
    static_assert(R.of != kind::queue || (!R.stated.has(tag::frozen) && !R.stated.has(tag::positional)),
                  "urd::Store: a queue changes by push and pop, and is reached by its front or top, not by position");
};
template<request_facts R>
struct nonempty_iterated
{
    static_assert(!R.stated.has(tag::nonempty) || R.of == kind::set || R.of == kind::map || sequence_like(R.of),
                  "urd::Store: nonempty is a promise about the elements a set, map or sequence iterates; a queue empties by pop and a "
                  "pool by erase(handle), so it cannot hold there: drop nonempty, or check empty() before pop");
};
template<request_facts R>
struct fixed_shape_rows
{
    static_assert(!R.stated.has(tag::fixed_shape) || (sequence_like(R.of) && !R.stated.has(tag::monotone) && R.value_is_store),
                  "urd::Store: fixed_shape is a sequence of rows (value<Store<…>>) whose number and lengths never change; its rows' "
                  "contents do");
};
template<request_facts R>
struct map_not_positional
{
    static_assert(R.of != kind::map || !R.stated.has(tag::positional),
                  "urd::Store: positional identifies a map's elements by position, so it cannot have key<K>");
};
template<request_facts R>
struct contiguous_moves
{
    static_assert(!(R.stated.has(tag::contiguous) && R.stated.has(tag::stable_refs)) || R.stated.has(tag::frozen),
                  "urd::Store: contiguous storage moves its elements when it grows, so stable_refs needs frozen too");
};
template<request_facts R>
struct positions_move
{
    static_assert(!((sequence_like(R.of) || R.stated.has(tag::positional)) && R.stated.has(tag::stable_refs)) || R.stated.has(tag::monotone),
                  "urd::Store: erasing or inserting at a position moves the elements after it: stable_refs on a sequence or a "
                  "positional set needs monotone, append only, or frozen");
};
template<request_facts R>
struct no_concurrent_sorted
{
    static_assert(!(R.stated.has(tag::sorted) && R.stated.has(tag::concurrent)) || R.stated.has(tag::dense),
                  "urd::Store: no concurrent sorted container: threads share a hashed (concurrent) or dense Store; keep sorted and "
                  "synchronize outside, or drop sorted");
};
template<request_facts R>
struct no_concurrent_queue
{
    static_assert(R.of != kind::queue || !R.stated.has(tag::concurrent),
                  "urd::Store: a queue is reached by its front or top, which pop moves: no queue is concurrent; guard it with a mutex, "
                  "or share a positional, monotone set");
};
template<request_facts R>
struct frozen_is_shared
{
    static_assert(!(R.stated.has(tag::frozen) && R.stated.has(tag::concurrent)),
                  "urd::Store: a frozen Store never changes, so any number of threads may read it; concurrent asks for inserts "
                  "from threads at once, which it has none of: drop concurrent");
};
template<request_facts R>
struct order_by_orders
{
    static_assert(!R.ordered_by || R.stated.has(tag::sorted) || R.stated.has(tag::by_priority),
                  "urd::Store: order_by<P> orders a sorted Store or a by_priority queue; ask for one of them too");
};
template<request_facts R>
struct order_by_breaks_ties
{
    static_assert(!R.ordered_by || !R.keyed || R.stated.has(tag::multi) || R.ordered_orderable,
                  "urd::Store: order_by<P> on unique keys breaks ties by the keys' own order, so they must be orderable "
                  "(else two keys with equal P(key) would be one)");
};
template<request_facts R>
struct dense_is_its_order
{
    static_assert(!R.ordered_by || !R.stated.has(tag::dense),
                  "urd::Store: dense keys are stored and iterated by index, so no order_by<P> applies: drop dense, or order the index "
                  "type itself");
};
template<request_facts R>
struct entry_order_is_a_scan
{
    static_assert(!R.ordered_by || !R.entry_order || R.stated.has(tag::small) || R.stated.has(tag::frozen),
                  "urd::Store: a map ordered by its values is a flat scan (O(n) insert, lookup by key): ask for small or frozen too, or "
                  "order by key");
};
template<request_facts R>
struct dense_keeps_slots
{
    static_assert(!(R.stated.has(tag::dense) && R.stated.has(tag::reclaims)),
                  "urd::Store: dense keys are stored by position (a slot per index), so erase returns no storage: reclaims needs sparse "
                  "keys (a tree or node table)");
};
template<request_facts R>
struct constant_clear_is_by_index
{
    static_assert(!R.keyed || !R.stated.has(tag::constant_clear) || R.stated.has(tag::dense),
                  "urd::Store: constant_clear is a generation counter over a slot per index: ask for dense too (a hash table or tree "
                  "clears in time proportional to its slots: proportional_clear)");
};
template<request_facts R>
struct layers_have_no_order
{
    static_assert(!(R.stated.has(tag::layered) && R.stated.has(tag::sorted)),
                  "urd::Store: a layer iterates its own elements, not its parent's, so no order holds over both: drop sorted, or iterate "
                  "the parent separately");
};
template<request_facts R>
struct partition_single_threaded
{
    static_assert(!(R.stated.has(tag::partition) && R.stated.has(tag::concurrent)),
                  "urd::Store: a partition's merge rewrites parents along two paths: single-threaded");
};
template<request_facts R>
struct values_by_value_are_sorted
{
    static_assert(R.of != kind::multiset || R.stated.has(tag::sorted),
                  "urd::Store: values kept by value (unique or multi, without key<K>) are kept in order: ask for sorted too, or use "
                  "key<K> for a set");
};

/// Every rule, each asked of a request by inheriting it.
template<request_facts R>
struct incompatible_rules :
    partition_groups_keys<R>,
    pool_by_handle<R>,
    flat_interns<R>,
    records_are_flat<R>,
    monotone_priority_is_a_priority_queue<R>,
    queue_by_front<R>,
    nonempty_iterated<R>,
    fixed_shape_rows<R>,
    map_not_positional<R>,
    contiguous_moves<R>,
    positions_move<R>,
    no_concurrent_sorted<R>,
    no_concurrent_queue<R>,
    frozen_is_shared<R>,
    order_by_orders<R>,
    order_by_breaks_ties<R>,
    dense_is_its_order<R>,
    entry_order_is_a_scan<R>,
    dense_keeps_slots<R>,
    constant_clear_is_by_index<R>,
    layers_have_no_order<R>,
    partition_single_threaded<R>,
    values_by_value_are_sorted<R>
{
};

}  // namespace urd::detail
