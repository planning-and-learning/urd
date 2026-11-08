#pragma once

// The vocabulary as code: each category is an enum of its own, none first (urd::order { none, sorted,
// insertion_ordered }), with a tag type per answer (urd::sorted, ...) listed by tags_of<E>, and its words beside them.
// A tag type derived from another means "at least that". A category's none type names it: its question, its group
// (kind, semantics, costs, hints), and in semantics its reading heading; any other tag has its description, the phrases
// a need is put in, and in semantics who keeps it (`promise`: guaranteed or caller). Categories are open: another enum
// with tag types is another category, and a kind lists the categories its requests answer (urd/kinds.hpp, format).
// The machinery that computes with them is in urd/property_set.hpp; share/urd/vocabulary.json is generated from them.

#include <boost/mp11.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>

namespace urd
{

/// The heading of a category outside semantics: none.
struct no_heading
{
    static constexpr std::string_view id = "";
    static constexpr std::string_view title = "";
};

/// What an answer says of a Store, each false unless the tag says so (`static constexpr answer_says says { .x = true }`):
/// demanded (a container that has it needs the request to promise it); whether it stops the Store growing, shrinking
/// or changing an element in place; whether a sequence it holds changes at its end only (no insert before, or erase
/// of, a position: that would move the elements after); whether the Store must always hold an element; whether it fixes the order the
/// Store is iterated in or hashed in; and whether giving it costs something (storage by index, nodes, locks,
/// generations, a log, a parent, shared parts), so that a container that gives it unasked is a surprise, unlike one
/// that also happens to keep its memory or lie contiguous. A tag derived from another restates what it keeps of it.
struct answer_says
{
    bool demanded = false;
    bool stops_growth = false;
    bool stops_shrinking = false;
    bool stops_clearing = false;  ///< Stops clear() emptying the Store at once (shrinking stopped, clear() may stay).
    bool stops_changes = false;
    bool changes_at_ends = false;
    bool needs_an_element = false;
    bool built_with_width = false;  ///< Built with the bit width its elements fit in (urd::bit_width).
    bool built_with_column_widths = false;  ///< Built with the column widths its rows are stored by (urd::column_widths).
    bool fixes_iteration_order = false;
    bool fixes_hash_order = false;
    bool costly = false;
};

/// What every tag says unless it says otherwise: every category's root derives from it, and its answers from the root,
/// so each piece of a tag's data is always there (read by urd/property_set.hpp, table_of_tags). How its category is
/// matched: exact (a container holds what the request holds, no more), request_only (no container declares it). What
/// it means for a Store, as lists of tag types: what it implies (frozen: stable_refs), what of that the Store keeps
/// itself and asks of no container (frozen: stable_refs), what it promises its container (nonempty: monotone), and
/// which containers come after the others (holding these: frozen and small put stable_refs later). What else it says
/// of a Store (says), and its category's reading heading (none outside semantics).
struct tag_defaults
{
    static constexpr bool exact = false;
    static constexpr bool request_only = false;
    using implies = boost::mp11::mp_list<>;
    using kept_by_store = boost::mp11::mp_list<>;
    using promises = boost::mp11::mp_list<>;
    using later = boost::mp11::mp_list<>;
    static constexpr answer_says says {};
    using heading = no_heading;
};

// The groups, by what leaving out a tag one needs causes. Every category is in exactly one.
struct kind_group
{
    static constexpr std::string_view id = "kind";
    static constexpr std::string_view title = "Kind";
    static constexpr std::string_view test = "Leaving it out does not compile: the operation does not exist.";
    static constexpr std::string_view summary = "What the Store is: its kind, named first (a set, a map, a sequence, …), and the tags that say how it is "
                                                "reached: the first decision, which every other changes with.";
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

/// The tag types of a category's enum E: its none, then its answers, in the order of E (each category's, beside its
/// tags below).
template<typename E>
struct tags_of;

/// Whether E is a category's enum.
template<typename E>
concept category_enum = requires { typename tags_of<E>::type; };

// The categories of properties, each a question a request may answer (its words: the category's root type below).
// Each is an enum of its own whose first value, none, is no answer: what a category left unstated means. A category
// is open: another enum with tag types (tags_of) is another category. Each category below is its enum, its tag types
// and its tags_of, together.
//
// Properties, in the style of Boost.Graph's graph_traits: each answers one question (a category) with a tag
// type, and a tag derived from another means "at least that". A category's none type promises nothing, and names
// itself as `root`, which every tag of its category inherits. Each tag type's `value` is its answer in the category's
// enum, from which urd::tag is made (its category's table and its answer), so that properties are computed as
// constexpr values, not as types: a request costs the compiler a constant evaluation instead of hundreds of class
// instantiations.

// What a container guarantees by itself.
enum class order : std::uint8_t
{
    none,
    sorted,
    insertion_ordered,
};
struct any_order : tag_defaults
{
    using root = any_order;
    static constexpr auto value = order::none;
    static constexpr std::string_view name = "any_order";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "In which order is it iterated?";
};
/// Ascending by urd::Less: operator< for numbers, member by member for identified types.
struct sorted : any_order
{
    static constexpr auto value = order::sorted;
    /// Its contents fix the order it is iterated and hashed in.
    static constexpr std::string_view name = "sorted";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Iterated in ascending order by urd::Less.";
    static constexpr std::array<std::string_view, 4> phrases { "sorted", "in order", "ordered by key", "range queries" };
    static constexpr answer_says says { .fixes_iteration_order = true, .fixes_hash_order = true };
};
/// In the order they were inserted: a new element goes last, and the others keep their places.
struct insertion_ordered : any_order
{
    static constexpr auto value = order::insertion_ordered;
    /// It is iterated in the order its elements came.
    static constexpr std::string_view name = "insertion_ordered";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Iterated in the order elements were first inserted.";
    static constexpr std::array<std::string_view, 3> phrases { "insertion order", "first come first listed", "keeps the order added" };
    static constexpr answer_says says { .fixes_iteration_order = true };
};

template<>
struct tags_of<order>
{
    using type = boost::mp11::mp_list<any_order, sorted, insertion_ordered>;
};

enum class uniqueness : std::uint8_t
{
    none,
    unique,
};

struct duplicates : tag_defaults
{
    using root = duplicates;
    static constexpr auto value = uniqueness::none;
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
    static constexpr auto value = uniqueness::unique;
    static constexpr std::string_view name = "unique";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "No two elements are equal.";
    static constexpr std::array<std::string_view, 3> phrases { "no duplicates", "distinct", "each at most once" };
};

template<>
struct tags_of<uniqueness>
{
    using type = boost::mp11::mp_list<duplicates, unique>;
};

enum class keys : std::uint8_t
{
    none,
    dense,
    sparse,
};

struct any_keys : tag_defaults
{
    using root = any_keys;
    static constexpr auto value = keys::none;
    static constexpr std::string_view name = "any_keys";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Which keys?";
};
/// The keys are 0..n-1, or nearly so.
struct dense : any_keys
{
    static constexpr auto value = keys::dense;
    static constexpr std::string_view name = "dense";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "The keys are 0..n-1, or nearly so: indices filling their range.";
    static constexpr std::array<std::string_view, 4> phrases { "ids", "indices", "vertices numbered 0..n-1", "small key range" };
    /// Demanded: a container that keeps it stores by index, so it needs keys that are indices, which the request must
    /// promise (its storage grows with the largest key, and a negative key is no index at all).
    static constexpr answer_says says { .demanded = true, .costly = true };
};
/// The keys are few compared to their range.
struct sparse : any_keys
{
    static constexpr auto value = keys::sparse;
    static constexpr std::string_view name = "sparse";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "The keys are few compared to their range.";
    static constexpr std::array<std::string_view, 3> phrases { "few keys in a large range", "scattered keys", "arbitrary keys" };
};

template<>
struct tags_of<keys>
{
    using type = boost::mp11::mp_list<any_keys, dense, sparse>;
};

enum class referencing : std::uint8_t
{
    none,
    stable_refs,
};

struct moving_refs : tag_defaults
{
    using root = moving_refs;
    static constexpr auto value = referencing::none;
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
    static constexpr auto value = referencing::stable_refs;
    static constexpr std::string_view name = "stable_refs";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "A reference to an element stays valid until that element is removed.";
    static constexpr std::array<std::string_view, 3> phrases { "pointers stay valid", "stable addresses", "references survive inserts" };
    /// A sequence with stable references removes at its end only: erasing a position would move the elements after.
    static constexpr answer_says says { .changes_at_ends = true, .costly = true };
};

template<>
struct tags_of<referencing>
{
    using type = boost::mp11::mp_list<moving_refs, stable_refs>;
};

enum class layout : std::uint8_t
{
    none,
    contiguous,
};

struct any_layout : tag_defaults
{
    using root = any_layout;
    static constexpr auto value = layout::none;
    static constexpr std::string_view name = "any_layout";
    using group = semantics_group;
    using heading = memory_heading;
    static constexpr std::string_view question = "One array?";
};
/// The elements lie next to each other in memory.
struct contiguous : any_layout
{
    static constexpr auto value = layout::contiguous;
    static constexpr std::string_view name = "contiguous";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "The elements lie next to each other in memory.";
    static constexpr std::array<std::string_view, 4> phrases { "contiguous", "cache friendly", "as a span", "one array" };
};

template<>
struct tags_of<layout>
{
    using type = boost::mp11::mp_list<any_layout, contiguous>;
};

enum class representation : std::uint8_t
{
    none,
    canonical,
};

struct any_representation : tag_defaults
{
    using root = any_representation;
    static constexpr auto value = representation::none;
    static constexpr std::string_view name = "any_representation";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Same contents, same bytes?";
};
/// Equal contents have equal representations.
struct canonical : any_representation
{
    static constexpr auto value = representation::canonical;
    /// Equal contents are equal bytes, so its order is fixed by its contents for hashing.
    static constexpr std::string_view name = "canonical";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Equal contents have equal representations.";
    static constexpr std::array<std::string_view, 3> phrases { "canonical", "same contents same bytes", "hashable by contents" };
    static constexpr answer_says says { .fixes_hash_order = true };
};

template<>
struct tags_of<representation>
{
    using type = boost::mp11::mp_list<any_representation, canonical>;
};

enum class threading : std::uint8_t
{
    none,
    concurrent,
};

struct single_threaded : tag_defaults
{
    using root = single_threaded;
    static constexpr auto value = threading::none;
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
    static constexpr auto value = threading::concurrent;
    static constexpr std::string_view name = "concurrent";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Threads share it: inserts from several threads at once, and the reads each kind allows beside them.";
    static constexpr std::array<std::string_view, 3> phrases { "thread safe", "shared between threads", "parallel inserts" };
    static constexpr answer_says says { .costly = true };
};

template<>
struct tags_of<threading>
{
    using type = boost::mp11::mp_list<single_threaded, concurrent>;
};

enum class memory : std::uint8_t
{
    none,
    retains,
    reclaims,
};

struct any_memory : tag_defaults
{
    using root = any_memory;
    static constexpr auto value = memory::none;
    static constexpr std::string_view name = "any_memory";
    using group = costs_group;
    static constexpr std::string_view question = "What happens to freed memory?";
};
/// Storage that erase and clear free is kept for reuse: refilling up to an earlier size allocates nothing.
struct retains : any_memory
{
    static constexpr auto value = memory::retains;
    static constexpr std::string_view name = "retains";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Storage freed by erase and clear is kept for reuse.";
    static constexpr std::array<std::string_view, 3> phrases { "no reallocation when refilled", "reuse memory", "keeps capacity" };
};
/// Erase and clear return the storage of what they remove.
struct reclaims : any_memory
{
    static constexpr auto value = memory::reclaims;
    static constexpr std::string_view name = "reclaims";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Erase and clear return the storage of what they remove.";
    static constexpr std::array<std::string_view, 3> phrases { "frees memory", "shrinks", "gives memory back" };
};

template<>
struct tags_of<memory>
{
    using type = boost::mp11::mp_list<any_memory, retains, reclaims>;
};

enum class clearing : std::uint8_t
{
    none,
    proportional_clear,
    constant_clear,
};

struct any_clear : tag_defaults
{
    using root = any_clear;
    static constexpr auto value = clearing::none;
    static constexpr std::string_view name = "any_clear";
    using group = costs_group;
    static constexpr std::string_view question = "How is it cleared?";
};
/// Clear takes time in proportion to the elements present, not to the keys or capacity seen before.
struct proportional_clear : any_clear
{
    static constexpr auto value = clearing::proportional_clear;
    static constexpr std::string_view name = "proportional_clear";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Clear takes time in proportion to the elements present.";
    static constexpr std::array<std::string_view, 2> phrases { "cheap clear", "cleared often" };
};
/// Clear takes constant time, as with a generation counter.
struct constant_clear : proportional_clear
{
    static constexpr auto value = clearing::constant_clear;
    static constexpr std::string_view name = "constant_clear";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Clear takes constant time, as with a generation counter.";
    static constexpr std::array<std::string_view, 3> phrases { "reset every round", "cleared per search", "O(1) clear" };
    static constexpr answer_says says { .costly = true };
};

template<>
struct tags_of<clearing>
{
    using type = boost::mp11::mp_list<any_clear, proportional_clear, constant_clear>;
};

enum class access : std::uint8_t
{
    none,
    fifo,
    lifo,
    by_priority,
};

struct any_access : tag_defaults
{
    using root = any_access;
    static constexpr auto value = access::none;
    static constexpr std::string_view name = "any_access";
    using group = kind_group;
    static constexpr std::string_view question = "How are elements taken out?";
};
/// A queue: elements leave in the order they came (front, push, pop).
struct fifo : any_access
{
    static constexpr auto value = access::fifo;
    static constexpr std::string_view name = "fifo";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A queue: elements leave in the order they came.";
    static constexpr std::array<std::string_view, 4> phrases { "queue", "fifo", "breadth first", "worklist" };
};
/// A stack: the last element pushed leaves first (top, push, pop).
struct lifo : any_access
{
    static constexpr auto value = access::lifo;
    static constexpr std::string_view name = "lifo";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A stack: the last element pushed leaves first.";
    static constexpr std::array<std::string_view, 3> phrases { "stack", "lifo", "depth first" };
};
/// A priority queue: the least element by urd::Less leaves first (top, push, pop).
struct by_priority : any_access
{
    static constexpr auto value = access::by_priority;
    static constexpr std::string_view name = "by_priority";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A priority queue: the least element leaves first.";
    static constexpr std::array<std::string_view, 4> phrases { "priority queue", "heap", "best first", "open list" };
};

template<>
struct tags_of<access>
{
    using type = boost::mp11::mp_list<any_access, fifo, lifo, by_priority>;
};

enum class scoping : std::uint8_t
{
    none,
    scoped,
};

struct any_scope : tag_defaults
{
    using root = any_scope;
    static constexpr auto value = scoping::none;
    static constexpr std::string_view name = "any_scope";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Nested scopes?";
};
/// Offers push_scope and pop_scope: popping undoes every change since the matching push.
struct scoped : any_scope
{
    static constexpr auto value = scoping::scoped;
    static constexpr std::string_view name = "scoped";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "push_scope and pop_scope: popping undoes every change since the matching push.";
    static constexpr std::array<std::string_view, 3> phrases { "nested scopes", "undo on backtrack", "symbol table" };
    static constexpr answer_says says { .costly = true };
};

template<>
struct tags_of<scoping>
{
    using type = boost::mp11::mp_list<any_scope, scoped>;
};

enum class layering : std::uint8_t
{
    none,
    layered,
};

struct any_layering : tag_defaults
{
    using root = any_layering;
    static constexpr auto value = layering::none;
    static constexpr std::string_view name = "any_layering";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Layered over a parent?";
};
/// Offers layer_over(parent): lookups find the parent's elements too, which it neither owns nor changes.
struct layered : any_layering
{
    static constexpr auto value = layering::layered;
    static constexpr std::string_view name = "layered";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "layer_over(parent): lookups find the parent's elements too, which it neither owns nor changes.";
    static constexpr std::array<std::string_view, 3> phrases { "falls through to a parent", "overlay", "phase over phase" };
    static constexpr answer_says says { .costly = true };
};

template<>
struct tags_of<layering>
{
    using type = boost::mp11::mp_list<any_layering, layered>;
};

enum class handling : std::uint8_t
{
    none,
    handles,
};

struct any_handle : tag_defaults
{
    using root = any_handle;
    static constexpr auto value = handling::none;
    static constexpr std::string_view name = "any_handle";
    using group = kind_group;
    static constexpr std::string_view question = "Reached by handle?";
    /// Exact: a container must hold what the request holds here, no more (handles make a pool, or give a priority queue update and erase by handle).
    static constexpr bool exact = true;
};
/// A pool: each element is identified by a handle that insert gives, with whether it was new, and equal elements get
/// equal handles. Not iterated; counted, with handles 0..size()-1, unless it has structural_sharing (handles name
/// shared nodes there).
struct handles : any_handle
{
    static constexpr auto value = handling::handles;
    static constexpr std::string_view name = "handles";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A pool: insert gives a handle; with unique, equal elements get equal handles.";
    static constexpr std::array<std::string_view, 4> phrases { "interning", "handles", "deduplicated rows", "hash consing" };
    static constexpr answer_says says { .costly = true };
};

template<>
struct tags_of<handling>
{
    using type = boost::mp11::mp_list<any_handle, handles>;
};

enum class sharing : std::uint8_t
{
    none,
    structural_sharing,
};

struct any_sharing : tag_defaults
{
    using root = any_sharing;
    static constexpr auto value = sharing::none;
    static constexpr std::string_view name = "any_sharing";
    using group = costs_group;
    static constexpr std::string_view question = "Rows share structure?";
};
/// Parts that are equal across elements (rows) are stored once.
struct structural_sharing : any_sharing
{
    static constexpr auto value = sharing::structural_sharing;
    static constexpr std::string_view name = "structural_sharing";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Parts that are equal across rows are stored once.";
    static constexpr std::array<std::string_view, 3> phrases { "rows differing in a few places", "persistent", "shared structure" };
    static constexpr answer_says says { .costly = true };
};

template<>
struct tags_of<sharing>
{
    using type = boost::mp11::mp_list<any_sharing, structural_sharing>;
};

enum class direction : std::uint8_t
{
    none,
    bidirectional,
};

struct any_direction : tag_defaults
{
    using root = any_direction;
    static constexpr auto value = direction::none;
    static constexpr std::string_view name = "any_direction";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Looked up both ways?";
};
/// A map that answers by value too: keys(v) gives the keys that map to v, as a graph gives a vertex's predecessors.
struct bidirectional : any_direction
{
    static constexpr auto value = direction::bidirectional;
    static constexpr std::string_view name = "bidirectional";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "A map that answers by value too: keys(v) gives the keys that map to v.";
    static constexpr std::array<std::string_view, 3> phrases { "predecessors", "reverse edges", "inverse map" };
    static constexpr answer_says says { .costly = true };
};

template<>
struct tags_of<direction>
{
    using type = boost::mp11::mp_list<any_direction, bidirectional>;
};

enum class shape : std::uint8_t
{
    none,
    fixed_shape,
};

struct any_shape : tag_defaults
{
    using root = any_shape;
    static constexpr auto value = shape::none;
    static constexpr std::string_view name = "any_shape";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Rows of a fixed shape?";
};
/// Rows of a fixed shape: how many there are and how long each is is set at construction (urd::row_lengths); only
/// their contents change.
struct fixed_shape : any_shape
{
    static constexpr auto value = shape::fixed_shape;
    /// Its rows keep their lengths: nothing is inserted or erased.
    static constexpr std::string_view name = "fixed_shape";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Rows whose number and lengths are set at construction; only their contents change.";
    static constexpr std::array<std::string_view, 3> phrases { "fixed rows", "matrix", "table of known shape" };
    static constexpr answer_says says { .stops_growth = true, .costly = true };
};

template<>
struct tags_of<shape>
{
    using type = boost::mp11::mp_list<any_shape, fixed_shape>;
};

enum class packing : std::uint8_t
{
    none,
    flat,
};

struct any_packing : tag_defaults
{
    using root = any_packing;
    static constexpr auto value = packing::none;
    static constexpr std::string_view name = "any_packing";
    using group = semantics_group;
    using heading = memory_heading;
    static constexpr std::string_view question = "Records packed flat?";
};
/// Each element is one self-contained block of bytes, with no part of its own on the heap: trivially copyable, or
/// serialized as a whole. Such elements are frozen once stored, and are read by index.
struct flat : any_packing
{
    static constexpr auto value = packing::flat;
    static constexpr std::string_view name = "flat";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Each record is one self-contained block of bytes, frozen once stored and read by index.";
    static constexpr std::array<std::string_view, 3> phrases { "flat records", "serialized", "one block per record" };
};

template<>
struct tags_of<packing>
{
    using type = boost::mp11::mp_list<any_packing, flat>;
};

enum class width : std::uint8_t
{
    word,
    bit,
    by_column_widths,
};

/// Each element whole, stored as its type: what a Store holds unless a request promises more (the default).
struct word : tag_defaults
{
    using root = word;
    static constexpr bool request_only = true;  ///< no container declares it: only a request states it
    static constexpr auto value = width::word;
    static constexpr std::string_view name = "word";
    using group = semantics_group;
    using heading = memory_heading;
    static constexpr std::string_view question = "Elements stored whole, or in fewer bits?";
};
/// Every element fits in the bit width given at construction (urd::bit_width): a representation may store each in
/// that many bits, and the Store hands out values, not references, to them. Every word representation still serves
/// the request, not using the promise.
struct bit : word
{
    static constexpr auto value = width::bit;
    static constexpr std::string_view name = "bit";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "Every element fits in the bit width given at construction (urd::bit_width).";
    static constexpr std::array<std::string_view, 3> phrases { "bit packed", "packed integers", "few bits per element" };
    static constexpr answer_says says { .built_with_width = true };
};

/// Rows are stored by the column widths given at construction (urd::column_widths, urd/repr/containers/schema_rows.hpp):
/// unsigned columns, each in the bits its width gives it. The Store hands out rows as values that read their columns.
struct by_column_widths : word
{
    static constexpr auto value = width::by_column_widths;
    static constexpr std::string_view name = "by_column_widths";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "Rows are stored by the column widths given at construction (urd::column_widths).";
    static constexpr std::array<std::string_view, 3> phrases { "column widths", "unsigned columns", "columns in given bits" };
    static constexpr answer_says says { .built_with_column_widths = true };
};

template<>
struct tags_of<width>
{
    using type = boost::mp11::mp_list<word, bit, by_column_widths>;
};

/// What a Store of urd::by_column_widths is built with (urd/repr/containers/schema_rows.hpp).
class column_widths;

/// The bit width a Store of urd::bit is built with: every element fits in `bits` bits.
struct bit_width
{
    std::uint8_t bits = 1;

    /// The width that holds every value below `bound` (at least one bit).
    [[nodiscard]] static constexpr bit_width for_bound(std::uint64_t bound) noexcept
    {
        auto bits = std::uint8_t { 1 };
        while (bits < 64 && (std::uint64_t { 1 } << bits) < bound)
            ++bits;
        return bit_width { bits };
    }
    friend constexpr bool operator==(bit_width, bit_width) noexcept = default;
};

enum class grouping : std::uint8_t
{
    none,
    partition,
};

struct any_grouping : tag_defaults
{
    using root = any_grouping;
    static constexpr auto value = grouping::none;
    static constexpr std::string_view name = "any_grouping";
    using group = kind_group;
    static constexpr std::string_view question = "Groups of keys?";
};
/// A partition: its elements fall into disjoint classes; merge joins two classes, find names an element's class. What
/// a partition<K> means (urd/kinds.hpp), not stated.
struct partitioned : any_grouping
{
    static constexpr auto value = grouping::partition;
    static constexpr std::string_view name = "partition";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A partition: elements fall into disjoint classes that merge joins.";
    static constexpr std::array<std::string_view, 3> phrases { "union find", "disjoint sets", "equivalence classes" };
    static constexpr answer_says says { .costly = true };
};

template<>
struct tags_of<grouping>
{
    using type = boost::mp11::mp_list<any_grouping, partitioned>;
};

enum class keying : std::uint8_t
{
    none,
    keyed,
};

struct any_keying : tag_defaults
{
    using root = any_keying;
    static constexpr auto value = keying::none;
    static constexpr std::string_view name = "any_keying";
    using group = kind_group;
    static constexpr std::string_view question = "Freed slots kept by a key?";
};
/// Slots acquired by a key (keyed<K>): acquire_shared(key, args...) reuses a slot freed under an equal key, else makes
/// one from (key, args...); the slot keeps its key. The words of the answer.
struct keyed_slots : any_keying
{
    static constexpr auto value = keying::keyed;
    static constexpr std::string_view name = "keyed";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Slots acquired by a key: a freed slot is reused only under an equal key.";
    static constexpr std::array<std::string_view, 3> phrases { "pooled by shape", "reuse by key", "free lists per key" };
};

template<>
struct tags_of<keying>
{
    using type = boost::mp11::mp_list<any_keying, keyed_slots>;
};

/// Keys a Store of slots<T> by K (urd::Hash, urd::EqualTo): a slot freed under one key is reused only for an equal
/// key, and T is made from (key, args...). A tag like any other, answering `keyed`, that also carries K.
template<typename K>
struct keyed : keyed_slots
{
    using key_type = K;
};

// What a Store's interface enforces: no container declares a lifecycle or emptiness; one that cannot erase relies on
// monotone, one that cannot insert on frozen. monotone_until_clear removes by clear() only. Multiplicity a multimap declares (multi), and a request states it.
enum class change : std::uint8_t
{
    none,
    monotone_until_clear,
    monotone,
    frozen,
};
struct may_change : tag_defaults
{
    using root = may_change;
    static constexpr bool request_only = true;  ///< no container declares it: only a request states it
    static constexpr auto value = change::none;
    static constexpr std::string_view name = "may_change";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Does it change after building?";
};
/// Only grows until cleared: elements are only added; the only removal is clear(), which empties the Store at once.
struct monotone_until_clear : may_change
{
    static constexpr auto value = change::monotone_until_clear;
    /// Nothing is erased; clear() empties it whole (with retains, refilling allocates nothing).
    static constexpr std::string_view name = "monotone_until_clear";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Only grows until cleared: elements are only added; clear() empties it at once.";
    static constexpr std::array<std::string_view, 3> phrases { "append only until cleared", "cleared, never erased", "refilled after clear" };
    static constexpr answer_says says { .stops_shrinking = true };
};
/// Only grows: nothing is ever removed, not even by clear().
struct monotone : monotone_until_clear
{
    static constexpr auto value = change::monotone;
    /// Nothing is removed.
    static constexpr std::string_view name = "monotone";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Only grows: nothing is ever removed.";
    static constexpr std::array<std::string_view, 3> phrases { "append only", "only grows", "never erased" };
    static constexpr answer_says says { .stops_shrinking = true, .stops_clearing = true };
};
/// No change after construction (assign rebuilds it whole): any number of threads may read it between rebuilds,
/// so concurrent, which is about inserts from threads at once, is not asked of it.
struct frozen : monotone
{
    static constexpr auto value = change::frozen;
    /// Nothing changes, so nothing moves: it means stable_refs, which the Store keeps itself (its container need not);
    /// contiguous storage serves it best, so containers with stable references come after the others.
    using implies = boost::mp11::mp_list<stable_refs>;
    using kept_by_store = boost::mp11::mp_list<stable_refs>;
    using later = boost::mp11::mp_list<stable_refs>;
    static constexpr std::string_view name = "frozen";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "No change after construction; assign rebuilds it whole.";
    static constexpr std::array<std::string_view, 3> phrases { "built once", "read only", "immutable after building" };
    static constexpr answer_says says { .stops_growth = true, .stops_shrinking = true, .stops_clearing = true, .stops_changes = true };
};

template<>
struct tags_of<change>
{
    using type = boost::mp11::mp_list<may_change, monotone_until_clear, monotone, frozen>;
};

enum class emptiness : std::uint8_t
{
    none,
    nonempty,
};

struct maybe_empty : tag_defaults
{
    using root = maybe_empty;
    static constexpr bool request_only = true;  ///< no container declares it: only a request states it
    static constexpr auto value = emptiness::none;
    static constexpr std::string_view name = "maybe_empty";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Can it be empty?";
};
/// At least one element.
struct nonempty : maybe_empty
{
    static constexpr auto value = emptiness::nonempty;
    /// It never erases, so it promises its container that it only grows; it is built from an element at least.
    using promises = boost::mp11::mp_list<monotone>;
    static constexpr std::string_view name = "nonempty";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "At least one element.";
    static constexpr std::array<std::string_view, 2> phrases { "never empty", "nonempty" };
    static constexpr answer_says says { .stops_shrinking = true, .stops_clearing = true, .needs_an_element = true };
};

template<>
struct tags_of<emptiness>
{
    using type = boost::mp11::mp_list<maybe_empty, nonempty>;
};

enum class multiplicity : std::uint8_t
{
    none,
    multi,
};

struct any_multiplicity : tag_defaults
{
    using root = any_multiplicity;
    static constexpr bool request_only = true;  ///< no container declares it: only a request states it
    static constexpr auto value = multiplicity::none;
    static constexpr std::string_view name = "any_multiplicity";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Values per key?";
};
/// Any number of values per key (a multimap).
struct multi : any_multiplicity
{
    static constexpr auto value = multiplicity::multi;
    static constexpr std::string_view name = "multi";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Any number of values per key: a multimap.";
    static constexpr std::array<std::string_view, 3> phrases { "multimap", "many values per key", "adjacency lists" };
};

template<>
struct tags_of<multiplicity>
{
    using type = boost::mp11::mp_list<any_multiplicity, multi>;
};

enum class position : std::uint8_t
{
    none,
    positional,
};

struct by_key : tag_defaults
{
    using root = by_key;
    static constexpr auto value = position::none;
    static constexpr std::string_view name = "by_key";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Reached by position?";
};
/// Identified by position 0..n-1, in order (a sequence).
struct positional : by_key
{
    static constexpr auto value = position::positional;
    /// Positions are contents: they fix the order it is iterated and hashed in.
    static constexpr std::string_view name = "positional";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Identified by position 0..n-1, in order.";
    static constexpr std::array<std::string_view, 3> phrases { "by position", "numbered as inserted", "interner" };
    static constexpr answer_says says { .fixes_iteration_order = true, .fixes_hash_order = true };
};

template<>
struct tags_of<position>
{
    using type = boost::mp11::mp_list<by_key, positional>;
};

// How a Store is used: it changes which representation is chosen, never what the Store guarantees. Only a
// request says it; docs/BENCHMARKS.md backs what each one chooses.
enum class workload : std::uint8_t
{
    none,
    small,
};
struct general : tag_defaults
{
    using root = general;
    static constexpr bool request_only = true;  ///< no container declares it: only a request states it
    static constexpr auto value = workload::none;
    static constexpr std::string_view name = "general";
    using group = hints_group;
    static constexpr std::string_view question = "How many elements?";
};
/// A few elements, or a few at a time.
struct small : general
{
    static constexpr auto value = workload::small;
    /// Few elements: contiguous storage serves them best, so containers with stable references come after the others.
    using later = boost::mp11::mp_list<stable_refs>;
    static constexpr std::string_view name = "small";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A few elements, or a few at a time: compact containers first.";
    static constexpr std::array<std::string_view, 3> phrases { "few elements", "small", "tiny" };
};

template<>
struct tags_of<workload>
{
    using type = boost::mp11::mp_list<general, small>;
};

enum class priority : std::uint8_t
{
    none,
    monotone_priority,
};

struct any_priority : tag_defaults
{
    using root = any_priority;
    static constexpr bool request_only = true;  ///< no container declares it: only a request states it
    static constexpr auto value = priority::none;
    static constexpr std::string_view name = "any_priority";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "How do priorities move?";
};
/// A priority queue whose pushes are never below the last popped (shortest paths with non-negative lengths, event
/// simulation): with integral priorities, a bucket queue serves it.
struct monotone_priority : any_priority
{
    static constexpr auto value = priority::monotone_priority;
    static constexpr std::string_view name = "monotone_priority";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "A priority queue never pushed below the last popped.";
    static constexpr std::array<std::string_view, 3> phrases { "dijkstra", "non-negative lengths", "event simulation" };
};

template<>
struct tags_of<priority>
{
    using type = boost::mp11::mp_list<any_priority, monotone_priority>;
};

enum class comparison : std::uint8_t
{
    none,
    given,
};

struct any_comparison : tag_defaults
{
    using root = any_comparison;
    static constexpr bool request_only = true;  ///< no container declares it: only a request states it
    static constexpr auto value = comparison::none;
    static constexpr std::string_view name = "any_comparison";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "What compares the elements?";
};
/// Compared by a comparator the request gives (order_by<C>), not by urd::Less: the words of the answer.
struct given_comparison : any_comparison
{
    static constexpr auto value = comparison::given;
    static constexpr std::string_view name = "order_by";
    static constexpr std::string_view promise = "caller";
    static constexpr std::string_view description = "Ordered by the given comparator (in a map, over its keys), not urd::Less.";
    static constexpr std::array<std::string_view, 3> phrases { "order by", "comparator", "priority function" };
};

template<>
struct tags_of<comparison>
{
    using type = boost::mp11::mp_list<any_comparison, given_comparison>;
};

/// Orders a sorted Store, or a by_priority queue, by comparator Compare instead of urd::Less: Compare compares two
/// elements (in a map, two keys), as std::set's does; urd::by<P> compares by a projection (order_by<by<&Job::deadline>>).
/// The order is Compare and nothing else: two elements it does not separate are equivalent, so in a unique Store they
/// are one. A tag like any other, answering `given`, that also carries Compare.
template<typename Compare>
struct order_by : given_comparison
{
    using compare = Compare;
};

}  // namespace urd
