#pragma once

// The vocabulary in words: tag_synopsis<T> for every tag (urd/tags.hpp), and the four groups the categories fall into
// (kind, semantics, costs, hints). share/urd/vocabulary.json and the docs' language pages are generated from them.

#include "urd/tags.hpp"

#include <array>
#include <string_view>
#include <tuple>

namespace urd
{

// Each tag's words, beside it: tag_synopsis<T>. A tag that is no root has its name, a description that follows its
// comment, and phrases, ways a need is put in plain words; a tag of semantics also says who keeps it (`promise`:
// "guaranteed", the container; "caller", the one who writes the request, about its data). A category's root has its
// name, its group, for semantics a reading heading, and the question its tags answer. A tag's role (kind, guarantee,
// demand, request, hint) is derived from these (urd::tag_role). share/urd/vocabulary.json is generated from them.
template<typename Tag>
struct tag_synopsis
{
};

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

template<>
struct tag_synopsis<any_order>
{
    static constexpr std::string_view name = "any_order";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "In which order is it iterated?";
};

template<>
struct tag_synopsis<sorted>
{
    static constexpr std::string_view name = "sorted";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Iterated in ascending order by urd::Less.";
    static constexpr std::array<std::string_view, 4> phrases { "sorted", "in order", "ordered by key", "range queries" };
};

template<>
struct tag_synopsis<insertion_ordered>
{
    static constexpr std::string_view name = "insertion_ordered";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Iterated in the order elements were first inserted.";
    static constexpr std::array<std::string_view, 3> phrases { "insertion order", "first come first listed", "keeps the order added" };
};

template<>
struct tag_synopsis<duplicates>
{
    static constexpr std::string_view name = "duplicates";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Duplicates allowed?";
};

template<>
struct tag_synopsis<unique>
{
    static constexpr std::string_view name = "unique";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "No two elements are equal.";
    static constexpr std::array<std::string_view, 3> phrases { "no duplicates", "distinct", "each at most once" };
};

template<>
struct tag_synopsis<any_keys>
{
    static constexpr std::string_view name = "any_keys";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Which keys?";
};

template<>
struct tag_synopsis<dense>
{
    static constexpr std::string_view name = "dense";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "The keys are 0..n-1, or nearly so: indices filling their range.";
    static constexpr std::array<std::string_view, 4> phrases { "ids", "indices", "vertices numbered 0..n-1", "small key range" };
};

template<>
struct tag_synopsis<sparse>
{
    static constexpr std::string_view name = "sparse";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "The keys are few compared to their range.";
    static constexpr std::array<std::string_view, 3> phrases { "few keys in a large range", "scattered keys", "arbitrary keys" };
};

template<>
struct tag_synopsis<moving_refs>
{
    static constexpr std::string_view name = "moving_refs";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Do references stay valid?";
};

template<>
struct tag_synopsis<stable_refs>
{
    static constexpr std::string_view name = "stable_refs";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "A reference to an element stays valid until that element is removed.";
    static constexpr std::array<std::string_view, 3> phrases { "pointers stay valid", "stable addresses", "references survive inserts" };
};

template<>
struct tag_synopsis<any_layout>
{
    static constexpr std::string_view name = "any_layout";
    using group = semantics_group;
    using heading = memory_heading;
    static constexpr std::string_view question = "One array?";
};

template<>
struct tag_synopsis<contiguous>
{
    static constexpr std::string_view name = "contiguous";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "The elements lie next to each other in memory.";
    static constexpr std::array<std::string_view, 4> phrases { "contiguous", "cache friendly", "as a span", "one array" };
};

template<>
struct tag_synopsis<any_representation>
{
    static constexpr std::string_view name = "any_representation";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Same contents, same bytes?";
};

template<>
struct tag_synopsis<canonical>
{
    static constexpr std::string_view name = "canonical";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Equal contents have equal representations.";
    static constexpr std::array<std::string_view, 3> phrases { "canonical", "same contents same bytes", "hashable by contents" };
};

template<>
struct tag_synopsis<single_threaded>
{
    static constexpr std::string_view name = "single_threaded";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Shared between threads?";
};

template<>
struct tag_synopsis<concurrent>
{
    static constexpr std::string_view name = "concurrent";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Threads share it: inserts from several threads at once, and the reads each kind allows beside them.";
    static constexpr std::array<std::string_view, 3> phrases { "thread safe", "shared between threads", "parallel inserts" };
};

template<>
struct tag_synopsis<any_memory>
{
    static constexpr std::string_view name = "any_memory";
    using group = costs_group;
    static constexpr std::string_view question = "What happens to freed memory?";
};

template<>
struct tag_synopsis<retains>
{
    static constexpr std::string_view name = "retains";
    static constexpr std::string_view description = "Storage freed by erase and clear is kept for reuse.";
    static constexpr std::array<std::string_view, 3> phrases { "no reallocation when refilled", "reuse memory", "keeps capacity" };
};

template<>
struct tag_synopsis<reclaims>
{
    static constexpr std::string_view name = "reclaims";
    static constexpr std::string_view description = "Erase and clear return the storage of what they remove.";
    static constexpr std::array<std::string_view, 3> phrases { "frees memory", "shrinks", "gives memory back" };
};

template<>
struct tag_synopsis<any_clear>
{
    static constexpr std::string_view name = "any_clear";
    using group = costs_group;
    static constexpr std::string_view question = "How is it cleared?";
};

template<>
struct tag_synopsis<proportional_clear>
{
    static constexpr std::string_view name = "proportional_clear";
    static constexpr std::string_view description = "Clear takes time in proportion to the elements present.";
    static constexpr std::array<std::string_view, 2> phrases { "cheap clear", "cleared often" };
};

template<>
struct tag_synopsis<constant_clear>
{
    static constexpr std::string_view name = "constant_clear";
    static constexpr std::string_view description = "Clear takes constant time, as with a generation counter.";
    static constexpr std::array<std::string_view, 3> phrases { "reset every round", "cleared per search", "O(1) clear" };
};

template<>
struct tag_synopsis<any_access>
{
    static constexpr std::string_view name = "any_access";
    using group = kind_group;
    static constexpr std::string_view question = "How are elements taken out?";
};

template<>
struct tag_synopsis<fifo>
{
    static constexpr std::string_view name = "fifo";
    static constexpr std::string_view description = "A queue: elements leave in the order they came.";
    static constexpr std::array<std::string_view, 4> phrases { "queue", "fifo", "breadth first", "worklist" };
};

template<>
struct tag_synopsis<lifo>
{
    static constexpr std::string_view name = "lifo";
    static constexpr std::string_view description = "A stack: the last element pushed leaves first.";
    static constexpr std::array<std::string_view, 3> phrases { "stack", "lifo", "depth first" };
};

template<>
struct tag_synopsis<by_priority>
{
    static constexpr std::string_view name = "by_priority";
    static constexpr std::string_view description = "A priority queue: the least element leaves first.";
    static constexpr std::array<std::string_view, 4> phrases { "priority queue", "heap", "best first", "open list" };
};

template<>
struct tag_synopsis<any_scope>
{
    static constexpr std::string_view name = "any_scope";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Nested scopes?";
};

template<>
struct tag_synopsis<scoped>
{
    static constexpr std::string_view name = "scoped";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "push_scope and pop_scope: popping undoes every change since the matching push.";
    static constexpr std::array<std::string_view, 3> phrases { "nested scopes", "undo on backtrack", "symbol table" };
};

template<>
struct tag_synopsis<any_layering>
{
    static constexpr std::string_view name = "any_layering";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Layered over a parent?";
};

template<>
struct tag_synopsis<layered>
{
    static constexpr std::string_view name = "layered";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "layer_over(parent): lookups find the parent's elements too, which it neither owns nor changes.";
    static constexpr std::array<std::string_view, 3> phrases { "falls through to a parent", "overlay", "phase over phase" };
};

template<>
struct tag_synopsis<any_handle>
{
    static constexpr std::string_view name = "any_handle";
    using group = kind_group;
    static constexpr std::string_view question = "Reached by handle?";
};

template<>
struct tag_synopsis<handles>
{
    static constexpr std::string_view name = "handles";
    static constexpr std::string_view description = "A pool: insert gives a handle; with unique, equal elements get equal handles.";
    static constexpr std::array<std::string_view, 4> phrases { "interning", "handles", "deduplicated rows", "hash consing" };
};

template<>
struct tag_synopsis<any_sharing>
{
    static constexpr std::string_view name = "any_sharing";
    using group = costs_group;
    static constexpr std::string_view question = "Rows share structure?";
};

template<>
struct tag_synopsis<structural_sharing>
{
    static constexpr std::string_view name = "structural_sharing";
    static constexpr std::string_view description = "Parts that are equal across rows are stored once.";
    static constexpr std::array<std::string_view, 3> phrases { "rows differing in a few places", "persistent", "shared structure" };
};

template<>
struct tag_synopsis<any_direction>
{
    static constexpr std::string_view name = "any_direction";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Looked up both ways?";
};

template<>
struct tag_synopsis<bidirectional>
{
    static constexpr std::string_view name = "bidirectional";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "A map that answers by value too: keys(v) gives the keys that map to v.";
    static constexpr std::array<std::string_view, 3> phrases { "predecessors", "reverse edges", "inverse map" };
};

template<>
struct tag_synopsis<any_shape>
{
    static constexpr std::string_view name = "any_shape";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Rows of a fixed shape?";
};

template<>
struct tag_synopsis<fixed_shape>
{
    static constexpr std::string_view name = "fixed_shape";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Rows whose number and lengths are set at construction; only their contents change.";
    static constexpr std::array<std::string_view, 3> phrases { "fixed rows", "matrix", "table of known shape" };
};

template<>
struct tag_synopsis<any_packing>
{
    static constexpr std::string_view name = "any_packing";
    using group = semantics_group;
    using heading = memory_heading;
    static constexpr std::string_view question = "Records packed flat?";
};

template<>
struct tag_synopsis<flat>
{
    static constexpr std::string_view name = "flat";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Each record is one self-contained block of bytes, frozen once stored and read by index.";
    static constexpr std::array<std::string_view, 3> phrases { "flat records", "serialized", "one block per record" };
};

template<>
struct tag_synopsis<any_grouping>
{
    static constexpr std::string_view name = "any_grouping";
    using group = kind_group;
    static constexpr std::string_view question = "Groups of keys?";
};

template<>
struct tag_synopsis<partition>
{
    static constexpr std::string_view name = "partition";
    static constexpr std::string_view description = "A partition: elements fall into disjoint classes that merge joins.";
    static constexpr std::array<std::string_view, 3> phrases { "union find", "disjoint sets", "equivalence classes" };
};

template<>
struct tag_synopsis<any_tracking>
{
    static constexpr std::string_view name = "any_tracking";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Tracks what changed?";
};

template<>
struct tag_synopsis<change_log>
{
    static constexpr std::string_view name = "change_log";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "added() and removed(): the elements changed since clear_changes().";
    static constexpr std::array<std::string_view, 3> phrases { "delta", "what changed", "semi-naive evaluation" };
};

template<>
struct tag_synopsis<immutable>
{
    static constexpr std::string_view name = "immutable";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Grows or shrinks? (follows from monotone and frozen)";
};

template<>
struct tag_synopsis<growable>
{
    static constexpr std::string_view name = "growable";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Offers insert.";
    static constexpr std::array<std::string_view, 1> phrases { "grows" };
};

template<>
struct tag_synopsis<shrinkable>
{
    static constexpr std::string_view name = "shrinkable";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Offers insert, erase and clear.";
    static constexpr std::array<std::string_view, 1> phrases { "add and remove" };
};

template<>
struct tag_synopsis<may_change>
{
    static constexpr std::string_view name = "may_change";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Does it change after building?";
};

template<>
struct tag_synopsis<monotone>
{
    static constexpr std::string_view name = "monotone";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Only grows: nothing is ever removed.";
    static constexpr std::array<std::string_view, 3> phrases { "append only", "only grows", "never erased" };
};

template<>
struct tag_synopsis<frozen>
{
    static constexpr std::string_view name = "frozen";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "No change after construction; assign rebuilds it whole.";
    static constexpr std::array<std::string_view, 3> phrases { "built once", "read only", "immutable after building" };
};

template<>
struct tag_synopsis<maybe_empty>
{
    static constexpr std::string_view name = "maybe_empty";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Can it be empty?";
};

template<>
struct tag_synopsis<nonempty>
{
    static constexpr std::string_view name = "nonempty";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "At least one element.";
    static constexpr std::array<std::string_view, 2> phrases { "never empty", "nonempty" };
};

template<>
struct tag_synopsis<any_multiplicity>
{
    static constexpr std::string_view name = "any_multiplicity";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Values per key?";
};

template<>
struct tag_synopsis<functional>
{
    static constexpr std::string_view name = "functional";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "At most one value per key: a map.";
    static constexpr std::array<std::string_view, 3> phrases { "map", "one value per key", "lookup table" };
};

template<>
struct tag_synopsis<multi>
{
    static constexpr std::string_view name = "multi";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Any number of values per key: a multimap.";
    static constexpr std::array<std::string_view, 3> phrases { "multimap", "many values per key", "adjacency lists" };
};

template<>
struct tag_synopsis<by_key>
{
    static constexpr std::string_view name = "by_key";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Reached by position?";
};

template<>
struct tag_synopsis<positional>
{
    static constexpr std::string_view name = "positional";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Identified by position 0..n-1, in order.";
    static constexpr std::array<std::string_view, 3> phrases { "by position", "numbered as inserted", "interner" };
};

template<>
struct tag_synopsis<general>
{
    static constexpr std::string_view name = "general";
    using group = hints_group;
    static constexpr std::string_view question = "How many elements?";
};

template<>
struct tag_synopsis<small>
{
    static constexpr std::string_view name = "small";
    static constexpr std::string_view description = "A few elements, or a few at a time: compact containers first.";
    static constexpr std::array<std::string_view, 3> phrases { "few elements", "small", "tiny" };
};

template<>
struct tag_synopsis<any_priority>
{
    static constexpr std::string_view name = "any_priority";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "How do priorities move?";
};

template<>
struct tag_synopsis<monotone_priority>
{
    static constexpr std::string_view name = "monotone_priority";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "A priority queue never pushed below the last popped.";
    static constexpr std::array<std::string_view, 3> phrases { "dijkstra", "non-negative lengths", "event simulation" };
};

}  // namespace urd
