#pragma once

#include "urd/semantics/concepts.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

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

// Properties, in the style of Boost.Graph's graph_traits: each answers one question (a category) with a tag
// type, and a tag derived from another means "at least that". The root of a category promises nothing, and names
// itself as `root`, which every tag of its category inherits. Each tag also carries its number in urd::tag, so that
// properties are computed as constexpr values, not as types: a request costs the compiler a constant evaluation
// instead of hundreds of class instantiations.

// Each tag's words, beside it: tag_synopsis<T>. A tag that is no root has its name, its role to the one who writes a
// request (guarantee: a container declares it, a request may ask for it; demand: the caller states it about its data
// and a container may rely on it; kind: it makes the Store a queue, a pool or a partition; request: only a request
// states it, the Store's interface enforces it; hint: it reorders the candidates, never changes a guarantee), a
// description that follows its comment, and phrases, ways a need is put in plain words. A category's root has its name,
// the theme it belongs to and the question its tags answer. share/urd/vocabulary.json is generated from them.
template<typename Tag>
struct tag_synopsis
{
};

// The themes, the areas a reader may care about, in the order a request is best thought through: they group the
// categories and have no rules of their own.
struct kind_theme
{
    static constexpr std::string_view id = "kind";
    static constexpr std::string_view title = "What it is";
    static constexpr std::string_view summary = "Whether the Store is a set, map or sequence, a queue, a pool or a partition, and how it is reached.";
};
struct data_theme
{
    static constexpr std::string_view id = "data";
    static constexpr std::string_view title = "What the data is like";
    static constexpr std::string_view summary = "Promises about the elements: which keys, duplicates, emptiness, priorities, how many.";
};
struct order_theme
{
    static constexpr std::string_view id = "order";
    static constexpr std::string_view title = "Order";
    static constexpr std::string_view summary = "In which order the elements are iterated, and whether they are reached by position.";
};
struct change_theme
{
    static constexpr std::string_view id = "change";
    static constexpr std::string_view title = "Change over time";
    static constexpr std::string_view summary = "Whether the Store changes after building, how it is cleared, and what it remembers of changes.";
};
struct memory_theme
{
    static constexpr std::string_view id = "memory";
    static constexpr std::string_view title = "Memory and layout";
    static constexpr std::string_view summary = "Where elements live: stable references, one array, packed records, shared rows, freed memory.";
};
struct threads_theme
{
    static constexpr std::string_view id = "threads";
    static constexpr std::string_view title = "Threads";
    static constexpr std::string_view summary = "Whether several threads change the Store at once.";
};

/// The themes, in order.
using all_themes = std::tuple<kind_theme, data_theme, order_theme, change_theme, memory_theme, threads_theme>;

// What a container guarantees by itself.
struct any_order
{
    using root = any_order;
    static constexpr tag value = tag::any_order;
};
template<>
struct tag_synopsis<any_order>
{
    static constexpr std::string_view name = "any_order";
    using theme = order_theme;
    static constexpr std::string_view question = "In which order is it iterated?";
};
/// Ascending by urd::Less: operator< for numbers, member by member for identified types.
struct sorted : any_order
{
    static constexpr tag value = tag::sorted;
};
template<>
struct tag_synopsis<sorted>
{
    static constexpr std::string_view name = "sorted";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Iterated in ascending order by urd::Less.";
    static constexpr std::array<std::string_view, 4> phrases { "sorted", "in order", "ordered by key", "range queries" };
};
/// In the order they were inserted: a new element goes last, and the others keep their places.
struct insertion_ordered : any_order
{
    static constexpr tag value = tag::insertion_ordered;
};
template<>
struct tag_synopsis<insertion_ordered>
{
    static constexpr std::string_view name = "insertion_ordered";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Iterated in the order elements were first inserted.";
    static constexpr std::array<std::string_view, 3> phrases { "insertion order", "first come first listed", "keeps the order added" };
};

struct duplicates
{
    using root = duplicates;
    static constexpr tag value = tag::duplicates;
};
template<>
struct tag_synopsis<duplicates>
{
    static constexpr std::string_view name = "duplicates";
    using theme = data_theme;
    static constexpr std::string_view question = "Duplicates allowed?";
};
/// No two elements are equal.
struct unique : duplicates
{
    static constexpr tag value = tag::unique;
};
template<>
struct tag_synopsis<unique>
{
    static constexpr std::string_view name = "unique";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "No two elements are equal.";
    static constexpr std::array<std::string_view, 3> phrases { "no duplicates", "distinct", "each at most once" };
};

struct any_keys
{
    using root = any_keys;
    static constexpr tag value = tag::any_keys;
};
template<>
struct tag_synopsis<any_keys>
{
    static constexpr std::string_view name = "any_keys";
    using theme = data_theme;
    static constexpr std::string_view question = "Which keys?";
};
/// The keys are 0..n-1, or nearly so.
struct dense : any_keys
{
    static constexpr tag value = tag::dense;
};
template<>
struct tag_synopsis<dense>
{
    static constexpr std::string_view name = "dense";
    static constexpr std::string_view role = "demand";
    static constexpr std::string_view description = "The keys are 0..n-1, or nearly so: indices filling their range.";
    static constexpr std::array<std::string_view, 4> phrases { "ids", "indices", "vertices numbered 0..n-1", "small key range" };
};
/// The keys are few compared to their range.
struct sparse : any_keys
{
    static constexpr tag value = tag::sparse;
};
template<>
struct tag_synopsis<sparse>
{
    static constexpr std::string_view name = "sparse";
    static constexpr std::string_view role = "demand";
    static constexpr std::string_view description = "The keys are few compared to their range.";
    static constexpr std::array<std::string_view, 3> phrases { "few keys in a large range", "scattered keys", "arbitrary keys" };
};

struct moving_refs
{
    using root = moving_refs;
    static constexpr tag value = tag::moving_refs;
};
template<>
struct tag_synopsis<moving_refs>
{
    static constexpr std::string_view name = "moving_refs";
    using theme = memory_theme;
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
};
template<>
struct tag_synopsis<stable_refs>
{
    static constexpr std::string_view name = "stable_refs";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "A reference to an element stays valid until that element is removed.";
    static constexpr std::array<std::string_view, 3> phrases { "pointers stay valid", "stable addresses", "references survive inserts" };
};

struct any_layout
{
    using root = any_layout;
    static constexpr tag value = tag::any_layout;
};
template<>
struct tag_synopsis<any_layout>
{
    static constexpr std::string_view name = "any_layout";
    using theme = memory_theme;
    static constexpr std::string_view question = "One array?";
};
/// The elements lie next to each other in memory.
struct contiguous : any_layout
{
    static constexpr tag value = tag::contiguous;
};
template<>
struct tag_synopsis<contiguous>
{
    static constexpr std::string_view name = "contiguous";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "The elements lie next to each other in memory.";
    static constexpr std::array<std::string_view, 4> phrases { "contiguous", "cache friendly", "as a span", "one array" };
};

struct any_representation
{
    using root = any_representation;
    static constexpr tag value = tag::any_representation;
};
template<>
struct tag_synopsis<any_representation>
{
    static constexpr std::string_view name = "any_representation";
    using theme = memory_theme;
    static constexpr std::string_view question = "Same contents, same bytes?";
};
/// Equal contents have equal representations.
struct canonical : any_representation
{
    static constexpr tag value = tag::canonical;
};
template<>
struct tag_synopsis<canonical>
{
    static constexpr std::string_view name = "canonical";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Equal contents have equal representations.";
    static constexpr std::array<std::string_view, 3> phrases { "canonical", "same contents same bytes", "hashable by contents" };
};

struct single_threaded
{
    using root = single_threaded;
    static constexpr tag value = tag::single_threaded;
};
template<>
struct tag_synopsis<single_threaded>
{
    static constexpr std::string_view name = "single_threaded";
    using theme = threads_theme;
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
};
template<>
struct tag_synopsis<concurrent>
{
    static constexpr std::string_view name = "concurrent";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Threads share it: inserts from several threads at once, and the reads each kind allows beside them.";
    static constexpr std::array<std::string_view, 3> phrases { "thread safe", "shared between threads", "parallel inserts" };
};

struct any_memory
{
    using root = any_memory;
    static constexpr tag value = tag::any_memory;
};
template<>
struct tag_synopsis<any_memory>
{
    static constexpr std::string_view name = "any_memory";
    using theme = memory_theme;
    static constexpr std::string_view question = "What happens to freed memory?";
};
/// Storage that erase and clear free is kept for reuse: refilling up to an earlier size allocates nothing.
struct retains : any_memory
{
    static constexpr tag value = tag::retains;
};
template<>
struct tag_synopsis<retains>
{
    static constexpr std::string_view name = "retains";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Storage freed by erase and clear is kept for reuse.";
    static constexpr std::array<std::string_view, 3> phrases { "no reallocation when refilled", "reuse memory", "keeps capacity" };
};
/// Erase and clear return the storage of what they remove.
struct reclaims : any_memory
{
    static constexpr tag value = tag::reclaims;
};
template<>
struct tag_synopsis<reclaims>
{
    static constexpr std::string_view name = "reclaims";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Erase and clear return the storage of what they remove.";
    static constexpr std::array<std::string_view, 3> phrases { "frees memory", "shrinks", "gives memory back" };
};

struct any_clear
{
    using root = any_clear;
    static constexpr tag value = tag::any_clear;
};
template<>
struct tag_synopsis<any_clear>
{
    static constexpr std::string_view name = "any_clear";
    using theme = change_theme;
    static constexpr std::string_view question = "How is it cleared?";
};
/// Clear takes time in proportion to the elements present, not to the keys or capacity seen before.
struct proportional_clear : any_clear
{
    static constexpr tag value = tag::proportional_clear;
};
template<>
struct tag_synopsis<proportional_clear>
{
    static constexpr std::string_view name = "proportional_clear";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Clear takes time in proportion to the elements present.";
    static constexpr std::array<std::string_view, 2> phrases { "cheap clear", "cleared often" };
};
/// Clear takes constant time, as with a generation counter.
struct constant_clear : proportional_clear
{
    static constexpr tag value = tag::constant_clear;
};
template<>
struct tag_synopsis<constant_clear>
{
    static constexpr std::string_view name = "constant_clear";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Clear takes constant time, as with a generation counter.";
    static constexpr std::array<std::string_view, 3> phrases { "reset every round", "cleared per search", "O(1) clear" };
};

struct any_access
{
    using root = any_access;
    static constexpr tag value = tag::any_access;
};
template<>
struct tag_synopsis<any_access>
{
    static constexpr std::string_view name = "any_access";
    using theme = kind_theme;
    static constexpr std::string_view question = "How are elements taken out?";
};
/// A queue: elements leave in the order they came (front, push, pop).
struct fifo : any_access
{
    static constexpr tag value = tag::fifo;
};
template<>
struct tag_synopsis<fifo>
{
    static constexpr std::string_view name = "fifo";
    static constexpr std::string_view role = "kind";
    static constexpr std::string_view description = "A queue: elements leave in the order they came.";
    static constexpr std::array<std::string_view, 4> phrases { "queue", "fifo", "breadth first", "worklist" };
};
/// A stack: the last element pushed leaves first (top, push, pop).
struct lifo : any_access
{
    static constexpr tag value = tag::lifo;
};
template<>
struct tag_synopsis<lifo>
{
    static constexpr std::string_view name = "lifo";
    static constexpr std::string_view role = "kind";
    static constexpr std::string_view description = "A stack: the last element pushed leaves first.";
    static constexpr std::array<std::string_view, 3> phrases { "stack", "lifo", "depth first" };
};
/// A priority queue: the least element by urd::Less leaves first (top, push, pop).
struct by_priority : any_access
{
    static constexpr tag value = tag::by_priority;
};
template<>
struct tag_synopsis<by_priority>
{
    static constexpr std::string_view name = "by_priority";
    static constexpr std::string_view role = "kind";
    static constexpr std::string_view description = "A priority queue: the least element leaves first.";
    static constexpr std::array<std::string_view, 4> phrases { "priority queue", "heap", "best first", "open list" };
};

struct any_scope
{
    using root = any_scope;
    static constexpr tag value = tag::any_scope;
};
template<>
struct tag_synopsis<any_scope>
{
    static constexpr std::string_view name = "any_scope";
    using theme = change_theme;
    static constexpr std::string_view question = "Nested scopes?";
};
/// Offers push_scope and pop_scope: popping undoes every change since the matching push.
struct scoped : any_scope
{
    static constexpr tag value = tag::scoped;
};
template<>
struct tag_synopsis<scoped>
{
    static constexpr std::string_view name = "scoped";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "push_scope and pop_scope: popping undoes every change since the matching push.";
    static constexpr std::array<std::string_view, 3> phrases { "nested scopes", "undo on backtrack", "symbol table" };
};

struct any_layering
{
    using root = any_layering;
    static constexpr tag value = tag::any_layering;
};
template<>
struct tag_synopsis<any_layering>
{
    static constexpr std::string_view name = "any_layering";
    using theme = change_theme;
    static constexpr std::string_view question = "Layered over a parent?";
};
/// Offers layer_over(parent): lookups find the parent's elements too, which it neither owns nor changes.
struct layered : any_layering
{
    static constexpr tag value = tag::layered;
};
template<>
struct tag_synopsis<layered>
{
    static constexpr std::string_view name = "layered";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "layer_over(parent): lookups find the parent's elements too, which it neither owns nor changes.";
    static constexpr std::array<std::string_view, 3> phrases { "falls through to a parent", "overlay", "phase over phase" };
};

struct any_handle
{
    using root = any_handle;
    static constexpr tag value = tag::any_handle;
};
template<>
struct tag_synopsis<any_handle>
{
    static constexpr std::string_view name = "any_handle";
    using theme = kind_theme;
    static constexpr std::string_view question = "Reached by handle?";
};
/// A pool: each element is identified by a handle that insert gives, with whether it was new, and equal elements get
/// equal handles. Not iterated; counted, with handles 0..size()-1, unless it has structural_sharing (handles name
/// shared nodes there).
struct handles : any_handle
{
    static constexpr tag value = tag::handles;
};
template<>
struct tag_synopsis<handles>
{
    static constexpr std::string_view name = "handles";
    static constexpr std::string_view role = "kind";
    static constexpr std::string_view description = "A pool: insert gives a handle; with unique, equal elements get equal handles.";
    static constexpr std::array<std::string_view, 4> phrases { "interning", "handles", "deduplicated rows", "hash consing" };
};

struct any_sharing
{
    using root = any_sharing;
    static constexpr tag value = tag::any_sharing;
};
template<>
struct tag_synopsis<any_sharing>
{
    static constexpr std::string_view name = "any_sharing";
    using theme = memory_theme;
    static constexpr std::string_view question = "Rows share structure?";
};
/// Parts that are equal across elements (rows) are stored once.
struct structural_sharing : any_sharing
{
    static constexpr tag value = tag::structural_sharing;
};
template<>
struct tag_synopsis<structural_sharing>
{
    static constexpr std::string_view name = "structural_sharing";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Parts that are equal across rows are stored once.";
    static constexpr std::array<std::string_view, 3> phrases { "rows differing in a few places", "persistent", "shared structure" };
};

struct any_direction
{
    using root = any_direction;
    static constexpr tag value = tag::any_direction;
};
template<>
struct tag_synopsis<any_direction>
{
    static constexpr std::string_view name = "any_direction";
    using theme = kind_theme;
    static constexpr std::string_view question = "Looked up both ways?";
};
/// A map that answers by value too: keys(v) gives the keys that map to v, as a graph gives a vertex's predecessors.
struct bidirectional : any_direction
{
    static constexpr tag value = tag::bidirectional;
};
template<>
struct tag_synopsis<bidirectional>
{
    static constexpr std::string_view name = "bidirectional";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "A map that answers by value too: keys(v) gives the keys that map to v.";
    static constexpr std::array<std::string_view, 3> phrases { "predecessors", "reverse edges", "inverse map" };
};

struct any_shape
{
    using root = any_shape;
    static constexpr tag value = tag::any_shape;
};
template<>
struct tag_synopsis<any_shape>
{
    static constexpr std::string_view name = "any_shape";
    using theme = kind_theme;
    static constexpr std::string_view question = "Rows of a fixed shape?";
};
/// Rows of a fixed shape: how many there are and how long each is is set at construction (urd::row_lengths); only
/// their contents change.
struct fixed_shape : any_shape
{
    static constexpr tag value = tag::fixed_shape;
};
template<>
struct tag_synopsis<fixed_shape>
{
    static constexpr std::string_view name = "fixed_shape";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Rows whose number and lengths are set at construction; only their contents change.";
    static constexpr std::array<std::string_view, 3> phrases { "fixed rows", "matrix", "table of known shape" };
};

struct any_packing
{
    using root = any_packing;
    static constexpr tag value = tag::any_packing;
};
template<>
struct tag_synopsis<any_packing>
{
    static constexpr std::string_view name = "any_packing";
    using theme = memory_theme;
    static constexpr std::string_view question = "Records packed flat?";
};
/// Each element is one self-contained block of bytes, with no part of its own on the heap: trivially copyable, or
/// serialized as a whole. Such elements are frozen once stored, and are read by index.
struct flat : any_packing
{
    static constexpr tag value = tag::flat;
};
template<>
struct tag_synopsis<flat>
{
    static constexpr std::string_view name = "flat";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Each record is one self-contained block of bytes, frozen once stored and read by index.";
    static constexpr std::array<std::string_view, 3> phrases { "flat records", "serialized", "one block per record" };
};

struct any_grouping
{
    using root = any_grouping;
    static constexpr tag value = tag::any_grouping;
};
template<>
struct tag_synopsis<any_grouping>
{
    static constexpr std::string_view name = "any_grouping";
    using theme = kind_theme;
    static constexpr std::string_view question = "Groups of keys?";
};
/// A partition: its elements fall into disjoint classes; merge joins two classes, find names an element's class.
struct partition : any_grouping
{
    static constexpr tag value = tag::partition;
};
template<>
struct tag_synopsis<partition>
{
    static constexpr std::string_view name = "partition";
    static constexpr std::string_view role = "kind";
    static constexpr std::string_view description = "A partition: elements fall into disjoint classes that merge joins.";
    static constexpr std::array<std::string_view, 3> phrases { "union find", "disjoint sets", "equivalence classes" };
};

struct any_tracking
{
    using root = any_tracking;
    static constexpr tag value = tag::any_tracking;
};
template<>
struct tag_synopsis<any_tracking>
{
    static constexpr std::string_view name = "any_tracking";
    using theme = change_theme;
    static constexpr std::string_view question = "Tracks what changed?";
};
/// Offers added() and removed(): exactly the elements that changed since clear_changes(), for semi-naive evaluation.
struct change_log : any_tracking
{
    static constexpr tag value = tag::change_log;
};
template<>
struct tag_synopsis<change_log>
{
    static constexpr std::string_view name = "change_log";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "added() and removed(): the elements changed since clear_changes().";
    static constexpr std::array<std::string_view, 3> phrases { "delta", "what changed", "semi-naive evaluation" };
};

/// Offers no change.
struct immutable
{
    using root = immutable;
    static constexpr tag value = tag::immutable;
};
template<>
struct tag_synopsis<immutable>
{
    static constexpr std::string_view name = "immutable";
    using theme = change_theme;
    static constexpr std::string_view question = "Grows or shrinks? (follows from monotone and frozen)";
};
/// Offers insert.
struct growable : immutable
{
    static constexpr tag value = tag::growable;
};
template<>
struct tag_synopsis<growable>
{
    static constexpr std::string_view name = "growable";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Offers insert.";
    static constexpr std::array<std::string_view, 1> phrases { "grows" };
};
/// Offers insert, erase and clear.
struct shrinkable : growable
{
    static constexpr tag value = tag::shrinkable;
};
template<>
struct tag_synopsis<shrinkable>
{
    static constexpr std::string_view name = "shrinkable";
    static constexpr std::string_view role = "guarantee";
    static constexpr std::string_view description = "Offers insert, erase and clear.";
    static constexpr std::array<std::string_view, 1> phrases { "add and remove" };
};

// What a Store's interface enforces: no container declares a lifecycle, emptiness or identity. Multiplicity a
// multimap declares (multi), but a request only states it.
struct may_change
{
    using root = may_change;
    static constexpr tag value = tag::may_change;
};
template<>
struct tag_synopsis<may_change>
{
    static constexpr std::string_view name = "may_change";
    using theme = change_theme;
    static constexpr std::string_view question = "Does it change after building?";
};
/// Only grows: nothing is ever removed.
struct monotone : may_change
{
    static constexpr tag value = tag::monotone;
};
template<>
struct tag_synopsis<monotone>
{
    static constexpr std::string_view name = "monotone";
    static constexpr std::string_view role = "request";
    static constexpr std::string_view description = "Only grows: nothing is ever removed.";
    static constexpr std::array<std::string_view, 3> phrases { "append only", "only grows", "never erased" };
};
/// No change after construction (assign rebuilds it whole): any number of threads may read it between rebuilds,
/// so concurrent, which is about inserts from threads at once, is not asked of it.
struct frozen : monotone
{
    static constexpr tag value = tag::frozen;
};
template<>
struct tag_synopsis<frozen>
{
    static constexpr std::string_view name = "frozen";
    static constexpr std::string_view role = "request";
    static constexpr std::string_view description = "No change after construction; assign rebuilds it whole.";
    static constexpr std::array<std::string_view, 3> phrases { "built once", "read only", "immutable after building" };
};

struct maybe_empty
{
    using root = maybe_empty;
    static constexpr tag value = tag::maybe_empty;
};
template<>
struct tag_synopsis<maybe_empty>
{
    static constexpr std::string_view name = "maybe_empty";
    using theme = data_theme;
    static constexpr std::string_view question = "Can it be empty?";
};
/// At least one element.
struct nonempty : maybe_empty
{
    static constexpr tag value = tag::nonempty;
};
template<>
struct tag_synopsis<nonempty>
{
    static constexpr std::string_view name = "nonempty";
    static constexpr std::string_view role = "request";
    static constexpr std::string_view description = "At least one element.";
    static constexpr std::array<std::string_view, 2> phrases { "never empty", "nonempty" };
};

struct any_multiplicity
{
    using root = any_multiplicity;
    static constexpr tag value = tag::any_multiplicity;
};
template<>
struct tag_synopsis<any_multiplicity>
{
    static constexpr std::string_view name = "any_multiplicity";
    using theme = kind_theme;
    static constexpr std::string_view question = "Values per key?";
};
/// At most one value per key (a map).
struct functional : any_multiplicity
{
    static constexpr tag value = tag::functional;
};
template<>
struct tag_synopsis<functional>
{
    static constexpr std::string_view name = "functional";
    static constexpr std::string_view role = "request";
    static constexpr std::string_view description = "At most one value per key: a map.";
    static constexpr std::array<std::string_view, 3> phrases { "map", "one value per key", "lookup table" };
};
/// Any number of values per key (a multimap).
struct multi : any_multiplicity
{
    static constexpr tag value = tag::multi;
};
template<>
struct tag_synopsis<multi>
{
    static constexpr std::string_view name = "multi";
    static constexpr std::string_view role = "request";
    static constexpr std::string_view description = "Any number of values per key: a multimap.";
    static constexpr std::array<std::string_view, 3> phrases { "multimap", "many values per key", "adjacency lists" };
};

struct by_key
{
    using root = by_key;
    static constexpr tag value = tag::by_key;
};
template<>
struct tag_synopsis<by_key>
{
    static constexpr std::string_view name = "by_key";
    using theme = order_theme;
    static constexpr std::string_view question = "Reached by position?";
};
/// Identified by position 0..n-1, in order (a sequence).
struct positional : by_key
{
    static constexpr tag value = tag::positional;
};
template<>
struct tag_synopsis<positional>
{
    static constexpr std::string_view name = "positional";
    static constexpr std::string_view role = "request";
    static constexpr std::string_view description = "Identified by position 0..n-1, in order.";
    static constexpr std::array<std::string_view, 3> phrases { "by position", "numbered as inserted", "interner" };
};

// How a Store is used: it changes which representation is chosen, never what the Store guarantees. Only a
// request says it; docs/BENCHMARKS.md backs what each one chooses.
struct general
{
    using root = general;
    static constexpr tag value = tag::general;
};
template<>
struct tag_synopsis<general>
{
    static constexpr std::string_view name = "general";
    using theme = data_theme;
    static constexpr std::string_view question = "How many elements?";
};
/// A few elements, or a few at a time.
struct small : general
{
    static constexpr tag value = tag::small;
};
template<>
struct tag_synopsis<small>
{
    static constexpr std::string_view name = "small";
    static constexpr std::string_view role = "hint";
    static constexpr std::string_view description = "A few elements, or a few at a time: compact containers first.";
    static constexpr std::array<std::string_view, 3> phrases { "few elements", "small", "tiny" };
};

struct any_priority
{
    using root = any_priority;
    static constexpr tag value = tag::any_priority;
};
template<>
struct tag_synopsis<any_priority>
{
    static constexpr std::string_view name = "any_priority";
    using theme = data_theme;
    static constexpr std::string_view question = "How do priorities move?";
};
/// A priority queue whose pushes are never below the last popped (shortest paths with non-negative lengths, event
/// simulation): with integral priorities, a bucket queue serves it.
struct monotone_priority : any_priority
{
    static constexpr tag value = tag::monotone_priority;
};
template<>
struct tag_synopsis<monotone_priority>
{
    static constexpr std::string_view name = "monotone_priority";
    static constexpr std::string_view role = "demand";
    static constexpr std::string_view description = "A priority queue never pushed below the last popped.";
    static constexpr std::array<std::string_view, 3> phrases { "dijkstra", "non-negative lengths", "event simulation" };
};

namespace detail
{

/// A list of types.
template<typename... Ts>
struct type_list
{
};

/// Every tag type, in the order of urd::tag.
using all_tags = type_list<any_order,
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
                           monotone_priority>;

inline constexpr std::size_t tag_count = static_cast<std::size_t>(tag::count);
constexpr std::size_t index(tag of) { return static_cast<std::size_t>(of); }

template<typename... Ts>
consteval bool numbered_in_order(type_list<Ts...> /*tags*/)
{
    std::size_t position = 0;
    return sizeof...(Ts) == tag_count && ((index(Ts::value) == position++) && ...);
}
static_assert(numbered_in_order(all_tags {}), "urd::tag and the tag types must list the same properties in the same order");

/// For each tag, its category's root and which tags it is at least (itself, and those it derives from).
struct tag_table
{
    std::array<tag, tag_count> root;
    std::array<std::array<bool, tag_count>, tag_count> at_least;
};
template<typename Tag, typename... Ts>
consteval std::array<bool, tag_count> at_least_row(type_list<Ts...> /*tags*/)
{ return { std::is_base_of_v<Ts, Tag>... }; }
template<typename... Ts>
consteval tag_table make_tag_table(type_list<Ts...> tags)
{ return { { Ts::root::value... }, { at_least_row<Ts>(tags)... } }; }
inline constexpr tag_table tags = make_tag_table(all_tags {});

constexpr tag root_of(tag of) { return tags.root[index(of)]; }
constexpr bool at_least(tag have, tag want) { return tags.at_least[index(have)][index(want)]; }

/// The roots of the categories a container declares, then those a request states. A container may also declare
/// positional (its elements are reached by position: a random-access range) and functional (a value per key: a
/// map); selection reads those two directly, so that it never has to look inside a candidate.
inline constexpr std::array guaranteed_roots { tag::any_order,          tag::duplicates,      tag::any_keys,      tag::moving_refs,  tag::any_layout,
                                               tag::any_representation, tag::single_threaded, tag::immutable,     tag::any_memory,   tag::any_clear,
                                               tag::any_access,         tag::any_scope,       tag::any_layering,  tag::any_tracking, tag::any_handle,
                                               tag::any_sharing,        tag::any_grouping,    tag::any_direction, tag::any_shape,    tag::any_packing };
inline constexpr std::array requested_roots { tag::may_change, tag::maybe_empty, tag::any_multiplicity, tag::by_key, tag::general, tag::any_priority };

constexpr bool in(tag root, const auto& roots)
{
    for (const auto each : roots)
        if (each == root)
            return true;
    return false;
}

/// The properties of something, as a value: for each category, the tag it holds there (by the root's number; the
/// entries of tags that are no root are unused). Equal properties are equal values.
struct property_set
{
    std::array<tag, tag_count> held {};

    /// Nothing said: every category at its root.
    static constexpr property_set roots()
    {
        auto result = property_set {};
        for (std::size_t each = 0; each < tag_count; ++each)
            result.held[each] = static_cast<tag>(each);
        return result;
    }
    /// The given tags, at most one per category, the others at their roots.
    static constexpr property_set of(std::initializer_list<tag> stated)
    {
        auto result = roots();
        for (const auto each : stated)
            result.put(each);
        return result;
    }

    /// The tag held in the category whose root is `root`.
    [[nodiscard]] constexpr tag in(tag root) const { return held[index(root)]; }
    /// Whether the tag held in `of`'s category is at least `of`.
    [[nodiscard]] constexpr bool has(tag of) const { return at_least(in(root_of(of)), of); }
    /// Holds `of` in its category, replacing what was there.
    constexpr property_set& put(tag of)
    {
        held[index(root_of(of))] = of;
        return *this;
    }

    friend constexpr bool operator==(const property_set&, const property_set&) = default;
};

/// Whether `have` provides, in every category a container declares, at least what `want` asks for.
constexpr bool satisfies(const property_set& have, const property_set& want)
{
    for (const auto root : guaranteed_roots)
        if (!at_least(have.in(root), want.in(root)))
            return false;
    return true;
}

/// Whether a request that states `stated` makes every promise in `needed`, in the categories only a request states.
constexpr bool promises(const property_set& stated, const property_set& needed)
{
    for (const auto root : requested_roots)
        if (!at_least(stated.in(root), needed.in(root)))
            return false;
    return true;
}

/// Whether `declared` demands nothing `requested` left open. dense is the one guaranteed category whose declaration
/// also asks something of callers: a container that stores by index needs keys that are indices (its storage grows
/// with the largest key, and a negative key is no index at all), so a representation that declares dense serves only
/// a request that promises dense. Every other guarantee (stable_refs, concurrent, constant_clear, ...) costs the
/// container, never the caller: a request that leaves it open may receive it (the registry audit watches that it
/// does not pay for it needlessly).
constexpr bool demands_met(const property_set& declared, const property_set& requested) { return !declared.has(tag::dense) || requested.has(tag::dense); }

template<typename P>
concept a_tag = requires {
    typename P::root;
    { P::value } -> std::convertible_to<tag>;
};

}  // namespace detail

/// The properties stated by Tags, in any order, at most one per category; categories no tag answers keep their root.
template<typename... Tags>
struct properties
{
    static constexpr detail::property_set urd_properties = detail::property_set::of({ Tags::value... });
    static constexpr bool declared = true;
};
template<typename... Tags>
using properties_of = properties<Tags...>;

namespace detail
{

template<typename Traits, typename... Tags>
struct replaced
{
    static constexpr property_set urd_properties = []
    {
        auto result = Traits::urd_properties;
        (result.put(Tags::value), ...);
        return result;
    }();
    static constexpr bool declared = true;
};

}  // namespace detail

/// Traits with the categories of Tags replaced by them.
template<typename Traits, typename... Tags>
using with_t = detail::replaced<Traits, Tags...>;

/// Whether Tag answers a category a container declares, or one only a request has.
template<typename Tag>
inline constexpr bool is_guarantee = detail::in(detail::root_of(Tag::value), detail::guaranteed_roots);
template<typename Tag>
inline constexpr bool is_request_only = detail::in(detail::root_of(Tag::value), detail::requested_roots);

/// What a container guarantees, by itself, through every operation it offers. Specialize it for a container
/// to declare its properties, and check the declaration by adding the container to tests/properties.cpp:
///
///     template<> struct urd::container_traits<MyList> : urd::properties_of<urd::sorted, urd::shrinkable> {};
///
/// A container nobody declared guarantees nothing, and no Store resolves to it.
template<typename R>
struct container_traits : properties_of<>
{
    static constexpr bool declared = false;
};

namespace detail
{

/// A string literal as a template argument: a header's path, or the reason a denied operation is denied.
template<std::size_t N>
struct fixed_string
{
    std::array<char, N> text {};

    consteval fixed_string(const char (&literal)[N]) { std::ranges::copy(literal, text.begin()); }

    [[nodiscard]] constexpr const char* c_str() const noexcept { return text.data(); }
};

}  // namespace detail

// The registry declares representations and does not define them: a file pays for the definitions of the families
// it holds Stores of, by including urd/use/<family>.hpp. That a family is included is told by a marker that header
// defines, never by looking at a representation (completing a wrapper of an undefined part is an error, not a
// false); a Store whose family is absent names the header to include in its error (store.hpp).

/// Declared only; urd/use/<family>.hpp defines include<"urd/use/<family>.hpp">.
template<detail::fixed_string Header>
struct include;
/// Whether the header's marker is defined here.
template<detail::fixed_string Header>
inline constexpr bool included = requires { sizeof(include<Header>); };
/// The header that defines representation R, specialized beside R's declaration in its repr/X_fwd.hpp. A
/// representation that names none has no header; resolution_table --check rejects that for anything the registry
/// chooses, so every registered representation names its family.
template<typename R>
inline constexpr auto defined_in = detail::fixed_string("urd/use/<unnamed>.hpp");
/// The representations a wrapper holds, whose headers are needed too (Layered<R>: R; Scoped<Map>: Map; ...).
template<typename R>
struct parts
{
    using type = detail::type_list<>;
};

namespace detail
{

/// The elements of a row type, read without completing it: a Store's by its request (where it iterates: a pool or
/// queue has no elements to hand out), else what iterating a range gives. Undefined otherwise, so a constraint
/// naming it is simply not satisfied.
template<typename Row>
struct row_element
{
};
template<typename Row>
    requires(!is_store<Row>) && std::ranges::range<Row>
struct row_element<Row>
{
    using type = std::ranges::range_value_t<Row>;
};
template<typename Row>
    requires(is_store<Row> && container_traits<Row>::iterable)
struct row_element<Row>
{
    using type = typename container_traits<Row>::element_type;
};
template<typename Row>
using row_element_t = typename row_element<Row>::type;

// Three questions about an element type that a trait or rule may ask, answered for a Store from its request and
// never by completing it (a template argument evaluates every operand, so `!is_store<V> && std::trait<V>` would
// still complete V: these are specialized instead).

/// Whether V can fill a slot that is emptied and filled again: default-constructible and movable. Every Store is
/// movable, and one not asked to be nonempty or given a fixed shape has an empty value.
template<typename V>
inline constexpr bool slot_value = std::default_initializable<V> && std::movable<V>;
template<typename R, typename S>
inline constexpr bool slot_value<Store<R, S>> = !S::urd_properties.has(tag::nonempty) && !S::urd_properties.has(tag::fixed_shape);

/// Whether destroying a V does nothing. A Store owns storage, so never.
template<typename V>
inline constexpr bool trivially_destructible = std::is_trivially_destructible_v<V>;
template<typename R, typename S>
inline constexpr bool trivially_destructible<Store<R, S>> = false;

/// Whether a V is its own block of bytes (copied by memcpy). A Store owns storage, so never.
template<typename V>
inline constexpr bool trivially_copyable = std::is_trivially_copyable_v<V>;
template<typename R, typename S>
inline constexpr bool trivially_copyable<Store<R, S>> = false;

/// Whether V is iterated as a range of elements. A Store is where its request says so (a set, map or sequence).
template<typename V>
inline constexpr bool iterates = std::ranges::input_range<V>;
template<typename R, typename S>
inline constexpr bool iterates<Store<R, S>> = S::iterable;

}  // namespace detail

/// The promises representation R relies on, among those only a request makes (monotone_priority, small, …): it
/// serves only requests that make them. A bucket queue pops in order whatever is pushed, but is fast only where pushes
/// never go below the last popped. None by default.
template<typename R>
inline constexpr detail::property_set relies_on = detail::property_set::roots();

/// Whether map representation R orders its entries by their values too (order_by<P> with P(key, value)): its values
/// then change only through operations that put the entry back in order (update, insert_or_assign), never through a
/// reference. False by default.
template<typename R>
inline constexpr bool orders_by_value = false;

/// What a container guarantees about its elements where they are Stores: the properties a nested Store's rows
/// have as the container holds them. By default their own, except memory: destroying a row frees what it owns,
/// as erase and clear do. A container that keeps rows, or stores them flattened, specializes it. The primary
/// only marks that nothing was declared: the default is computed by the registry from the request's row type
/// (row_traits), so that selection reads nothing of a candidate.
template<typename R>
struct element_traits
{
    static constexpr bool defaulted = true;
};

namespace detail
{

template<typename X>
struct traits_of
{
    using type = container_traits<std::remove_cvref_t<X>>;
};
template<typename X>
    requires(!is_store<X>) && requires { X::urd_properties; }
struct traits_of<X>
{
    using type = X;
};

}  // namespace detail

/// The properties of X: X itself when it lists them (a request's spec), else its container_traits.
template<typename X>
using traits_t = typename detail::traits_of<X>::type;

/// Whether X (a container, a Store, or a request's spec) has every property in Tags.
template<typename X, typename... Tags>
    requires(detail::a_tag<Tags> && ...)
inline constexpr bool has = (traits_t<X>::urd_properties.has(Tags::value) && ...);

/// Whether Have provides, in every category a container declares, at least what Want asks for.
template<typename Have, typename Want>
inline constexpr bool satisfies = detail::satisfies(traits_t<Have>::urd_properties, traits_t<Want>::urd_properties);

}  // namespace urd
