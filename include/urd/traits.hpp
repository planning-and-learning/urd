#pragma once

#include <concepts>
#include <ranges>
#include <type_traits>

namespace urd
{

// Properties, in the style of Boost.Graph's graph_traits: each answers one question (a category) with a tag
// type, and a tag derived from another means "at least that". The root of a category promises nothing, and names
// itself as `root`, which every tag of its category inherits: a tag's category is one typedef away.

// What a container guarantees by itself.
struct any_order
{
    using root = any_order;
};
/// Ascending by urd::Less: operator< for numbers, member by member for identified types.
struct sorted : any_order
{
};
/// In the order they were inserted: a new element goes last, and the others keep their places.
struct insertion_ordered : any_order
{
};

struct duplicates
{
    using root = duplicates;
};
/// No two elements are equal.
struct unique : duplicates
{
};

struct any_keys
{
    using root = any_keys;
};
/// The keys are 0..n-1, or nearly so.
struct dense : any_keys
{
};
/// The keys are few compared to their range.
struct sparse : any_keys
{
};

struct moving_refs
{
    using root = moving_refs;
};
/// A reference to an element stays valid until that element is removed (erase, clear).
struct stable_refs : moving_refs
{
};

struct any_layout
{
    using root = any_layout;
};
/// The elements lie next to each other in memory.
struct contiguous : any_layout
{
};

struct any_representation
{
    using root = any_representation;
};
/// Equal contents have equal representations.
struct canonical : any_representation
{
};

struct single_threaded
{
    using root = single_threaded;
};
/// Inserts may run concurrently with each other and with reads of elements already published. Not covered:
/// erase, clear and iteration, which need no other operation running.
struct concurrent : single_threaded
{
};

struct any_memory
{
    using root = any_memory;
};
/// Storage that erase and clear free is kept for reuse: refilling up to an earlier size allocates nothing.
struct retains : any_memory
{
};
/// Erase and clear return the storage of what they remove.
struct reclaims : any_memory
{
};

struct any_clear
{
    using root = any_clear;
};
/// Clear takes time in proportion to the elements present, not to the keys or capacity seen before.
struct proportional_clear : any_clear
{
};
/// Clear takes constant time, as with a generation counter.
struct constant_clear : proportional_clear
{
};

struct any_access
{
    using root = any_access;
};
/// A queue: elements leave in the order they came (front, push, pop).
struct fifo : any_access
{
};
/// A stack: the last element pushed leaves first (top, push, pop).
struct lifo : any_access
{
};
/// A priority queue: the least element by urd::Less leaves first (top, push, pop).
struct by_priority : any_access
{
};

struct any_scope
{
    using root = any_scope;
};
/// Offers push_scope and pop_scope: popping undoes every change since the matching push.
struct scoped : any_scope
{
};

struct any_layering
{
    using root = any_layering;
};
/// Offers layer_over(parent): lookups find the parent's elements too, which it neither owns nor changes.
struct layered : any_layering
{
};

struct any_handle
{
    using root = any_handle;
};
/// A pool: each element is identified by a handle that insert gives, and equal elements get equal handles. Not
/// iterated or counted.
struct handles : any_handle
{
};

struct any_sharing
{
    using root = any_sharing;
};
/// Parts that are equal across elements (rows) are stored once.
struct structural_sharing : any_sharing
{
};

struct any_direction
{
    using root = any_direction;
};
/// A map that answers by value too: keys(v) gives the keys that map to v, as a graph gives a vertex's predecessors.
struct bidirectional : any_direction
{
};

struct any_grouping
{
    using root = any_grouping;
};
/// A partition: its elements fall into disjoint classes; merge joins two classes, find names an element's class.
struct partition : any_grouping
{
};

struct any_tracking
{
    using root = any_tracking;
};
/// Offers added() and removed(): exactly the elements that changed since clear_changes(), for semi-naive evaluation.
struct change_log : any_tracking
{
};

/// Offers no change.
struct immutable
{
    using root = immutable;
};
/// Offers insert.
struct growable : immutable
{
};
/// Offers insert, erase and clear.
struct shrinkable : growable
{
};

// What a Store's interface enforces: no container declares a lifecycle, emptiness or identity. Multiplicity a
// multimap declares (multi), but a request only states it.
struct may_change
{
    using root = may_change;
};
/// Only grows: nothing is ever removed.
struct monotone : may_change
{
};
/// No change after construction.
struct frozen : monotone
{
};

struct maybe_empty
{
    using root = maybe_empty;
};
/// At least one element.
struct nonempty : maybe_empty
{
};

struct any_multiplicity
{
    using root = any_multiplicity;
};
/// At most one value per key (a map).
struct functional : any_multiplicity
{
};
/// Any number of values per key (a multimap).
struct multi : any_multiplicity
{
};

struct by_key
{
    using root = by_key;
};
/// Identified by position 0..n-1, in order (a sequence).
struct positional : by_key
{
};

// How a Store is used: it changes which representation is chosen, never what the Store guarantees. Only a
// request says it; docs/BENCHMARKS.md backs what each one chooses.
struct general
{
    using root = general;
};
/// A few elements, or a few at a time.
struct small : general
{
};

namespace detail
{

/// A list of types: tags, or the roots of their categories.
template<typename... Ts>
struct type_list
{
};

/// The roots of the categories a container declares, then those only a Store's request has.
using guaranteed_roots = type_list<any_order,
                                   duplicates,
                                   any_keys,
                                   moving_refs,
                                   any_layout,
                                   any_representation,
                                   single_threaded,
                                   immutable,
                                   any_memory,
                                   any_clear,
                                   any_access,
                                   any_scope,
                                   any_layering,
                                   any_tracking,
                                   any_handle,
                                   any_sharing,
                                   any_grouping,
                                   any_direction>;
using requested_roots = type_list<may_change, maybe_empty, any_multiplicity, by_key, general>;

template<typename... Lists>
struct concat;
template<typename... As, typename... Bs>
struct concat<type_list<As...>, type_list<Bs...>>
{
    using type = type_list<As..., Bs...>;
};

/// Every category, in the order that makes properties canonical. A new category is a root added here.
using all_roots = typename concat<guaranteed_roots, requested_roots>::type;

/// The root of P's category; void for what is no tag (key<K>, value<V>).
template<typename P>
struct root_of
{
    using type = void;
};
template<typename P>
    requires requires { typename P::root; }
struct root_of<P>
{
    using type = typename P::root;
};
template<typename P>
using root_t = typename root_of<P>::type;

template<typename P, typename List>
inline constexpr bool in_list = false;
template<typename P, typename... Roots>
inline constexpr bool in_list<P, type_list<Roots...>> = (std::is_same_v<root_t<P>, Roots> || ...);

/// How many of Ps answer the category Root.
template<typename Root, typename... Ps>
inline constexpr int count_of = (0 + ... + static_cast<int>(std::is_same_v<root_t<Ps>, Root>));

/// Whether at most one of Ps answers each category of the list.
template<typename List, typename... Ps>
inline constexpr bool one_per_category = false;
template<typename... Roots, typename... Ps>
inline constexpr bool one_per_category<type_list<Roots...>, Ps...> = ((count_of<Roots, Ps...> <= 1) && ...);

// Finding the tag of a category among several is overload resolution, not recursion: a holder derives from one
// slot<Root, Tag> per tag, and found_in<Root> deduces Tag from the one slot whose root is Root, or falls back to
// Root. Each lookup is a deduction, which costs the compiler far less than a chain of class instantiations. The
// calls are qualified: argument-dependent lookup would instantiate the holder's template arguments (containers).

/// A tag in its category's place.
template<typename Root, typename Tag>
struct slot
{
};
template<typename Root, typename Tag>
Tag found_in(const slot<Root, Tag>*);
template<typename Root>
Root found_in(const void*);
/// The tag in the category Root among those Holder derives slots from, else Root.
template<typename Root, typename Holder>
using found_t = decltype(detail::found_in<Root>(static_cast<const Holder*>(nullptr)));

template<typename Root, typename Otherwise, typename Tag>
Tag found_or(const slot<Root, Tag>*);
template<typename Root, typename Otherwise>
Otherwise found_or(const void*);

/// Slots for the tags among Ps, in any order; what is no tag gets a slot no lookup finds.
template<typename... Ps>
struct slots : slot<root_t<Ps>, Ps>...
{
};

}  // namespace detail

/// The properties of something: one tag per category, in the order of all_roots, so that equal properties are
/// one type. Write them with properties_of.
template<typename... Tags>
struct properties : detail::slot<typename Tags::root, Tags>...
{
    using tag_list = detail::type_list<Tags...>;
    static constexpr bool declared = true;
};

namespace detail
{

template<typename Roots, typename Holder>
struct in_root_order;
template<typename... Roots, typename Holder>
struct in_root_order<type_list<Roots...>, Holder>
{
    using type = properties<found_t<Roots, Holder>...>;
};

template<typename Roots, typename Traits, typename Replacing>
struct replaced;
template<typename... Roots, typename Traits, typename Replacing>
struct replaced<type_list<Roots...>, Traits, Replacing>
{
    using type = properties<decltype(detail::found_or<Roots, found_t<Roots, Traits>>(static_cast<const Replacing*>(nullptr)))...>;
};

}  // namespace detail

template<typename Tag>
inline constexpr bool is_guarantee = detail::in_list<Tag, detail::guaranteed_roots>;

template<typename Tag>
inline constexpr bool is_request_only = detail::in_list<Tag, detail::requested_roots>;

/// The properties stated by Tags, in any order, at most one per category; categories no tag answers keep their root.
template<typename... Tags>
using properties_of = typename detail::in_root_order<detail::all_roots, detail::slots<Tags...>>::type;

/// The tag Traits holds in the category rooted at Root.
template<typename Traits, typename Root>
using category_t = detail::found_t<Root, Traits>;

/// Traits with the categories of Tags replaced by them.
template<typename Traits, typename... Tags>
using with_t = typename detail::replaced<detail::all_roots, Traits, detail::slots<Tags...>>::type;

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
    requires requires { typename X::tag_list; }
struct traits_of<X>
{
    using type = X;
};

}  // namespace detail

/// The properties of X: X itself when it lists them (a request's spec), else its container_traits.
template<typename X>
using traits_t = typename detail::traits_of<X>::type;

namespace detail
{

template<typename Have, typename Want, typename Roots>
inline constexpr bool satisfies_all = false;
template<typename Have, typename Want, typename... Roots>
inline constexpr bool satisfies_all<Have, Want, type_list<Roots...>> = (std::is_base_of_v<found_t<Roots, Want>, found_t<Roots, Have>> && ...);

}  // namespace detail

/// Whether X (a container, a Store, or a request's spec) has every property in Tags.
template<typename X, typename... Tags>
    requires(!std::is_void_v<detail::root_t<Tags>> && ...)  // each an urd property
inline constexpr bool has = (std::is_base_of_v<Tags, detail::found_t<detail::root_t<Tags>, traits_t<X>>> && ...);

/// Whether Have provides, in every category a container declares, at least what Want asks for.
template<typename Have, typename Want>
inline constexpr bool satisfies = detail::satisfies_all<Have, Want, detail::guaranteed_roots>;

}  // namespace urd
