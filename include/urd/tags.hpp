#pragma once

// The vocabulary as code: every property a container declares or a request states, as a number (urd::tag) and as a
// type (urd::sorted, ...), with its words beside it. A tag derived from another means "at least that". A category's
// root names it: its question, its group (kind, semantics, costs, hints), and in semantics its reading heading; any
// other tag has its description, the phrases a need is put in, and in semantics who keeps it (`promise`: guaranteed or
// caller). The machinery that computes with them is in urd/property_set.hpp; share/urd/vocabulary.json is generated
// from them.

#include <array>
#include <cstddef>
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


/// The categories of properties, each a question a request may answer (its words: the category's root type below),
/// in the order of urd's tag types. Each has an enum of its own whose first value, none, is no answer: what a category
/// left unstated means.
enum class category : std::uint8_t
{
    order,
    uniqueness,
    keys,
    referencing,
    layout,
    representation,
    threading,
    memory,
    clearing,
    access,
    scoping,
    layering,
    handling,
    sharing,
    direction,
    shape,
    packing,
    grouping,
    tracking,
    change,
    emptiness,
    multiplicity,
    position,
    workload,
    priority,
    hashability,
    comparing,
    indexing,
    copying,
    destruction,
    refilling,
    order_source,
    natural_order_source,
    entry_order,
    priority_key,
    hash_source,
    row_flattening,
    row_updates,
    row_reuse,
    member_naming,
    count  ///< not a category: how many there are
};

enum class order : std::uint8_t
{
    none,
    sorted,
    insertion_ordered,
};
enum class uniqueness : std::uint8_t
{
    none,
    unique,
};
enum class keys : std::uint8_t
{
    none,
    dense,
    sparse,
};
enum class referencing : std::uint8_t
{
    none,
    stable_refs,
};
enum class layout : std::uint8_t
{
    none,
    contiguous,
};
enum class representation : std::uint8_t
{
    none,
    canonical,
};
enum class threading : std::uint8_t
{
    none,
    concurrent,
};
enum class memory : std::uint8_t
{
    none,
    retains,
    reclaims,
};
enum class clearing : std::uint8_t
{
    none,
    proportional_clear,
    constant_clear,
};
enum class access : std::uint8_t
{
    none,
    fifo,
    lifo,
    by_priority,
};
enum class scoping : std::uint8_t
{
    none,
    scoped,
};
enum class layering : std::uint8_t
{
    none,
    layered,
};
enum class handling : std::uint8_t
{
    none,
    handles,
};
enum class sharing : std::uint8_t
{
    none,
    structural_sharing,
};
enum class direction : std::uint8_t
{
    none,
    bidirectional,
};
enum class shape : std::uint8_t
{
    none,
    fixed_shape,
};
enum class packing : std::uint8_t
{
    none,
    flat,
};
enum class grouping : std::uint8_t
{
    none,
    partition,
};
enum class tracking : std::uint8_t
{
    none,
    change_log,
};
enum class change : std::uint8_t
{
    none,
    monotone,
    frozen,
};
enum class emptiness : std::uint8_t
{
    none,
    nonempty,
};
enum class multiplicity : std::uint8_t
{
    none,
    multi,
};
enum class position : std::uint8_t
{
    none,
    positional,
};
enum class workload : std::uint8_t
{
    none,
    small,
};
enum class priority : std::uint8_t
{
    none,
    monotone_priority,
};
enum class hashability : std::uint8_t
{
    none,
    can_hash,
};
enum class comparing : std::uint8_t
{
    none,
    can_order,
};
enum class indexing : std::uint8_t
{
    none,
    an_index,
};
enum class copying : std::uint8_t
{
    none,
    copies_as_bytes,
};
enum class destruction : std::uint8_t
{
    none,
    destroys_trivially,
};
enum class refilling : std::uint8_t
{
    none,
    refillable,
};
enum class order_source : std::uint8_t
{
    none,
    own_order,
};
enum class natural_order_source : std::uint8_t
{
    none,
    natural_ordered,
};
enum class entry_order : std::uint8_t
{
    none,
    by_entry,
};
enum class priority_key : std::uint8_t
{
    none,
    integral_priority,
};
enum class hash_source : std::uint8_t
{
    none,
    urd_hashed,
};
enum class row_flattening : std::uint8_t
{
    none,
    rows_flatten,
};
enum class row_updates : std::uint8_t
{
    none,
    rows_in_place,
};
enum class row_reuse : std::uint8_t
{
    none,
    rows_reused,
};
enum class member_naming : std::uint8_t
{
    none,
    names_members,
};

/// The category of enum E, and the enum of a category.
template<typename E>
struct category_of;
template<category C>
struct enum_of;
template<>
struct category_of<order>
{
    static constexpr category value = category::order;
    static constexpr std::uint8_t offset = 0;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::order>
{
    using type = order;
};
template<>
struct category_of<uniqueness>
{
    static constexpr category value = category::uniqueness;
    static constexpr std::uint8_t offset = 3;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::uniqueness>
{
    using type = uniqueness;
};
template<>
struct category_of<keys>
{
    static constexpr category value = category::keys;
    static constexpr std::uint8_t offset = 5;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::keys>
{
    using type = keys;
};
template<>
struct category_of<referencing>
{
    static constexpr category value = category::referencing;
    static constexpr std::uint8_t offset = 8;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::referencing>
{
    using type = referencing;
};
template<>
struct category_of<layout>
{
    static constexpr category value = category::layout;
    static constexpr std::uint8_t offset = 10;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::layout>
{
    using type = layout;
};
template<>
struct category_of<representation>
{
    static constexpr category value = category::representation;
    static constexpr std::uint8_t offset = 12;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::representation>
{
    using type = representation;
};
template<>
struct category_of<threading>
{
    static constexpr category value = category::threading;
    static constexpr std::uint8_t offset = 14;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::threading>
{
    using type = threading;
};
template<>
struct category_of<memory>
{
    static constexpr category value = category::memory;
    static constexpr std::uint8_t offset = 16;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::memory>
{
    using type = memory;
};
template<>
struct category_of<clearing>
{
    static constexpr category value = category::clearing;
    static constexpr std::uint8_t offset = 19;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::clearing>
{
    using type = clearing;
};
template<>
struct category_of<access>
{
    static constexpr category value = category::access;
    static constexpr std::uint8_t offset = 22;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::access>
{
    using type = access;
};
template<>
struct category_of<scoping>
{
    static constexpr category value = category::scoping;
    static constexpr std::uint8_t offset = 26;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::scoping>
{
    using type = scoping;
};
template<>
struct category_of<layering>
{
    static constexpr category value = category::layering;
    static constexpr std::uint8_t offset = 28;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::layering>
{
    using type = layering;
};
template<>
struct category_of<handling>
{
    static constexpr category value = category::handling;
    static constexpr std::uint8_t offset = 30;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::handling>
{
    using type = handling;
};
template<>
struct category_of<sharing>
{
    static constexpr category value = category::sharing;
    static constexpr std::uint8_t offset = 32;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::sharing>
{
    using type = sharing;
};
template<>
struct category_of<direction>
{
    static constexpr category value = category::direction;
    static constexpr std::uint8_t offset = 34;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::direction>
{
    using type = direction;
};
template<>
struct category_of<shape>
{
    static constexpr category value = category::shape;
    static constexpr std::uint8_t offset = 36;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::shape>
{
    using type = shape;
};
template<>
struct category_of<packing>
{
    static constexpr category value = category::packing;
    static constexpr std::uint8_t offset = 38;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::packing>
{
    using type = packing;
};
template<>
struct category_of<grouping>
{
    static constexpr category value = category::grouping;
    static constexpr std::uint8_t offset = 40;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::grouping>
{
    using type = grouping;
};
template<>
struct category_of<tracking>
{
    static constexpr category value = category::tracking;
    static constexpr std::uint8_t offset = 42;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::tracking>
{
    using type = tracking;
};
template<>
struct category_of<change>
{
    static constexpr category value = category::change;
    static constexpr std::uint8_t offset = 44;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::change>
{
    using type = change;
};
template<>
struct category_of<emptiness>
{
    static constexpr category value = category::emptiness;
    static constexpr std::uint8_t offset = 47;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::emptiness>
{
    using type = emptiness;
};
template<>
struct category_of<multiplicity>
{
    static constexpr category value = category::multiplicity;
    static constexpr std::uint8_t offset = 49;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::multiplicity>
{
    using type = multiplicity;
};
template<>
struct category_of<position>
{
    static constexpr category value = category::position;
    static constexpr std::uint8_t offset = 51;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::position>
{
    using type = position;
};
template<>
struct category_of<workload>
{
    static constexpr category value = category::workload;
    static constexpr std::uint8_t offset = 53;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::workload>
{
    using type = workload;
};
template<>
struct category_of<priority>
{
    static constexpr category value = category::priority;
    static constexpr std::uint8_t offset = 55;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::priority>
{
    using type = priority;
};
template<>
struct category_of<hashability>
{
    static constexpr category value = category::hashability;
    static constexpr std::uint8_t offset = 57;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::hashability>
{
    using type = hashability;
};
template<>
struct category_of<comparing>
{
    static constexpr category value = category::comparing;
    static constexpr std::uint8_t offset = 59;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::comparing>
{
    using type = comparing;
};
template<>
struct category_of<indexing>
{
    static constexpr category value = category::indexing;
    static constexpr std::uint8_t offset = 61;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::indexing>
{
    using type = indexing;
};
template<>
struct category_of<copying>
{
    static constexpr category value = category::copying;
    static constexpr std::uint8_t offset = 63;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::copying>
{
    using type = copying;
};
template<>
struct category_of<destruction>
{
    static constexpr category value = category::destruction;
    static constexpr std::uint8_t offset = 65;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::destruction>
{
    using type = destruction;
};
template<>
struct category_of<refilling>
{
    static constexpr category value = category::refilling;
    static constexpr std::uint8_t offset = 67;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::refilling>
{
    using type = refilling;
};
template<>
struct category_of<order_source>
{
    static constexpr category value = category::order_source;
    static constexpr std::uint8_t offset = 69;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::order_source>
{
    using type = order_source;
};
template<>
struct category_of<natural_order_source>
{
    static constexpr category value = category::natural_order_source;
    static constexpr std::uint8_t offset = 71;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::natural_order_source>
{
    using type = natural_order_source;
};
template<>
struct category_of<entry_order>
{
    static constexpr category value = category::entry_order;
    static constexpr std::uint8_t offset = 73;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::entry_order>
{
    using type = entry_order;
};
template<>
struct category_of<priority_key>
{
    static constexpr category value = category::priority_key;
    static constexpr std::uint8_t offset = 75;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::priority_key>
{
    using type = priority_key;
};
template<>
struct category_of<hash_source>
{
    static constexpr category value = category::hash_source;
    static constexpr std::uint8_t offset = 77;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::hash_source>
{
    using type = hash_source;
};
template<>
struct category_of<row_flattening>
{
    static constexpr category value = category::row_flattening;
    static constexpr std::uint8_t offset = 79;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::row_flattening>
{
    using type = row_flattening;
};
template<>
struct category_of<row_updates>
{
    static constexpr category value = category::row_updates;
    static constexpr std::uint8_t offset = 81;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::row_updates>
{
    using type = row_updates;
};
template<>
struct category_of<row_reuse>
{
    static constexpr category value = category::row_reuse;
    static constexpr std::uint8_t offset = 83;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::row_reuse>
{
    using type = row_reuse;
};
template<>
struct category_of<member_naming>
{
    static constexpr category value = category::member_naming;
    static constexpr std::uint8_t offset = 85;  ///< where its answers start among all tags
};
template<>
struct enum_of<category::member_naming>
{
    using type = member_naming;
};

/// Whether E is a category's enum.
template<typename E>
concept category_enum = requires { category_of<E>::value; };

/// How many tags there are, every category's none included, and each one's category and answer, by its position
/// among all tags (each category's none, then its answers, in the order of the categories).
inline constexpr std::size_t all_tag_count = 87;
inline constexpr std::array<category, all_tag_count> category_at_position { category::order, category::order, category::order, category::uniqueness, category::uniqueness, category::keys, category::keys, category::keys, category::referencing, category::referencing, category::layout, category::layout, category::representation, category::representation, category::threading, category::threading, category::memory, category::memory, category::memory, category::clearing, category::clearing, category::clearing, category::access, category::access, category::access, category::access, category::scoping, category::scoping, category::layering, category::layering, category::handling, category::handling, category::sharing, category::sharing, category::direction, category::direction, category::shape, category::shape, category::packing, category::packing, category::grouping, category::grouping, category::tracking, category::tracking, category::change, category::change, category::change, category::emptiness, category::emptiness, category::multiplicity, category::multiplicity, category::position, category::position, category::workload, category::workload, category::priority, category::priority, category::hashability, category::hashability, category::comparing, category::comparing, category::indexing, category::indexing, category::copying, category::copying, category::destruction, category::destruction, category::refilling, category::refilling, category::order_source, category::order_source, category::natural_order_source, category::natural_order_source, category::entry_order, category::entry_order, category::priority_key, category::priority_key, category::hash_source, category::hash_source, category::row_flattening, category::row_flattening, category::row_updates, category::row_updates, category::row_reuse, category::row_reuse, category::member_naming, category::member_naming };
inline constexpr std::array<std::uint8_t, all_tag_count> answer_at_position { 0, 1, 2, 0, 1, 0, 1, 2, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 0, 1, 2, 0, 1, 2, 3, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 };

/// A property: a category and its answer, from any category's enum (order::sorted, uniqueness::unique, …), held as its
/// position among all tags. What the machinery computes with; users name the tag types below.
struct tag
{
    std::uint8_t position = all_tag_count;  ///< all_tag_count: no tag

    constexpr tag() = default;
    template<category_enum E>
    constexpr tag(E answer)  // NOLINT(google-explicit-constructor): every category's answer is a tag
        : position(static_cast<std::uint8_t>(category_of<E>::offset + static_cast<std::uint8_t>(answer)))
    {
    }
    [[nodiscard]] constexpr category of() const { return category_at_position[position]; }
    [[nodiscard]] constexpr std::uint8_t value() const { return answer_at_position[position]; }
    friend constexpr bool operator==(tag, tag) = default;
};

/// The answer of tag `t` in its category's own enum.
template<tag T>
inline constexpr auto answer_of = static_cast<typename enum_of<T.of()>::type>(T.value());

/// What a Store is, by how its elements are placed and reached (kind_of below).
enum class kind : std::uint8_t
{
    sequence,   ///< values by position
    multiset,   ///< values kept by value, in order, duplicates kept
    set,        ///< keys, each once
    map,        ///< a value per key, or several
    pool,       ///< rows interned by handle (handles, unique)
    slots,      ///< values in slots, by handle (handles)
    records,    ///< records stored flat, by handle
    queue,      ///< reached by front or top
    partition,  ///< keys in disjoint classes
};

/// A kind's name, as a request or a tool writes it.
constexpr std::string_view name(kind of)
{
    switch (of)
    {
        case kind::sequence:
            return "sequence";
        case kind::multiset:
            return "multiset";
        case kind::set:
            return "set";
        case kind::map:
            return "map";
        case kind::pool:
            return "pool";
        case kind::slots:
            return "slots";
        case kind::records:
            return "records";
        case kind::queue:
            return "queue";
        case kind::partition:
            return "partition";
    }
    return {};
}

/// The tag of a container's threading and of its references, by a flag of its own: concurrent or not, stable or not.
constexpr tag threads(bool concurrent_) { return concurrent_ ? threading::concurrent : threading::none; }
constexpr tag references(bool stable) { return stable ? referencing::stable_refs : referencing::none; }

/// Whether a kind keeps its elements by value (inserted where their value puts them), not where they are put.
constexpr bool keeps_by_value(kind of) { return of == kind::multiset || of == kind::set || of == kind::map; }
/// The kind of a request: from the tags it states, else from its shape (key<K>: a set, key<K> and value<V>: a map,
/// value<V>: a sequence). A priority queue with handles is a queue; a pool of records stored flat is records.
template<typename Set>
constexpr kind kind_of(const Set& stated, bool keyed, bool valued)
{
    if (stated.has(grouping::partition))
        return kind::partition;
    if (stated.has(access::fifo) || stated.has(access::lifo) || stated.has(access::by_priority) || stated.has(priority::monotone_priority))
        return kind::queue;
    if (stated.has(packing::flat))
        return kind::records;
    if (stated.has(handling::handles))
        return stated.has(uniqueness::unique) ? kind::pool : kind::slots;
    if (keyed)
        return valued ? kind::map : kind::set;
    if (stated.has(order::sorted) || stated.has(uniqueness::unique) || stated.has(multiplicity::multi))
        return kind::multiset;
    return kind::sequence;
}

/// A preference: where a request holds `when`, containers holding `later` come after the others that serve it.
/// Contiguous storage is more compact and faster to search and iterate where elements are few or never change, so
/// stable storage (a node per element, segments that never move) comes last for them (docs/BENCHMARKS.md).
struct preference
{
    tag when = tag {};
    tag later = tag {};
};
inline constexpr std::array preferences { preference { .when = change::frozen, .later = referencing::stable_refs },
                                          preference { .when = workload::small, .later = referencing::stable_refs } };

/// What a Store held as an element of another gets beyond its own request: its storage given back when it is
/// destroyed, as erase and clear do.
inline constexpr std::array<tag, 1> nested_gives { memory::reclaims };

/// The registry tree's levels: the categories its containers are told apart by, high level first, below the kind:
/// what the interface is, then what holds, then what it costs. urd's selection checks each container's tags as a
/// whole (urd/registry/scan.hpp); a view of the registry (urd-search) follows this order.
inline constexpr std::array<tag, 21> registry_levels { uniqueness::none,      multiplicity::none, handling::none,   access::none,   packing::none,
                                              grouping::none,    position::none,           order::none,    keys::none,     referencing::none,
                                              threading::none, layering::none,     scoping::none,    tracking::none, direction::none,
                                              shape::none,       layout::none,       memory::none,   clearing::none,    representation::none,
                                              sharing::none };

/// What a request asks of its container: what it means, but for what the Store keeps itself: once nothing changes
/// (frozen), nothing moves, so stable references need nothing of the container.
template<typename Set>
constexpr Set asked_of_container(Set stated)
{
    if (stated.has(change::frozen))
        stated.put(referencing::none);
    return stated;
}

/// Whether a request means something, whatever container would hold it: a partition groups keys, without values; a
/// pool (handles) is reached by handle, not by key; monotone_priority is a promise of a priority queue; order_by<P>
/// orders a sorted Store or a priority queue, and on unique keys breaks ties by the keys' own order, so they must be
/// orderable. Anything else no container serves fails in the registry.
template<typename Set>
constexpr bool well_formed(const Set& stated, bool keyed, bool valued, bool ordered_by, bool orderable)
{
    return (!stated.has(grouping::partition) || !valued) && (!stated.has(handling::handles) || !keyed)
           && (!stated.has(priority::monotone_priority) || stated.has(access::by_priority))
           && (!ordered_by || stated.has(order::sorted) || stated.has(access::by_priority))
           && (!ordered_by || !keyed || stated.has(multiplicity::multi) || orderable);
}

/// What a request means beyond what it states, by its kind and by what its Store's interface keeps: a set, and a map
/// without multi, keep each key once; a set with positions numbers its keys as they come (an interner), unless it
/// asks for another order; once nothing changes (frozen), nothing moves.
template<typename Set>
constexpr Set meaning(Set stated, kind of)
{
    if (of == kind::set || (of == kind::map && !stated.has(multiplicity::multi)))
        stated.put(uniqueness::unique);
    if (of == kind::set && stated.has(position::positional) && stated.in(order::none) == order::none)
        stated.put(order::insertion_ordered);
    if (stated.has(change::frozen))
        stated.put(referencing::stable_refs);
    return stated;
}

/// What a request promises its container: what it means, and that a Store which must stay nonempty never erases, so
/// a container that only grows serves it.
template<typename Set>
constexpr Set promised_to_container(Set meant)
{
    if (meant.has(emptiness::nonempty) && !meant.has(change::monotone))
        meant.put(change::monotone);
    return meant;
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
    static constexpr std::string_view name = "sorted";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Iterated in ascending order by urd::Less.";
    static constexpr std::array<std::string_view, 4> phrases { "sorted", "in order", "ordered by key", "range queries" };
};
/// In the order they were inserted: a new element goes last, and the others keep their places.
struct insertion_ordered : any_order
{
    static constexpr auto value = order::insertion_ordered;
    static constexpr std::string_view name = "insertion_ordered";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Iterated in the order elements were first inserted.";
    static constexpr std::array<std::string_view, 3> phrases { "insertion order", "first come first listed", "keeps the order added" };
};

struct duplicates
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

struct any_keys
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
    static constexpr bool demanded = true;
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

struct moving_refs
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
};

struct any_layout
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

struct any_representation
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
    static constexpr std::string_view name = "canonical";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Equal contents have equal representations.";
    static constexpr std::array<std::string_view, 3> phrases { "canonical", "same contents same bytes", "hashable by contents" };
};

struct single_threaded
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
};

struct any_memory
{
    using root = any_memory;
    static constexpr auto value = memory::none;
    static constexpr std::string_view name = "any_memory";
    using group = costs_group;
    static constexpr std::string_view question = "What happens to freed memory?";
    using heading = no_heading;
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

struct any_clear
{
    using root = any_clear;
    static constexpr auto value = clearing::none;
    static constexpr std::string_view name = "any_clear";
    using group = costs_group;
    static constexpr std::string_view question = "How is it cleared?";
    using heading = no_heading;
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
};

struct any_access
{
    using root = any_access;
    static constexpr auto value = access::none;
    static constexpr std::string_view name = "any_access";
    using group = kind_group;
    static constexpr std::string_view question = "How are elements taken out?";
    using heading = no_heading;
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

struct any_scope
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
};

struct any_layering
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
};

struct any_handle
{
    using root = any_handle;
    static constexpr auto value = handling::none;
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
    static constexpr auto value = handling::handles;
    static constexpr std::string_view name = "handles";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A pool: insert gives a handle; with unique, equal elements get equal handles.";
    static constexpr std::array<std::string_view, 4> phrases { "interning", "handles", "deduplicated rows", "hash consing" };
};

struct any_sharing
{
    using root = any_sharing;
    static constexpr auto value = sharing::none;
    static constexpr std::string_view name = "any_sharing";
    using group = costs_group;
    static constexpr std::string_view question = "Rows share structure?";
    using heading = no_heading;
};
/// Parts that are equal across elements (rows) are stored once.
struct structural_sharing : any_sharing
{
    static constexpr auto value = sharing::structural_sharing;
    static constexpr std::string_view name = "structural_sharing";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "Parts that are equal across rows are stored once.";
    static constexpr std::array<std::string_view, 3> phrases { "rows differing in a few places", "persistent", "shared structure" };
};

struct any_direction
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
};

struct any_shape
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
    static constexpr std::string_view name = "fixed_shape";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Rows whose number and lengths are set at construction; only their contents change.";
    static constexpr std::array<std::string_view, 3> phrases { "fixed rows", "matrix", "table of known shape" };
};

struct any_packing
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

struct any_grouping
{
    using root = any_grouping;
    static constexpr auto value = grouping::none;
    static constexpr std::string_view name = "any_grouping";
    using group = kind_group;
    static constexpr std::string_view question = "Groups of keys?";
    using heading = no_heading;
};
/// A partition: its elements fall into disjoint classes; merge joins two classes, find names an element's class.
struct partition : any_grouping
{
    static constexpr auto value = grouping::partition;
    static constexpr std::string_view name = "partition";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A partition: elements fall into disjoint classes that merge joins.";
    static constexpr std::array<std::string_view, 3> phrases { "union find", "disjoint sets", "equivalence classes" };
};

struct any_tracking
{
    using root = any_tracking;
    static constexpr auto value = tracking::none;
    static constexpr std::string_view name = "any_tracking";
    using group = semantics_group;
    using heading = more_heading;
    static constexpr std::string_view question = "Tracks what changed?";
};
/// Offers added() and removed(): exactly the elements that changed since clear_changes(), for semi-naive evaluation.
struct change_log : any_tracking
{
    static constexpr auto value = tracking::change_log;
    static constexpr std::string_view name = "change_log";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "added() and removed(): the elements changed since clear_changes().";
    static constexpr std::array<std::string_view, 3> phrases { "delta", "what changed", "semi-naive evaluation" };
};

// What a Store's interface enforces: no container declares a lifecycle or emptiness; one that cannot erase relies on
// monotone, one that cannot insert on frozen. Multiplicity a multimap declares (multi), and a request states it.
struct may_change
{
    using root = may_change;
    static constexpr auto value = change::none;
    static constexpr std::string_view name = "may_change";
    using group = semantics_group;
    using heading = holds_heading;
    static constexpr std::string_view question = "Does it change after building?";
};
/// Only grows: nothing is ever removed.
struct monotone : may_change
{
    static constexpr auto value = change::monotone;
    static constexpr std::string_view name = "monotone";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "Only grows: nothing is ever removed.";
    static constexpr std::array<std::string_view, 3> phrases { "append only", "only grows", "never erased" };
};
/// No change after construction (assign rebuilds it whole): any number of threads may read it between rebuilds,
/// so concurrent, which is about inserts from threads at once, is not asked of it.
struct frozen : monotone
{
    static constexpr auto value = change::frozen;
    static constexpr std::string_view name = "frozen";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "No change after construction; assign rebuilds it whole.";
    static constexpr std::array<std::string_view, 3> phrases { "built once", "read only", "immutable after building" };
};

struct maybe_empty
{
    using root = maybe_empty;
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
    static constexpr std::string_view name = "nonempty";
    static constexpr std::string_view promise = "guaranteed";
    static constexpr std::string_view description = "At least one element.";
    static constexpr std::array<std::string_view, 2> phrases { "never empty", "nonempty" };
};

struct any_multiplicity
{
    using root = any_multiplicity;
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

struct by_key
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
    static constexpr auto value = workload::none;
    static constexpr std::string_view name = "general";
    using group = hints_group;
    static constexpr std::string_view question = "How many elements?";
    using heading = no_heading;
};
/// A few elements, or a few at a time.
struct small : general
{
    static constexpr auto value = workload::small;
    static constexpr std::string_view name = "small";
    static constexpr std::string_view promise = "";
    static constexpr std::string_view description = "A few elements, or a few at a time: compact containers first.";
    static constexpr std::array<std::string_view, 3> phrases { "few elements", "small", "tiny" };
};

struct any_priority
{
    using root = any_priority;
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

// What the data is: derived from the element types (urd/registry/features.hpp), never stated; a container relies on
// them per role (its key or elements, its mapped values, its rows).

// Can urd hash it?
struct any_can_hash
{
    using root = any_can_hash;
    static constexpr auto value = hashability::none;
    static constexpr std::string_view name = "any_can_hash";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Can urd hash it?";
};
/// urd::Hash and urd::EqualTo hash it and compare it for equality. Derived from the types, never stated.
struct can_hash : any_can_hash
{
    static constexpr auto value = hashability::can_hash;
    static constexpr std::string_view name = "can_hash";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "urd::Hash and urd::EqualTo hash it and compare it for equality.";
    static constexpr std::array<std::string_view, 2> phrases { "hashable", "has a hash" };
};

// Can urd order it?
struct any_can_order
{
    using root = any_can_order;
    static constexpr auto value = comparing::none;
    static constexpr std::string_view name = "any_can_order";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Can urd order it?";
};
/// urd::Less orders it. Derived from the types, never stated.
struct can_order : any_can_order
{
    static constexpr auto value = comparing::can_order;
    static constexpr std::string_view name = "can_order";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "urd::Less orders it.";
    static constexpr std::array<std::string_view, 2> phrases { "orderable", "comparable" };
};

// Is it an index?
struct any_an_index
{
    using root = any_an_index;
    static constexpr auto value = indexing::none;
    static constexpr std::string_view name = "any_an_index";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Is it an index?";
};
/// It is an index 0..n-1 (urd::index_of gives its position). Derived from the types, never stated.
struct an_index : any_an_index
{
    static constexpr auto value = indexing::an_index;
    static constexpr std::string_view name = "an_index";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "It is an index 0..n-1 (urd::index_of gives its position).";
    static constexpr std::array<std::string_view, 2> phrases { "index", "id" };
};

// Copied as bytes?
struct any_copies_as_bytes
{
    using root = any_copies_as_bytes;
    static constexpr auto value = copying::none;
    static constexpr std::string_view name = "any_copies_as_bytes";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Copied as bytes?";
};
/// It is trivially copyable: copied as bytes. Derived from the types, never stated.
struct copies_as_bytes : any_copies_as_bytes
{
    static constexpr auto value = copying::copies_as_bytes;
    static constexpr std::string_view name = "copies_as_bytes";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "It is trivially copyable: copied as bytes.";
    static constexpr std::array<std::string_view, 2> phrases { "trivially copyable", "plain data" };
};

// Destroyed without code?
struct any_destroys_trivially
{
    using root = any_destroys_trivially;
    static constexpr auto value = destruction::none;
    static constexpr std::string_view name = "any_destroys_trivially";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Destroyed without code?";
};
/// It is trivially destructible: destroyed without running code. Derived from the types, never stated.
struct destroys_trivially : any_destroys_trivially
{
    static constexpr auto value = destruction::destroys_trivially;
    static constexpr std::string_view name = "destroys_trivially";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "It is trivially destructible: destroyed without running code.";
    static constexpr std::array<std::string_view, 2> phrases { "trivially destructible", "no destructor" };
};

// Can it fill an emptied slot?
struct any_refillable
{
    using root = any_refillable;
    static constexpr auto value = refilling::none;
    static constexpr std::string_view name = "any_refillable";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Can it fill an emptied slot?";
};
/// It is default-constructible and movable, so a slot emptied can be filled again. Derived from the types, never stated.
struct refillable : any_refillable
{
    static constexpr auto value = refilling::refillable;
    static constexpr std::string_view name = "refillable";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "It is default-constructible and movable, so a slot emptied can be filled again.";
    static constexpr std::array<std::string_view, 2> phrases { "default constructible", "movable" };
};

// Its own order?
struct any_own_order
{
    using root = any_own_order;
    static constexpr auto value = order_source::none;
    static constexpr std::string_view name = "any_own_order";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Its own order?";
};
/// The request gives no order_by: the elements are in their own order. Derived from the types, never stated.
struct own_order : any_own_order
{
    static constexpr auto value = order_source::own_order;
    static constexpr std::string_view name = "own_order";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "The request gives no order_by: the elements are in their own order.";
    static constexpr std::array<std::string_view, 2> phrases { "no order_by", "natural keys" };
};

// Ordered as sorted means?
struct any_natural_ordered
{
    using root = any_natural_ordered;
    static constexpr auto value = natural_order_source::none;
    static constexpr std::string_view name = "any_natural_ordered";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Ordered as sorted means?";
};
/// What orders the elements orders them as sorted means: urd::Less on orderable elements, or an order_by that agrees with it. Derived from the types, never stated.
struct natural_ordered : any_natural_ordered
{
    static constexpr auto value = natural_order_source::natural_ordered;
    static constexpr std::string_view name = "natural_ordered";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "What orders the elements orders them as sorted means: urd::Less on orderable elements, or an order_by that agrees with it.";
    static constexpr std::array<std::string_view, 2> phrases { "sortable", "ordered by Less" };
};

// Ordered by entry?
struct any_by_entry
{
    using root = any_by_entry;
    static constexpr auto value = entry_order::none;
    static constexpr std::string_view name = "any_by_entry";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Ordered by entry?";
};
/// The order_by orders a map's entries, by key and value. Derived from the types, never stated.
struct by_entry : any_by_entry
{
    static constexpr auto value = entry_order::by_entry;
    static constexpr std::string_view name = "by_entry";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "The order_by orders a map's entries, by key and value.";
    static constexpr std::array<std::string_view, 2> phrases { "ordered by value", "entry order" };
};

// Integral priority?
struct any_integral_priority
{
    using root = any_integral_priority;
    static constexpr auto value = priority_key::none;
    static constexpr std::string_view name = "any_integral_priority";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Integral priority?";
};
/// What the order compares is an integral number (a bucket per value). Derived from the types, never stated.
struct integral_priority : any_integral_priority
{
    static constexpr auto value = priority_key::integral_priority;
    static constexpr std::string_view name = "integral_priority";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "What the order compares is an integral number (a bucket per value).";
    static constexpr std::array<std::string_view, 2> phrases { "integer priority", "bucketed priority" };
};

// Hashed by urd?
struct any_urd_hashed
{
    using root = any_urd_hashed;
    static constexpr auto value = hash_source::none;
    static constexpr std::string_view name = "any_urd_hashed";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Hashed by urd?";
};
/// It is hashed by urd's own hash and equality, or ones that agree with them. Derived from the types, never stated.
struct urd_hashed : any_urd_hashed
{
    static constexpr auto value = hash_source::urd_hashed;
    static constexpr std::string_view name = "urd_hashed";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "It is hashed by urd's own hash and equality, or ones that agree with them.";
    static constexpr std::array<std::string_view, 2> phrases { "natural hash", "urd hash" };
};

// Rows that flatten?
struct any_rows_flatten
{
    using root = any_rows_flatten;
    static constexpr auto value = row_flattening::none;
    static constexpr std::string_view name = "any_rows_flatten";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Rows that flatten?";
};
/// Its rows are frozen and hold elements, or rows kept in order: a jagged array stores them flattened. Derived from the types, never stated.
struct rows_flatten : any_rows_flatten
{
    static constexpr auto value = row_flattening::rows_flatten;
    static constexpr std::string_view name = "rows_flatten";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "Its rows are frozen and hold elements, or rows kept in order: a jagged array stores them flattened.";
    static constexpr std::array<std::string_view, 2> phrases { "frozen rows", "flattenable rows" };
};

// Rows changing in place?
struct any_rows_in_place
{
    using root = any_rows_in_place;
    static constexpr auto value = row_updates::none;
    static constexpr std::string_view name = "any_rows_in_place";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Rows changing in place?";
};
/// Its rows change in place: sequences, dense sets' bits, or fixed-shape rows of such rows at any depth. Derived from the types, never stated.
struct rows_in_place : any_rows_in_place
{
    static constexpr auto value = row_updates::rows_in_place;
    static constexpr std::string_view name = "rows_in_place";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "Its rows change in place: sequences, dense sets' bits, or fixed-shape rows of such rows at any depth.";
    static constexpr std::array<std::string_view, 2> phrases { "fixed rows", "rows changed in place" };
};

// Rows kept for reuse?
struct any_rows_reused
{
    using root = any_rows_reused;
    static constexpr auto value = row_reuse::none;
    static constexpr std::string_view name = "any_rows_reused";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Rows kept for reuse?";
};
/// Its rows are Stores that may shrink (not frozen, monotone, nonempty or of a fixed shape): emptied rows can be kept and refilled. Derived from the types, never stated.
struct rows_reused : any_rows_reused
{
    static constexpr auto value = row_reuse::rows_reused;
    static constexpr std::string_view name = "rows_reused";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "Its rows are Stores that may shrink (not frozen, monotone, nonempty or of a fixed shape): emptied rows can be kept and refilled.";
    static constexpr std::array<std::string_view, 2> phrases { "reusable rows", "rows cleared and refilled" };
};

// Names its members?
struct any_names_members
{
    using root = any_names_members;
    static constexpr auto value = member_naming::none;
    static constexpr std::string_view name = "any_names_members";
    using group = semantics_group;
    using heading = promise_heading;
    static constexpr std::string_view question = "Names its members?";
};
/// It names its members for serialization (static constexpr cista_members()), so cista can store it as bytes. Derived from the types, never stated.
struct names_members : any_names_members
{
    static constexpr auto value = member_naming::names_members;
    static constexpr std::string_view name = "names_members";
    static constexpr std::string_view promise = "data";
    static constexpr std::string_view description = "It names its members for serialization (static constexpr cista_members()), so cista can store it as bytes.";
    static constexpr std::array<std::string_view, 2> phrases { "serializable", "cista members" };
};

}  // namespace urd
