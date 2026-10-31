#pragma once

// The vocabulary as code: every property a container declares or a request states, as a number (urd::tag) and as a
// type (urd::sorted, ...), with its words beside it. A tag derived from another means "at least that". A category's
// root names it: its question, its group (kind, semantics, costs, hints), and in semantics its reading heading; any
// other tag has its description, the phrases a need is put in, and in semantics who keeps it (`promise`: guaranteed or
// caller). The machinery that computes with them is in urd/property_set.hpp; share/urd/vocabulary.json is generated
// from them.

#include <array>
#include <cstdint>
#include <string_view>
#include <tuple>

namespace urd
{

// The groups, by what leaving out a tag one needs causes. Every category is in exactly one.
struct kind_group
{
    static constexpr std::string_view id = "kind";
    static constexpr std::string_view title = "Kind";
    static constexpr std::string_view test = "Leaving it out does not compile: the operation does not exist.";
    static constexpr std::string_view summary =
        "Whether the Store is a set, map or sequence, a queue, a pool or a partition: the first decision, which every other changes with.";
    static constexpr std::string_view literature = "Abstract data types: a type is the operations it offers (Liskov and Zilles, 1974).";
};
struct semantics_group
{
    static constexpr std::string_view id = "semantics";
    static constexpr std::string_view title = "Semantics";
    static constexpr std::string_view test = "Leaving it out is a bug: the program relies on something nobody keeps.";
    static constexpr std::string_view summary =
        "What holds of the elements: guaranteed by the container (sorted, stable_refs), or promised by the caller about its data (dense).";
    static constexpr std::string_view literature = "Design by contract: guarantees the supplier keeps, preconditions the client keeps (Meyer, 1992).";
};
struct costs_group
{
    static constexpr std::string_view id = "costs";
    static constexpr std::string_view title = "Costs";
    static constexpr std::string_view test = "Leaving it out makes it slower by a complexity class: what is guaranteed is how much work or memory.";
    static constexpr std::string_view summary = "How long clear takes, what erase and clear do with memory, and whether equal parts are stored once.";
    static constexpr std::string_view literature =
        "Complexity requirements as part of an interface, as the C++ standard library states them (Stepanov and Lee, 1995).";
};
struct hints_group
{
    static constexpr std::string_view id = "hints";
    static constexpr std::string_view title = "Hints";
    static constexpr std::string_view test = "Leaving it out makes it slower by a constant factor; it never changes a guarantee.";
    static constexpr std::string_view summary = "How the Store is used, which reorders the containers tried and nothing else.";
    static constexpr std::string_view literature = "Workload hints, as a query hint or madvise gives one: advice that changes no result.";
};

/// The groups, in order.
using all_groups = std::tuple<kind_group, semantics_group, costs_group, hints_group>;

// The reading headings of semantics: they order its page and carry no rules.
struct promise_heading
{
    static constexpr std::string_view id = "promise";
    static constexpr std::string_view title = "What you promise about your data";
};
struct holds_heading
{
    static constexpr std::string_view id = "holds";
    static constexpr std::string_view title = "What holds";
};
struct more_heading
{
    static constexpr std::string_view id = "more";
    static constexpr std::string_view title = "What more it can do";
};
struct memory_heading
{
    static constexpr std::string_view id = "memory";
    static constexpr std::string_view title = "How it hands out memory";
};

/// The headings, in order.
using all_headings = std::tuple<promise_heading, holds_heading, more_heading, memory_heading>;

/// The heading of a category outside semantics: none.
struct no_heading
{
    static constexpr std::string_view id = "";
    static constexpr std::string_view title = "";
};


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
    static constexpr std::string_view name = "any_order";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "In which order is it iterated?";
};
/// Ascending by urd::Less: operator< for numbers, member by member for identified types.
struct sorted : any_order
{
    static constexpr tag value = tag::sorted;
    static constexpr std::string_view name = "sorted";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Iterated in ascending order by urd::Less.";
    static constexpr std::array<std::string_view, 4> phrases { "sorted", "in order", "ordered by key", "range queries" };
    static constexpr kind makes = kind::multiset;
};
/// In the order they were inserted: a new element goes last, and the others keep their places.
struct insertion_ordered : any_order
{
    static constexpr tag value = tag::insertion_ordered;
    static constexpr std::string_view name = "insertion_ordered";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Iterated in the order elements were first inserted.";
    static constexpr std::array<std::string_view, 3> phrases { "insertion order", "first come first listed", "keeps the order added" };
};

struct duplicates
{
    using root = duplicates;
    static constexpr tag value = tag::duplicates;
    static constexpr std::string_view name = "duplicates";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Duplicates allowed?";
    /// Exact: a container must hold what the request holds here, no more (a unique container would drop a sequence's duplicates; one that keeps them would
    /// break a set).
    static constexpr bool exact = true;
};
/// No two elements are equal.
struct unique : duplicates
{
    static constexpr tag value = tag::unique;
    static constexpr std::string_view name = "unique";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "No two elements are equal.";
    static constexpr std::array<std::string_view, 3> phrases { "no duplicates", "distinct", "each at most once" };
    static constexpr kind makes = kind::multiset;
};

struct any_keys
{
    using root = any_keys;
    static constexpr tag value = tag::any_keys;
    static constexpr std::string_view name = "any_keys";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Which keys?";
};
/// The keys are 0..n-1, or nearly so.
struct dense : any_keys
{
    static constexpr tag value = tag::dense;
    static constexpr std::string_view name = "dense";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "The keys are 0..n-1, or nearly so: indices filling their range.";
    static constexpr std::array<std::string_view, 4> phrases { "ids", "indices", "vertices numbered 0..n-1", "small key range" };
    /// Demanded: a container that keeps it stores by index, so it needs keys that are indices, which the request must
    /// promise (its storage grows with the largest key, and a negative key is no index at all).
    static constexpr bool demanded = true;
};
/// The keys are few compared to their range.
struct sparse : any_keys
{
    static constexpr tag value = tag::sparse;
    static constexpr std::string_view name = "sparse";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "The keys are few compared to their range.";
    static constexpr std::array<std::string_view, 3> phrases { "few keys in a large range", "scattered keys", "arbitrary keys" };
};

struct moving_refs
{
    using root = moving_refs;
    static constexpr tag value = tag::moving_refs;
    static constexpr std::string_view name = "moving_refs";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Do references stay valid?";
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
    static constexpr std::string_view name = "stable_refs";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "A reference to an element stays valid until that element is removed.";
    static constexpr std::array<std::string_view, 3> phrases { "pointers stay valid", "stable addresses", "references survive inserts" };
};

struct any_layout
{
    using root = any_layout;
    static constexpr tag value = tag::any_layout;
    static constexpr std::string_view name = "any_layout";
    using group = semantics_group;
    using heading = memory_heading;
    static constexpr std::string_view question = "One array?";
};
/// The elements lie next to each other in memory.
struct contiguous : any_layout
{
    static constexpr tag value = tag::contiguous;
    static constexpr std::string_view name = "contiguous";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "The elements lie next to each other in memory.";
    static constexpr std::array<std::string_view, 4> phrases { "contiguous", "cache friendly", "as a span", "one array" };
};

struct any_representation
{
    using root = any_representation;
    static constexpr tag value = tag::any_representation;
    static constexpr std::string_view name = "any_representation";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Same contents, same bytes?";
};
/// Equal contents have equal representations.
struct canonical : any_representation
{
    static constexpr tag value = tag::canonical;
    static constexpr std::string_view name = "canonical";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Equal contents have equal representations.";
    static constexpr std::array<std::string_view, 3> phrases { "canonical", "same contents same bytes", "hashable by contents" };
};

struct single_threaded
{
    using root = single_threaded;
    static constexpr tag value = tag::single_threaded;
    static constexpr std::string_view name = "single_threaded";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Shared between threads?";
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
    static constexpr std::string_view name = "concurrent";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Threads share it: inserts from several threads at once, and the reads each kind allows beside them.";
    static constexpr std::array<std::string_view, 3> phrases { "thread safe", "shared between threads", "parallel inserts" };
};

struct any_memory
{
    using root = any_memory;
    static constexpr tag value = tag::any_memory;
    static constexpr std::string_view name = "any_memory";
    using group = costs_group;
    static constexpr std::string_view question = "What happens to freed memory?";
    using heading = no_heading;
};
/// Storage that erase and clear free is kept for reuse: refilling up to an earlier size allocates nothing.
struct retains : any_memory
{
    static constexpr tag value = tag::retains;
    static constexpr std::string_view name = "retains";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Storage freed by erase and clear is kept for reuse.";
    static constexpr std::array<std::string_view, 3> phrases { "no reallocation when refilled", "reuse memory", "keeps capacity" };
};
/// Erase and clear return the storage of what they remove.
struct reclaims : any_memory
{
    static constexpr tag value = tag::reclaims;
    static constexpr std::string_view name = "reclaims";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Erase and clear return the storage of what they remove.";
    static constexpr std::array<std::string_view, 3> phrases { "frees memory", "shrinks", "gives memory back" };
};

struct any_clear
{
    using root = any_clear;
    static constexpr tag value = tag::any_clear;
    static constexpr std::string_view name = "any_clear";
    using group = costs_group;
    static constexpr std::string_view question = "How is it cleared?";
    using heading = no_heading;
};
/// Clear takes time in proportion to the elements present, not to the keys or capacity seen before.
struct proportional_clear : any_clear
{
    static constexpr tag value = tag::proportional_clear;
    static constexpr std::string_view name = "proportional_clear";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Clear takes time in proportion to the elements present.";
    static constexpr std::array<std::string_view, 2> phrases { "cheap clear", "cleared often" };
};
/// Clear takes constant time, as with a generation counter.
struct constant_clear : proportional_clear
{
    static constexpr tag value = tag::constant_clear;
    static constexpr std::string_view name = "constant_clear";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Clear takes constant time, as with a generation counter.";
    static constexpr std::array<std::string_view, 3> phrases { "reset every round", "cleared per search", "O(1) clear" };
};

struct any_access
{
    using root = any_access;
    static constexpr tag value = tag::any_access;
    static constexpr std::string_view name = "any_access";
    using group = kind_group;
    static constexpr std::string_view question = "How are elements taken out?";
    using heading = no_heading;
};
/// A queue: elements leave in the order they came (front, push, pop).
struct fifo : any_access
{
    static constexpr tag value = tag::fifo;
    static constexpr std::string_view name = "fifo";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A queue: elements leave in the order they came.";
    static constexpr std::array<std::string_view, 4> phrases { "queue", "fifo", "breadth first", "worklist" };
    static constexpr kind makes = kind::queue;
};
/// A stack: the last element pushed leaves first (top, push, pop).
struct lifo : any_access
{
    static constexpr tag value = tag::lifo;
    static constexpr std::string_view name = "lifo";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A stack: the last element pushed leaves first.";
    static constexpr std::array<std::string_view, 3> phrases { "stack", "lifo", "depth first" };
    static constexpr kind makes = kind::queue;
};
/// A priority queue: the least element by urd::Less leaves first (top, push, pop).
struct by_priority : any_access
{
    static constexpr tag value = tag::by_priority;
    static constexpr std::string_view name = "by_priority";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A priority queue: the least element leaves first.";
    static constexpr std::array<std::string_view, 4> phrases { "priority queue", "heap", "best first", "open list" };
    static constexpr kind makes = kind::queue;
};

struct any_scope
{
    using root = any_scope;
    static constexpr tag value = tag::any_scope;
    static constexpr std::string_view name = "any_scope";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Nested scopes?";
};
/// Offers push_scope and pop_scope: popping undoes every change since the matching push.
struct scoped : any_scope
{
    static constexpr tag value = tag::scoped;
    static constexpr std::string_view name = "scoped";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "push_scope and pop_scope: popping undoes every change since the matching push.";
    static constexpr std::array<std::string_view, 3> phrases { "nested scopes", "undo on backtrack", "symbol table" };
};

struct any_layering
{
    using root = any_layering;
    static constexpr tag value = tag::any_layering;
    static constexpr std::string_view name = "any_layering";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Layered over a parent?";
};
/// Offers layer_over(parent): lookups find the parent's elements too, which it neither owns nor changes.
struct layered : any_layering
{
    static constexpr tag value = tag::layered;
    static constexpr std::string_view name = "layered";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "layer_over(parent): lookups find the parent's elements too, which it neither owns nor changes.";
    static constexpr std::array<std::string_view, 3> phrases { "falls through to a parent", "overlay", "phase over phase" };
};

struct any_handle
{
    using root = any_handle;
    static constexpr tag value = tag::any_handle;
    static constexpr std::string_view name = "any_handle";
    using group = kind_group;
    static constexpr std::string_view question = "Reached by handle?";
    using heading = no_heading;
    /// Exact: a container must hold what the request holds here, no more (handles make a pool, or give a priority queue update and erase by handle).
    static constexpr bool exact = true;
};
/// A pool: each element is identified by a handle that insert gives, with whether it was new, and equal elements get
/// equal handles. Not iterated; counted, with handles 0..size()-1, unless it has structural_sharing (handles name
/// shared nodes there).
struct handles : any_handle
{
    static constexpr tag value = tag::handles;
    static constexpr std::string_view name = "handles";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A pool: insert gives a handle; with unique, equal elements get equal handles.";
    static constexpr std::array<std::string_view, 4> phrases { "interning", "handles", "deduplicated rows", "hash consing" };
    static constexpr kind makes = kind::pool;
};

struct any_sharing
{
    using root = any_sharing;
    static constexpr tag value = tag::any_sharing;
    static constexpr std::string_view name = "any_sharing";
    using group = costs_group;
    static constexpr std::string_view question = "Rows share structure?";
    using heading = no_heading;
};
/// Parts that are equal across elements (rows) are stored once.
struct structural_sharing : any_sharing
{
    static constexpr tag value = tag::structural_sharing;
    static constexpr std::string_view name = "structural_sharing";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Parts that are equal across rows are stored once.";
    static constexpr std::array<std::string_view, 3> phrases { "rows differing in a few places", "persistent", "shared structure" };
};

struct any_direction
{
    using root = any_direction;
    static constexpr tag value = tag::any_direction;
    static constexpr std::string_view name = "any_direction";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Looked up both ways?";
};
/// A map that answers by value too: keys(v) gives the keys that map to v, as a graph gives a vertex's predecessors.
struct bidirectional : any_direction
{
    static constexpr tag value = tag::bidirectional;
    static constexpr std::string_view name = "bidirectional";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "A map that answers by value too: keys(v) gives the keys that map to v.";
    static constexpr std::array<std::string_view, 3> phrases { "predecessors", "reverse edges", "inverse map" };
};

struct any_shape
{
    using root = any_shape;
    static constexpr tag value = tag::any_shape;
    static constexpr std::string_view name = "any_shape";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Rows of a fixed shape?";
};
/// Rows of a fixed shape: how many there are and how long each is is set at construction (urd::row_lengths); only
/// their contents change.
struct fixed_shape : any_shape
{
    static constexpr tag value = tag::fixed_shape;
    static constexpr std::string_view name = "fixed_shape";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Rows whose number and lengths are set at construction; only their contents change.";
    static constexpr std::array<std::string_view, 3> phrases { "fixed rows", "matrix", "table of known shape" };
};

struct any_packing
{
    using root = any_packing;
    static constexpr tag value = tag::any_packing;
    static constexpr std::string_view name = "any_packing";
    using group = semantics_group;
    using heading = memory_heading;
    static constexpr std::string_view question = "Records packed flat?";
};
/// Each element is one self-contained block of bytes, with no part of its own on the heap: trivially copyable, or
/// serialized as a whole. Such elements are frozen once stored, and are read by index.
struct flat : any_packing
{
    static constexpr tag value = tag::flat;
    static constexpr std::string_view name = "flat";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Each record is one self-contained block of bytes, frozen once stored and read by index.";
    static constexpr std::array<std::string_view, 3> phrases { "flat records", "serialized", "one block per record" };
    static constexpr kind makes = kind::records;
};

struct any_grouping
{
    using root = any_grouping;
    static constexpr tag value = tag::any_grouping;
    static constexpr std::string_view name = "any_grouping";
    using group = kind_group;
    static constexpr std::string_view question = "Groups of keys?";
    using heading = no_heading;
};
/// A partition: its elements fall into disjoint classes; merge joins two classes, find names an element's class.
struct partition : any_grouping
{
    static constexpr tag value = tag::partition;
    static constexpr std::string_view name = "partition";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A partition: elements fall into disjoint classes that merge joins.";
    static constexpr std::array<std::string_view, 3> phrases { "union find", "disjoint sets", "equivalence classes" };
    static constexpr kind makes = kind::partition;
};

struct any_tracking
{
    using root = any_tracking;
    static constexpr tag value = tag::any_tracking;
    static constexpr std::string_view name = "any_tracking";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Tracks what changed?";
};
/// Offers added() and removed(): exactly the elements that changed since clear_changes(), for semi-naive evaluation.
struct change_log : any_tracking
{
    static constexpr tag value = tag::change_log;
    static constexpr std::string_view name = "change_log";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "added() and removed(): the elements changed since clear_changes().";
    static constexpr std::array<std::string_view, 3> phrases { "delta", "what changed", "semi-naive evaluation" };
};

/// Offers no change.
struct immutable
{
    using root = immutable;
    static constexpr tag value = tag::immutable;
    static constexpr std::string_view name = "immutable";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Grows or shrinks? (follows from monotone and frozen)";
};
/// Offers insert.
struct growable : immutable
{
    static constexpr tag value = tag::growable;
    static constexpr std::string_view name = "growable";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Offers insert.";
    static constexpr std::array<std::string_view, 1> phrases { "grows" };
};
/// Offers insert, erase and clear.
struct shrinkable : growable
{
    static constexpr tag value = tag::shrinkable;
    static constexpr std::string_view name = "shrinkable";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Offers insert, erase and clear.";
    static constexpr std::array<std::string_view, 1> phrases { "add and remove" };
};

// What a Store's interface enforces: no container declares a lifecycle, emptiness or identity. Multiplicity a
// multimap declares (multi), but a request only states it.
struct may_change
{
    using root = may_change;
    static constexpr tag value = tag::may_change;
    static constexpr std::string_view name = "may_change";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Does it change after building?";
};
/// Only grows: nothing is ever removed.
struct monotone : may_change
{
    static constexpr tag value = tag::monotone;
    static constexpr std::string_view name = "monotone";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Only grows: nothing is ever removed.";
    static constexpr std::array<std::string_view, 3> phrases { "append only", "only grows", "never erased" };
};
/// No change after construction (assign rebuilds it whole): any number of threads may read it between rebuilds,
/// so concurrent, which is about inserts from threads at once, is not asked of it.
struct frozen : monotone
{
    static constexpr tag value = tag::frozen;
    static constexpr std::string_view name = "frozen";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "No change after construction; assign rebuilds it whole.";
    static constexpr std::array<std::string_view, 3> phrases { "built once", "read only", "immutable after building" };
};

struct maybe_empty
{
    using root = maybe_empty;
    static constexpr tag value = tag::maybe_empty;
    static constexpr std::string_view name = "maybe_empty";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Can it be empty?";
};
/// At least one element.
struct nonempty : maybe_empty
{
    static constexpr tag value = tag::nonempty;
    static constexpr std::string_view name = "nonempty";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "At least one element.";
    static constexpr std::array<std::string_view, 2> phrases { "never empty", "nonempty" };
};

struct any_multiplicity
{
    using root = any_multiplicity;
    static constexpr tag value = tag::any_multiplicity;
    static constexpr std::string_view name = "any_multiplicity";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Values per key?";
};
/// At most one value per key (a map).
struct functional : any_multiplicity
{
    static constexpr tag value = tag::functional;
    static constexpr std::string_view name = "functional";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "At most one value per key: a map.";
    static constexpr std::array<std::string_view, 3> phrases { "map", "one value per key", "lookup table" };
};
/// Any number of values per key (a multimap).
struct multi : any_multiplicity
{
    static constexpr tag value = tag::multi;
    static constexpr std::string_view name = "multi";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Any number of values per key: a multimap.";
    static constexpr std::array<std::string_view, 3> phrases { "multimap", "many values per key", "adjacency lists" };
    static constexpr kind makes = kind::multiset;
};

struct by_key
{
    using root = by_key;
    static constexpr tag value = tag::by_key;
    static constexpr std::string_view name = "by_key";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Reached by position?";
};
/// Identified by position 0..n-1, in order (a sequence).
struct positional : by_key
{
    static constexpr tag value = tag::positional;
    static constexpr std::string_view name = "positional";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Identified by position 0..n-1, in order.";
    static constexpr std::array<std::string_view, 3> phrases { "by position", "numbered as inserted", "interner" };
};

// How a Store is used: it changes which representation is chosen, never what the Store guarantees. Only a
// request says it; docs/BENCHMARKS.md backs what each one chooses.
struct general
{
    using root = general;
    static constexpr tag value = tag::general;
    static constexpr std::string_view name = "general";
    using group = hints_group;
    static constexpr std::string_view question = "How many elements?";
    using heading = no_heading;
};
/// A few elements, or a few at a time.
struct small : general
{
    static constexpr tag value = tag::small;
    static constexpr std::string_view name = "small";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A few elements, or a few at a time: compact containers first.";
    static constexpr std::array<std::string_view, 3> phrases { "few elements", "small", "tiny" };
};

struct any_priority
{
    using root = any_priority;
    static constexpr tag value = tag::any_priority;
    static constexpr std::string_view name = "any_priority";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "How do priorities move?";
};
/// A priority queue whose pushes are never below the last popped (shortest paths with non-negative lengths, event
/// simulation): with integral priorities, a bucket queue serves it.
struct monotone_priority : any_priority
{
    static constexpr tag value = tag::monotone_priority;
    static constexpr std::string_view name = "monotone_priority";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "A priority queue never pushed below the last popped.";
    static constexpr std::array<std::string_view, 3> phrases { "dijkstra", "non-negative lengths", "event simulation" };
    static constexpr kind makes = kind::queue;
};

}  // namespace urd
