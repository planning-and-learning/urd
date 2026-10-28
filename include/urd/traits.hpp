#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ranges>
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
    count  ///< not a tag: how many there are
};

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
};
/// No two elements are equal.
struct unique : duplicates
{
    static constexpr tag value = tag::unique;
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
/// A reference to an element stays valid until that element is removed (erase, clear).
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
/// Inserts may run concurrently with each other and with reads of elements already published. Not covered:
/// erase, clear and iteration, which need no other operation running.
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
};
/// A stack: the last element pushed leaves first (top, push, pop).
struct lifo : any_access
{
    static constexpr tag value = tag::lifo;
};
/// A priority queue: the least element by urd::Less leaves first (top, push, pop).
struct by_priority : any_access
{
    static constexpr tag value = tag::by_priority;
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
};
/// A pool: each element is identified by a handle that insert gives, and equal elements get equal handles. Not
/// iterated or counted.
struct handles : any_handle
{
    static constexpr tag value = tag::handles;
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

struct any_grouping
{
    using root = any_grouping;
    static constexpr tag value = tag::any_grouping;
};
/// A partition: its elements fall into disjoint classes; merge joins two classes, find names an element's class.
struct partition : any_grouping
{
    static constexpr tag value = tag::partition;
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
/// No change after construction.
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
                           small>;

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

/// The roots of the categories a container declares, then those only a Store's request has.
inline constexpr std::array guaranteed_roots { tag::any_order,          tag::duplicates,      tag::any_keys,      tag::moving_refs,  tag::any_layout,
                                               tag::any_representation, tag::single_threaded, tag::immutable,     tag::any_memory,   tag::any_clear,
                                               tag::any_access,         tag::any_scope,       tag::any_layering,  tag::any_tracking, tag::any_handle,
                                               tag::any_sharing,        tag::any_grouping,    tag::any_direction, tag::any_shape };
inline constexpr std::array requested_roots { tag::may_change, tag::maybe_empty, tag::any_multiplicity, tag::by_key, tag::general };

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

template<typename R, typename S>
class Store;

}  // namespace detail

/// Whether R is a Store.
template<typename R>
inline constexpr bool is_store = false;
template<typename R, typename S>
inline constexpr bool is_store<detail::Store<R, S>> = true;

/// What a container guarantees about its elements where they are Stores: the properties a nested Store's rows
/// have as the container holds them. By default their own, except memory: destroying a row frees what it owns,
/// as erase and clear do. A container that keeps rows, or stores them flattened, specializes it.
namespace detail
{

/// What R holds: its value_type where it names one (a queue or pool is no range), else what iterating it gives.
template<typename R>
struct held
{
    using type = std::ranges::range_value_t<R>;
};
template<typename R>
    requires requires { typename R::value_type; }
struct held<R>
{
    using type = typename R::value_type;
};

}  // namespace detail

template<typename R>
struct element_traits : with_t<container_traits<typename detail::held<R>::type>, reclaims>
{
};

namespace detail
{

template<typename X>
struct traits_of
{
    using type = container_traits<std::remove_cvref_t<X>>;
};
template<typename X>
    requires requires { X::urd_properties; }
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
