#pragma once

// The kinds of Store: the data urd's tree walks (urd/tree.hpp). A kind is a struct: its name, which requests it claims,
// the type arguments it takes, the questions its requests answer (format), its Store's interface (facets), its data
// (its element types, built from its type arguments), what a request of it means, and its rules. It derives from
// detail::any_kind, which says what every Store is, and states only what differs; generic code reads the members and
// makes no choice. A kind added outside urd is a struct like these, listed by specializing urd::extra_kinds; its
// containers by specializing urd::detail::declarations_of (urd/registry/declarations.hpp).

#include "urd/detail/type_list.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/property_set.hpp"
#include "urd/registry/facts.hpp"
#include "urd/tags.hpp"
#include "urd/tree.hpp"

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace urd
{

using detail::property_set;

// The type arguments a Store is written with, beside its tags: each fills a slot (what the type is to the Store), and
// a kind takes some slots (takes) and reads its data from them. Slots are open, as categories, traits and roles are:
// a slot is a struct with its words, identified by them; a type argument is a template naming its slot.

/// A slot's words, and the slot Slot (a struct with its name and description).
using slot_info = words<struct slot_words>;
template<typename Slot>
inline constexpr const slot_info* slot_v = &words_of<struct slot_words, Slot>;

/// The keys that identify elements.
struct key_slot
{
    static constexpr std::string_view name = "key";
    static constexpr std::string_view description = "the keys that identify elements";
};
/// The values elements carry.
struct value_slot
{
    static constexpr std::string_view name = "value";
    static constexpr std::string_view description = "the values elements carry";
};

/// The elements are identified by keys of type K. Without value<V>, the keys are the contents: a set.
template<typename K>
struct key
{
    using type = K;
    using slot = key_slot;
};

/// Each element carries a value of type V. Without key<K>, values are identified by position: a sequence.
template<typename V>
struct value
{
    using type = V;
    using slot = value_slot;
};

/// What orders a sorted Store or a priority queue, instead of its elements' own order.
struct order_slot
{
    static constexpr std::string_view name = "order_by";
    static constexpr std::string_view description = "what orders the elements: P(element), or in a map P(key) or P(key, value)";
};

/// Orders a sorted Store, or a by_priority queue, by P(element) (P(key) in a map) instead of the element: P is a member
/// pointer (&Job::deadline) or a stateless lambda ([](const Job& job) { return std::tuple(job.deadline, -job.weight); }).
/// Ties fall back to the element's own order where it has one.
template<auto P>
struct order_by
{
    static constexpr auto projection = P;
    using slot = order_slot;
};

/// A preference: where a request holds `when`, containers holding `later` come after the others that serve it.
struct preference
{
    tag when = tag {};
    tag later = tag {};
};

namespace detail
{

/// The most slots one request fills.
inline constexpr std::size_t most_slots = 8;

/// The slots a request's type arguments fill.
struct slot_set : fixed_list<const slot_info*, most_slots>
{
    [[nodiscard]] constexpr bool has(const slot_info* of) const { return std::ranges::find(*this, of) != end(); }
    template<typename Slot>
    [[nodiscard]] constexpr bool has() const
    { return has(slot_v<Slot>); }
};

/// What a kind's claims and rules read of a request: the tags it states, the slots its type arguments fill, and for
/// the rules, whether what it orders is orderable by itself.
struct request_facts
{
    property_set stated = property_set::roots();
    slot_set slots {};
    bool orderable = false;
};

/// The types the type arguments among Args carry (key<K>: K), in order: what a Store's elements are made of. An
/// argument that carries a value (order_by<P>) carries no type.
template<typename Argument>
struct carried
{
    using type = type_list<>;
};
template<typename Argument>
    requires requires { typename Argument::type; }
struct carried<Argument>
{
    using type = type_list<typename Argument::type>;
};
template<typename Args>
struct argument_types;
template<typename... Arguments>
struct argument_types<type_list<Arguments...>>
{
    using type = typename join<typename carried<Arguments>::type...>::type;
};

/// A list of slots: what a kind takes.
template<typename... Slots>
struct slot_list
{
};

/// A list of facets (urd/store/): what a kind's Store offers, each where its condition holds.
template<template<typename, typename, typename> class... Facets>
struct facet_list
{
};
/// Whether F is among the facets of a list.
template<template<typename, typename, typename> class F, typename List>
inline constexpr bool listed_facet = false;
template<template<typename, typename, typename> class F, template<typename, typename, typename> class... Fs>
inline constexpr bool listed_facet<F, facet_list<Fs...>> = (std::is_same_v<facet_list<F>, facet_list<Fs>> || ...);
template<template<typename, typename, typename> class... As, template<typename, typename, typename> class... Bs, typename... Rest>
struct concat<facet_list<As...>, facet_list<Bs...>, Rest...> : concat<facet_list<As..., Bs...>, Rest...>
{
};

/// The categories a kind's requests answer (urd/tags.hpp): its request format.
template<typename... Categories>
struct request_format
{
};

/// The facets urd's kinds offer, defined in urd/store/<facet>.hpp.
namespace facet
{
template<typename Derived, typename R, typename S>
class sizing;
template<typename Derived, typename R, typename S>
class iteration;
template<typename Derived, typename R, typename S>
class visiting;
template<typename Derived, typename R, typename S>
class equality;
template<typename Derived, typename R, typename S>
class ordering;
template<typename Derived, typename R, typename S>
class lookup;
template<typename Derived, typename R, typename S>
class ordered_lookup;
template<typename Derived, typename R, typename S>
class reverse_iteration;
template<typename Derived, typename R, typename S>
class positions;
template<typename Derived, typename R, typename S>
class insertion_positions;
template<typename Derived, typename R, typename S>
class contiguous_data;
template<typename Derived, typename R, typename S>
class capacity;
template<typename Derived, typename R, typename S>
class growth;
template<typename Derived, typename R, typename S>
class removal;
template<typename Derived, typename R, typename S>
class values_by_key;
template<typename Derived, typename R, typename S>
class multi_values;
template<typename Derived, typename R, typename S>
class set_algebra;
template<typename Derived, typename R, typename S>
class queue_access;
template<typename Derived, typename R, typename S>
class scopes;
template<typename Derived, typename R, typename S>
class layers;
template<typename Derived, typename R, typename S>
class changes;
template<typename Derived, typename R, typename S>
class pool;
template<typename Derived, typename R, typename S>
class records;
template<typename Derived, typename R, typename S>
class slots;
template<typename Derived, typename R, typename S>
class grouping;
template<typename Derived, typename R, typename S>
class entries;
template<typename Derived, typename R, typename S>
class inverse;
template<typename Derived, typename R, typename S>
class shape;
template<typename Derived, typename R, typename S>
class sorted_ends;
}  // namespace facet

/// Which type plays which role in a request's data.
template<typename Role, typename T>
struct plays
{
    using role = Role;
    using type = T;
};
template<typename... Plays>
struct role_types
{
};

/// order_by<P> on keys that are each once breaks ties by the keys' own order, so they must be orderable (a rule of
/// the kinds with keys).
constexpr bool orderable_keys(const request_facts& request)
{ return !request.slots.has<order_slot>() || request.stated.has(multiplicity::multi) || request.orderable; }

/// The order_by<P> among Args, else void.
template<typename T>
inline constexpr bool is_order_by = false;
template<auto P>
inline constexpr bool is_order_by<order_by<P>> = true;
template<typename Args>
struct order_argument
{
    using type = void;
};
template<typename P, typename... Ps>
struct order_argument<type_list<P, Ps...>> : std::conditional_t<is_order_by<P>, std::type_identity<P>, order_argument<type_list<Ps...>>>
{
};

/// The comparator Order (an order_by<P>, or void) makes over Ordered: urd::ProjectedLess, or void for its own order.
template<typename Ordered, typename Order>
consteval auto projection_over()
{
    if constexpr (std::is_void_v<Order>)
        return std::type_identity<void> {};
    else
    {
        static_assert(std::invocable<decltype(Order::projection), const Ordered&>,
                      "urd::Store: order_by<P> takes P(element) (a map's P(key)), or for a map P(key, value); P is neither");
        return std::type_identity<ProjectedLess<Ordered, Order::projection>> {};
    }
}
/// What orders a Store of one ordered type: order_by<P> among Args over it, else its own order (void).
template<typename Ordered, typename Args>
using projected_order_t = typename decltype(projection_over<Ordered, typename order_argument<Args>::type>())::type;

/// What orders a map: order_by<P> among Args over its entries where P takes a key and a value, else over its keys.
template<typename K, typename V, typename Args>
consteval auto map_order()
{
    using order = typename order_argument<Args>::type;
    if constexpr (std::is_void_v<order>)
        return std::type_identity<void> {};
    else if constexpr (std::invocable<decltype(order::projection), const K&, const V&>)
        return std::type_identity<EntryLess<K, V, order::projection>> {};
    else
        return projection_over<K, order>();
}

/// A Store of values (sequence, multiset, queues, pool, slots), from value<V> among Args: V, found by value.
template<typename Args>
struct value_data
{
    using value_type = typename type_in<urd::value, Args>::type;
    using element_type = value_type;
    /// What may be a Store (a row).
    using held_type = value_type;
    /// What find takes, where the kind has lookup.
    using lookup_type = value_type;
    /// What its order compares.
    using ordered_type = value_type;
    using played = role_types<plays<role::element, value_type>>;
    /// What orders it: order_by<P> over its values, else their own order (void).
    using order_type = projected_order_t<value_type, Args>;
};

/// Keys, by value: a set's and a partition's data, from key<K> among Args.
template<typename Args>
struct key_data
{
    using key_type = typename type_in<urd::key, Args>::type;
    using element_type = key_type;
    using held_type = key_type;
    using lookup_type = key_type;
    using ordered_type = key_type;
    using played = role_types<plays<role::element, key_type>>;
    using order_type = projected_order_t<key_type, Args>;
};

/// A value per key: a map's data, from key<K> and value<V> among Args.
template<typename Args>
struct map_data
{
    using key_type = typename type_in<urd::key, Args>::type;
    using mapped_type = typename type_in<urd::value, Args>::type;
    using element_type = std::pair<const key_type, mapped_type>;
    using held_type = mapped_type;
    using lookup_type = key_type;
    using ordered_type = key_type;
    using played = role_types<plays<role::element, key_type>, plays<role::mapped, mapped_type>>;
    using order_type = typename decltype(map_order<key_type, mapped_type, Args>())::type;
};

/// What every Store is, and what a kind is unless it says otherwise. A kind derives from it and restates what
/// differs: the type arguments it takes, its own questions (asked before the common ones), its own facets (after the
/// common ones), its data, what a request means, its rules.
struct any_kind
{
    /// The questions every kind's requests answer: how it stores, what it costs, how it is used.
    using common_format = request_format<referencing,
                                         layout,
                                         representation,
                                         threading,
                                         memory,
                                         clearing,
                                         scoping,
                                         layering,
                                         sharing,
                                         tracking,
                                         change,
                                         emptiness,
                                         position,
                                         workload>;
    /// The facets every kind's Store has, each where its condition holds.
    using common_facets = facet_list<facet::sizing,
                                     facet::equality,
                                     facet::ordering,
                                     facet::ordered_lookup,
                                     facet::contiguous_data,
                                     facet::capacity,
                                     facet::scopes,
                                     facet::layers,
                                     facet::changes,
                                     facet::shape>;

    using takes = slot_list<>;
    using format = request_format<>;
    using facets = facet_list<>;
    template<typename Args>
    using data = value_data<Args>;

    /// What a request means beyond what it states: once nothing changes (frozen), nothing moves.
    static constexpr property_set meaning(property_set stated)
    {
        if (stated.has(change::frozen))
            stated.put(referencing::stable_refs);
        return stated;
    }
    /// What it asks of its container: what it means, but for what the Store keeps itself: once nothing changes
    /// (frozen), nothing moves, so stable references need nothing of the container.
    static constexpr property_set asks(property_set meant)
    {
        if (meant.has(change::frozen))
            meant.put(referencing::none);
        return meant;
    }
    /// What it promises its container: what it means, and that a Store which must stay nonempty never erases, so a
    /// container that only grows serves it.
    static constexpr property_set promises(property_set meant)
    {
        if (meant.has(emptiness::nonempty) && !meant.has(change::monotone))
            meant.put(change::monotone);
        return meant;
    }
    /// Which containers come after the others that serve it. Contiguous storage is more compact and faster to search
    /// and iterate where elements are few or never change, so stable storage (a node per element, segments that never
    /// move) comes last for them (docs/BENCHMARKS.md).
    static constexpr std::array prefers { preference { .when = change::frozen, .later = referencing::stable_refs },
                                          preference { .when = workload::small, .later = referencing::stable_refs } };
    static constexpr bool well_formed(const request_facts& /*request*/) { return true; }
    static constexpr bool accepts_order_by(const property_set& stated) { return stated.has(order::sorted); }
    static constexpr bool changes_in_place(const property_set& /*row*/) { return true; }
    /// What a Store of it is built from besides its elements (spec S): nothing.
    template<typename S, typename From>
    static constexpr bool builds_from = false;
};

/// What both kinds of queue share: values, reached by their front or top, visited in no order urd reads.
struct queue_base : any_kind
{
    using takes = slot_list<value_slot>;
    using facets = facet_list<facet::visiting, facet::positions, facet::queue_access>;
};

}  // namespace detail

/// Keys in disjoint classes, reached by merge and find.
struct partition_kind : detail::any_kind
{
    static constexpr std::string_view name = "partition";
    static constexpr bool claims(const detail::request_facts& request) { return request.stated.has(grouping::partition); }
    using takes = detail::slot_list<key_slot, order_slot>;
    using format = detail::request_format<order, uniqueness, keys, grouping>;
    using facets = detail::facet_list<detail::facet::positions, detail::facet::grouping>;
    template<typename Args>
    using data = detail::key_data<Args>;
    static constexpr bool well_formed(const detail::request_facts& request) { return detail::orderable_keys(request); }
};

/// Values reached by the order they arrived in: first in first out (fifo), or last in first out (lifo).
struct arrival_queue_kind : detail::queue_base
{
    static constexpr std::string_view name = "queue/arrival";
    static constexpr bool claims(const detail::request_facts& request) { return request.stated.has(access::fifo) || request.stated.has(access::lifo); }
    using format = detail::request_format<uniqueness, access, handling>;
};

/// Values reached by their priority: the top is the greatest by the order (order_by<P>, else the values' own).
struct priority_queue_kind : detail::queue_base
{
    static constexpr std::string_view name = "queue/priority";
    static constexpr bool claims(const detail::request_facts& request)
    { return request.stated.has(access::by_priority) || request.stated.has(priority::monotone_priority); }
    using takes = detail::slot_list<value_slot, order_slot>;
    using format = detail::request_format<order, uniqueness, access, handling, priority>;
    /// monotone_priority is a promise of a queue reached by priority.
    static constexpr bool well_formed(const detail::request_facts& request)
    { return !request.stated.has(priority::monotone_priority) || request.stated.has(access::by_priority); }
    static constexpr bool accepts_order_by(const property_set& /*stated*/) { return true; }
};

/// Values reached by their front or top: by arrival or by priority.
struct queue_kind
{
    static constexpr std::string_view name = "queue";
    using children = detail::type_list<priority_queue_kind, arrival_queue_kind>;
};

/// Values interned by handle, each distinct one once: rows (a range each, by row) or records and values (one each,
/// by index; records copied as bytes can be stored flat).
struct pool_kind : detail::any_kind
{
    static constexpr std::string_view name = "pool";
    static constexpr bool claims(const detail::request_facts& request)
    { return request.stated.has(handling::handles) && request.stated.has(uniqueness::unique); }
    using takes = detail::slot_list<value_slot, order_slot>;
    using format = detail::request_format<order, uniqueness, keys, handling, packing>;
    using facets = detail::facet_list<detail::facet::positions, detail::facet::records, detail::facet::pool>;
};

/// Values in slots, by handle.
struct slots_kind : detail::any_kind
{
    static constexpr std::string_view name = "slots";
    static constexpr bool claims(const detail::request_facts& request) { return request.stated.has(handling::handles); }
    using takes = detail::slot_list<value_slot, order_slot>;
    using format = detail::request_format<order, uniqueness, keys, handling>;
    using facets = detail::facet_list<detail::facet::positions, detail::facet::slots>;
};

/// A value per key, or several (multi): without multi, each key once.
struct map_kind : detail::any_kind
{
    static constexpr std::string_view name = "map";
    static constexpr bool claims(const detail::request_facts& request) { return request.slots.has<key_slot>() && request.slots.has<value_slot>(); }
    using takes = detail::slot_list<key_slot, value_slot, order_slot>;
    using format = detail::request_format<order, uniqueness, keys, direction, multiplicity>;
    using facets = detail::facet_list<detail::facet::iteration,
                                      detail::facet::lookup,
                                      detail::facet::reverse_iteration,
                                      detail::facet::positions,
                                      detail::facet::growth,
                                      detail::facet::removal,
                                      detail::facet::values_by_key,
                                      detail::facet::multi_values,
                                      detail::facet::entries,
                                      detail::facet::inverse,
                                      detail::facet::sorted_ends>;
    template<typename Args>
    using data = detail::map_data<Args>;
    /// Without multi, each key once.
    static constexpr property_set meaning(property_set stated)
    {
        if (!stated.has(multiplicity::multi))
            stated.put(uniqueness::unique);
        return any_kind::meaning(stated);
    }
    static constexpr bool well_formed(const detail::request_facts& request) { return detail::orderable_keys(request); }
    static constexpr bool changes_in_place(const property_set& /*row*/) { return false; }
};

/// Keys, each once; with positions it numbers its keys as they come (an interner), unless it asks for another order.
struct set_kind : detail::any_kind
{
    static constexpr std::string_view name = "set";
    static constexpr bool claims(const detail::request_facts& request) { return request.slots.has<key_slot>(); }
    using takes = detail::slot_list<key_slot, order_slot>;
    using format = detail::request_format<order, uniqueness, keys>;
    using facets = detail::facet_list<detail::facet::iteration,
                                      detail::facet::lookup,
                                      detail::facet::reverse_iteration,
                                      detail::facet::insertion_positions,
                                      detail::facet::growth,
                                      detail::facet::removal,
                                      detail::facet::set_algebra,
                                      detail::facet::sorted_ends>;
    template<typename Args>
    using data = detail::key_data<Args>;
    /// Each key once; with positions and no order, numbered as they come.
    static constexpr property_set meaning(property_set stated)
    {
        stated.put(uniqueness::unique);
        if (stated.has(position::positional) && stated.get<order>() == order::none)
            stated.put(order::insertion_ordered);
        return any_kind::meaning(stated);
    }
    static constexpr bool well_formed(const detail::request_facts& request) { return detail::orderable_keys(request); }
    /// A set of dense keys is bits, set and cleared in place.
    static constexpr bool changes_in_place(const property_set& row) { return row.has(uniqueness::unique) && row.has(keys::dense); }
};

/// Values kept by value, in order, duplicates kept unless unique.
struct multiset_kind : detail::any_kind
{
    static constexpr std::string_view name = "multiset";
    static constexpr bool claims(const detail::request_facts& request)
    { return request.stated.has(order::sorted) || request.stated.has(uniqueness::unique) || request.stated.has(multiplicity::multi); }
    using takes = detail::slot_list<value_slot, order_slot>;
    using format = detail::request_format<order, uniqueness, keys, multiplicity>;
    using facets = detail::facet_list<detail::facet::iteration,
                                      detail::facet::lookup,
                                      detail::facet::reverse_iteration,
                                      detail::facet::positions,
                                      detail::facet::growth,
                                      detail::facet::removal>;
    /// Its elements have positions.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(position::positional)); }
    static constexpr bool changes_in_place(const property_set& /*row*/) { return false; }
};

/// Values by position.
struct sequence_kind : detail::any_kind
{
    static constexpr std::string_view name = "sequence";
    static constexpr bool claims(const detail::request_facts& /*request*/) { return true; }
    using takes = detail::slot_list<value_slot, order_slot>;
    using format = detail::request_format<order, keys, shape>;
    using facets = detail::facet_list<detail::facet::iteration,
                                      detail::facet::lookup,
                                      detail::facet::reverse_iteration,
                                      detail::facet::positions,
                                      detail::facet::growth,
                                      detail::facet::removal>;
    /// Its elements have positions.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(position::positional)); }
    /// Rows of a fixed shape are built from their lengths (urd::row_lengths), or for rows of rows, each row's shape.
    template<typename S, typename From>
    static constexpr bool builds_from = S::urd_properties.has(shape::fixed_shape);
};

/// Kinds added outside urd, tried before urd's own: specialize extra_kinds<void> with `using type =
/// detail::type_list<my_kind, …>;` before the first Store is named.
template<typename = void>
struct extra_kinds
{
    using type = detail::type_list<>;
};

/// The root of the tree: the kinds, in the order they claim a request (the first that claims it is its kind: a
/// priority queue with handles is a queue; the last claims every request). A template, so that kinds added outside
/// urd are read where a request is resolved, not here.
template<typename Unused = void>
struct store_root
{
    static constexpr std::string_view name = "store";
    using children =
        typename detail::join<typename extra_kinds<Unused>::type,
                              detail::type_list<partition_kind, queue_kind, pool_kind, slots_kind, map_kind, set_kind, multiset_kind, sequence_kind>>::type;
};

/// The kinds at the top of the tree.
template<typename Unused = void>
using kinds = typename store_root<Unused>::children;

namespace detail
{

/// The questions a request of Kind answers (its own, then every kind's), and its Store's facets (every kind's, then
/// its own).
template<typename Kind>
using format_of = typename concat<typename Kind::format, typename any_kind::common_format>::type;
template<typename Kind>
using facets_of = typename concat<typename any_kind::common_facets, typename Kind::facets>::type;

/// The root, named where it is used (dependent on T, so read when a request is resolved, not before).
template<typename T>
using root_for = store_root<std::conditional_t<std::is_void_v<T>, void, void>>;

template<typename Path>
struct leaf_of;
template<typename... Nodes>
struct leaf_of<type_list<Nodes...>> : last<Nodes...>
{
};

/// The leaf a request reaches from the root. A struct, not an alias over the walk: GCC 13 fails on those internally.
template<request_facts Facts>
struct claimed
{
    using type = typename leaf_of<typename path_from<root_for<decltype(Facts)>, Facts>::type>::type;
};

}  // namespace detail

/// The kind of a request: from the root, the first kind that claims it, and below it the first child that claims it,
/// down to a leaf.
template<detail::request_facts Facts>
using kind_of = typename detail::claimed<Facts>::type;

/// The leaves of the tree of kinds: what a Store's kind can be.
template<typename Unused = void>
using leaf_kinds = typename detail::leaves_below<store_root<Unused>>::type;

/// Whether Kind is Node or a kind below it (a priority queue is a queue).
template<typename Kind, typename Node>
concept kind_below = detail::below<Kind, Node>;

/// Whether a request of kind Kind contradicts itself, whatever container would hold it: by its kind's rule, and
/// order_by<P> where its kind takes none. Anything else no container serves fails in the registry.
template<typename Kind>
constexpr bool well_formed(const detail::request_facts& request)
{ return Kind::well_formed(request) && (!request.slots.has<order_slot>() || Kind::accepts_order_by(request.stated)); }

/// Whether the slots a request fills are all among those its kind takes.
template<typename... Slots>
constexpr bool takes_all(detail::slot_list<Slots...> /*taken*/, const detail::slot_set& filled)
{
    return std::ranges::all_of(filled, []([[maybe_unused]] const slot_info* each) { return ((each == slot_v<Slots>) || ...); });
}

/// What a request of kind Kind means beyond what it states: what its kind keeps (a set keeps each key once; a map
/// without multi too; a sequence's elements have positions), and what its tags imply.
template<typename Kind>
constexpr property_set meaning(property_set stated)
{ return Kind::meaning(stated); }

}  // namespace urd
