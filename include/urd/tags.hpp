#pragma once

// The vocabulary as code: every property a container declares or a request states, as a number (urd::tag) and as a
// type (urd::sorted, ...). A tag derived from another means "at least that"; its words are in urd/tag_synopsis.hpp, the
// machinery that computes with them in urd/property_set.hpp.

#include <cstdint>

namespace urd
{

/// Every property, by number, in the order of their categories, each category's root first: what the machinery below
/// computes with. Users name the tag types; a Store's spec lists these numbers, which compilers print by name.
enum class tag : std::uint8_t
{
    any_order,
    sorted,
    insertion_ordered,
    duplicates,
    unique,
    any_keys,
    dense,
    sparse,
    moving_refs,
    stable_refs,
    any_layout,
    contiguous,
    any_representation,
    canonical,
    single_threaded,
    concurrent,
    any_memory,
    retains,
    reclaims,
    any_clear,
    proportional_clear,
    constant_clear,
    any_access,
    fifo,
    lifo,
    by_priority,
    any_scope,
    scoped,
    any_layering,
    layered,
    any_handle,
    handles,
    any_sharing,
    structural_sharing,
    any_direction,
    bidirectional,
    any_shape,
    fixed_shape,
    any_packing,
    flat,
    any_grouping,
    partition,
    any_tracking,
    change_log,
    immutable,
    growable,
    shrinkable,
    may_change,
    monotone,
    frozen,
    maybe_empty,
    nonempty,
    any_multiplicity,
    functional,
    multi,
    by_key,
    positional,
    general,
    small,
    any_priority,
    monotone_priority,
    count  ///< not a tag: how many there are
};

/// What a Store is, by how its elements are placed and reached. A request is the kind its shape makes (key<K>: a set,
/// key<K> and value<V>: a map, value<V>: a sequence), or the highest of those its stated tags make (`makes` below), in
/// this order: a priority queue with handles is a queue, a pool of records stored flat is records.
enum class kind : std::uint8_t
{
    sequence,   ///< values by position
    multiset,   ///< values kept by value, in order, duplicates kept
    set,        ///< keys, each once
    map,        ///< a value per key, or several
    pool,       ///< rows by handle
    records,    ///< records stored flat, by handle
    queue,      ///< reached by front or top
    partition,  ///< keys in disjoint classes
};

/// Whether a kind keeps its elements by value (inserted where their value puts them), not where they are put.
constexpr bool keeps_by_value(kind of) { return of == kind::multiset || of == kind::set || of == kind::map; }
/// Whether a container of kind `container` holds a request of kind `request`: its own kind; and keys and values
/// kept by value are held alike (a sorted sequence without duplicates, value<V>, sorted, unique, is held as a set
/// is), whether duplicates are kept being a property of its own (unique).
constexpr bool holds_kind(kind container, kind request)
{
    const auto elements_by_value = [](kind of) { return of == kind::set || of == kind::multiset; };
    return container == request || (elements_by_value(container) && elements_by_value(request));
}

// Properties, in the style of Boost.Graph's graph_traits: each answers one question (a category) with a tag
// type, and a tag derived from another means "at least that". The root of a category promises nothing, and names
// itself as `root`, which every tag of its category inherits. Each tag also carries its number in urd::tag, so that
// properties are computed as constexpr values, not as types: a request costs the compiler a constant evaluation
// instead of hundreds of class instantiations.

// What a container guarantees by itself.
struct any_order
{
    using root = any_order;
    static constexpr tag value = tag::any_order;
};
/// Ascending by urd::Less: operator< for numbers, member by member for identified types.
struct sorted : any_order
{
    static constexpr tag value = tag::sorted;
    static constexpr kind makes = kind::multiset;
};
/// In the order they were inserted: a new element goes last, and the others keep their places.
struct insertion_ordered : any_order
{
    static constexpr tag value = tag::insertion_ordered;
};

struct duplicates
{
    using root = duplicates;
    static constexpr tag value = tag::duplicates;
    /// Exact: a container must hold what the request holds here, no more (a unique container would drop a sequence's duplicates; one that keeps them would
    /// break a set).
    static constexpr bool exact = true;
};
/// No two elements are equal.
struct unique : duplicates
{
    static constexpr tag value = tag::unique;
    static constexpr kind makes = kind::multiset;
};

struct any_keys
{
    using root = any_keys;
    static constexpr tag value = tag::any_keys;
};
/// The keys are 0..n-1, or nearly so.
struct dense : any_keys
{
    static constexpr tag value = tag::dense;
    /// Demanded: a container that keeps it stores by index, so it needs keys that are indices, which the request must
    /// promise (its storage grows with the largest key, and a negative key is no index at all).
    static constexpr bool demanded = true;
};
/// The keys are few compared to their range.
struct sparse : any_keys
{
    static constexpr tag value = tag::sparse;
};

struct moving_refs
{
    using root = moving_refs;
    static constexpr tag value = tag::moving_refs;
};
/// A reference to a stored element stays valid until that element is removed, by whichever path the container
/// hands it out: the element behind an iterator where iteration yields references; a map's value, by at,
/// operator[], the argument of update and the .second of an iterated entry (not the entry a dense map builds on
/// the fly); a sequence's operator[], front and back; a queue's front or top, which stays put while others are
/// pushed and when it is the next element again after pops; a pool's [handle], a record pool's [index]. Insert
/// never moves another element; erase and clear end only the references to what they remove.
struct stable_refs : moving_refs
{
    static constexpr tag value = tag::stable_refs;
};

struct any_layout
{
    using root = any_layout;
    static constexpr tag value = tag::any_layout;
};
/// The elements lie next to each other in memory.
struct contiguous : any_layout
{
    static constexpr tag value = tag::contiguous;
};

struct any_representation
{
    using root = any_representation;
    static constexpr tag value = tag::any_representation;
};
/// Equal contents have equal representations.
struct canonical : any_representation
{
    static constexpr tag value = tag::canonical;
};

struct single_threaded
{
    using root = single_threaded;
    static constexpr tag value = tag::single_threaded;
};
/// Threads share the container, per kind: sets and sequences take inserts from threads at once, and reads of
/// published elements (lookups, a position) beside them; maps take insert, try_emplace, insert_or_assign and
/// update from threads at once on any keys, and read beside them, each key's value under its own lock, so at()
/// (unlocked) is for keys no thread writes; pools take inserts and reads by handle of published rows at once, and
/// a layered pool reads a parent no thread changes; slots take insert, acquire, erase and [h] on handles the thread
/// holds. An element is published to a thread once it was inserted before the thread started, or by the thread
/// itself. A lookup beside inserts is contains, count, or find's result used as an element; comparing find's
/// iterator with end() races with the size, which another thread's insert moves. Everything else needs quiescence,
/// no other operation running: erase of another thread's element, clear, iteration, reserve, memory_usage, copy,
/// move and destruction.
struct concurrent : single_threaded
{
    static constexpr tag value = tag::concurrent;
};

struct any_memory
{
    using root = any_memory;
    static constexpr tag value = tag::any_memory;
};
/// Storage that erase and clear free is kept for reuse: refilling up to an earlier size allocates nothing.
struct retains : any_memory
{
    static constexpr tag value = tag::retains;
};
/// Erase and clear return the storage of what they remove.
struct reclaims : any_memory
{
    static constexpr tag value = tag::reclaims;
};

struct any_clear
{
    using root = any_clear;
    static constexpr tag value = tag::any_clear;
};
/// Clear takes time in proportion to the elements present, not to the keys or capacity seen before.
struct proportional_clear : any_clear
{
    static constexpr tag value = tag::proportional_clear;
};
/// Clear takes constant time, as with a generation counter.
struct constant_clear : proportional_clear
{
    static constexpr tag value = tag::constant_clear;
};

struct any_access
{
    using root = any_access;
    static constexpr tag value = tag::any_access;
};
/// A queue: elements leave in the order they came (front, push, pop).
struct fifo : any_access
{
    static constexpr tag value = tag::fifo;
    static constexpr kind makes = kind::queue;
};
/// A stack: the last element pushed leaves first (top, push, pop).
struct lifo : any_access
{
    static constexpr tag value = tag::lifo;
    static constexpr kind makes = kind::queue;
};
/// A priority queue: the least element by urd::Less leaves first (top, push, pop).
struct by_priority : any_access
{
    static constexpr tag value = tag::by_priority;
    static constexpr kind makes = kind::queue;
};

struct any_scope
{
    using root = any_scope;
    static constexpr tag value = tag::any_scope;
};
/// Offers push_scope and pop_scope: popping undoes every change since the matching push.
struct scoped : any_scope
{
    static constexpr tag value = tag::scoped;
};

struct any_layering
{
    using root = any_layering;
    static constexpr tag value = tag::any_layering;
};
/// Offers layer_over(parent): lookups find the parent's elements too, which it neither owns nor changes.
struct layered : any_layering
{
    static constexpr tag value = tag::layered;
};

struct any_handle
{
    using root = any_handle;
    static constexpr tag value = tag::any_handle;
    /// Exact: a container must hold what the request holds here, no more (handles make a pool, or give a priority queue update and erase by handle).
    static constexpr bool exact = true;
};
/// A pool: each element is identified by a handle that insert gives, with whether it was new, and equal elements get
/// equal handles. Not iterated; counted, with handles 0..size()-1, unless it has structural_sharing (handles name
/// shared nodes there).
struct handles : any_handle
{
    static constexpr tag value = tag::handles;
    static constexpr kind makes = kind::pool;
};

struct any_sharing
{
    using root = any_sharing;
    static constexpr tag value = tag::any_sharing;
};
/// Parts that are equal across elements (rows) are stored once.
struct structural_sharing : any_sharing
{
    static constexpr tag value = tag::structural_sharing;
};

struct any_direction
{
    using root = any_direction;
    static constexpr tag value = tag::any_direction;
};
/// A map that answers by value too: keys(v) gives the keys that map to v, as a graph gives a vertex's predecessors.
struct bidirectional : any_direction
{
    static constexpr tag value = tag::bidirectional;
};

struct any_shape
{
    using root = any_shape;
    static constexpr tag value = tag::any_shape;
};
/// Rows of a fixed shape: how many there are and how long each is is set at construction (urd::row_lengths); only
/// their contents change.
struct fixed_shape : any_shape
{
    static constexpr tag value = tag::fixed_shape;
};

struct any_packing
{
    using root = any_packing;
    static constexpr tag value = tag::any_packing;
};
/// Each element is one self-contained block of bytes, with no part of its own on the heap: trivially copyable, or
/// serialized as a whole. Such elements are frozen once stored, and are read by index.
struct flat : any_packing
{
    static constexpr tag value = tag::flat;
    static constexpr kind makes = kind::records;
};

struct any_grouping
{
    using root = any_grouping;
    static constexpr tag value = tag::any_grouping;
};
/// A partition: its elements fall into disjoint classes; merge joins two classes, find names an element's class.
struct partition : any_grouping
{
    static constexpr tag value = tag::partition;
    static constexpr kind makes = kind::partition;
};

struct any_tracking
{
    using root = any_tracking;
    static constexpr tag value = tag::any_tracking;
};
/// Offers added() and removed(): exactly the elements that changed since clear_changes(), for semi-naive evaluation.
struct change_log : any_tracking
{
    static constexpr tag value = tag::change_log;
};

/// Offers no change.
struct immutable
{
    using root = immutable;
    static constexpr tag value = tag::immutable;
};
/// Offers insert.
struct growable : immutable
{
    static constexpr tag value = tag::growable;
};
/// Offers insert, erase and clear.
struct shrinkable : growable
{
    static constexpr tag value = tag::shrinkable;
};

// What a Store's interface enforces: no container declares a lifecycle, emptiness or identity. Multiplicity a
// multimap declares (multi), but a request only states it.
struct may_change
{
    using root = may_change;
    static constexpr tag value = tag::may_change;
};
/// Only grows: nothing is ever removed.
struct monotone : may_change
{
    static constexpr tag value = tag::monotone;
};
/// No change after construction (assign rebuilds it whole): any number of threads may read it between rebuilds,
/// so concurrent, which is about inserts from threads at once, is not asked of it.
struct frozen : monotone
{
    static constexpr tag value = tag::frozen;
};

struct maybe_empty
{
    using root = maybe_empty;
    static constexpr tag value = tag::maybe_empty;
};
/// At least one element.
struct nonempty : maybe_empty
{
    static constexpr tag value = tag::nonempty;
};

struct any_multiplicity
{
    using root = any_multiplicity;
    static constexpr tag value = tag::any_multiplicity;
};
/// At most one value per key (a map).
struct functional : any_multiplicity
{
    static constexpr tag value = tag::functional;
};
/// Any number of values per key (a multimap).
struct multi : any_multiplicity
{
    static constexpr tag value = tag::multi;
    static constexpr kind makes = kind::multiset;
};

struct by_key
{
    using root = by_key;
    static constexpr tag value = tag::by_key;
};
/// Identified by position 0..n-1, in order (a sequence).
struct positional : by_key
{
    static constexpr tag value = tag::positional;
};

// How a Store is used: it changes which representation is chosen, never what the Store guarantees. Only a
// request says it; docs/BENCHMARKS.md backs what each one chooses.
struct general
{
    using root = general;
    static constexpr tag value = tag::general;
};
/// A few elements, or a few at a time.
struct small : general
{
    static constexpr tag value = tag::small;
};

struct any_priority
{
    using root = any_priority;
    static constexpr tag value = tag::any_priority;
};
/// A priority queue whose pushes are never below the last popped (shortest paths with non-negative lengths, event
/// simulation): with integral priorities, a bucket queue serves it.
struct monotone_priority : any_priority
{
    static constexpr tag value = tag::monotone_priority;
    static constexpr kind makes = kind::queue;
};

}  // namespace urd
