#pragma once

#include "urd/elements.hpp"
#include "urd/kinds.hpp"
#include "urd/property_set.hpp"
#include "urd/traits.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd
{

// What a Store can be asked for: element types and the property tags of tags.hpp (workloads such as small
// among them). Each is a type, so they can be listed together, in any order.

/// The elements are identified by keys of type K. Without value<V>, the keys are the contents: a set.
template<typename K>
struct key
{
    using type = K;
};

/// Each element carries a value of type V. Without key<K>, values are identified by position: a sequence.
template<typename V>
struct value
{
    using type = V;
};

/// Orders a sorted Store, or a by_priority queue, by P(element) (P(key) in a map) instead of the element: P is a member
/// pointer (&Job::deadline) or a stateless lambda ([](const Job& job) { return std::tuple(job.deadline, -job.weight); }). Ties fall back
/// to the element's own order where it has one.
template<auto P>
struct order_by
{
    static constexpr auto projection = P;
};

/// How long each row of a fixed_shape Store is, in order: what such a Store is built from.
struct row_lengths
{
    std::vector<std::size_t> lengths;

    row_lengths(std::initializer_list<std::size_t> each) : lengths(each) {}
    explicit row_lengths(std::vector<std::size_t> each) : lengths(std::move(each)) {}
};

namespace detail
{

template<template<typename> class Template, typename T>
inline constexpr bool is_a = false;
template<template<typename> class Template, typename T>
inline constexpr bool is_a<Template, Template<T>> = true;

/// The type inside the first Template<…> among Ps, else void.
template<template<typename> class Template, typename... Ps>
struct first_of
{
    using type = void;
};
template<template<typename> class Template, typename T, typename... Ps>
struct first_of<Template, Template<T>, Ps...>
{
    using type = T;
};
template<template<typename> class Template, typename P, typename... Ps>
struct first_of<Template, P, Ps...> : first_of<Template, Ps...>
{
};

/// Whether P is a tag a Store may be asked for.
template<typename P>
consteval bool is_tag()
{ return a_tag<P>; }

template<typename P>
inline constexpr bool is_order_by = false;
template<auto P>
inline constexpr bool is_order_by<order_by<P>> = true;

/// The comparator order_by<P> stands for: with one argument, P orders elements T (a map's keys); with two, a map's
/// entries by key K and value V.
template<typename T, auto P>
consteval auto projected_order()
{
    static_assert(std::invocable<decltype(P), const T&>, "urd::Store: order_by<P> takes P(element) (a map's P(key)), or for a map P(key, value); P is neither");
    return std::type_identity<ProjectedLess<T, P>> {};
}
template<typename T, typename K, typename V, auto P>
consteval auto order_from()
{
    if constexpr (!std::is_void_v<K> && !std::is_void_v<V>)
    {
        if constexpr (std::invocable<decltype(P), const K&, const V&>)
            return std::type_identity<EntryLess<K, V, P>> {};
        else
            return projected_order<T, P>();
    }
    else
        return projected_order<T, P>();
}
/// The comparator of the first order_by<P> among Ps, else void.
template<typename T, typename K, typename V, typename... Ps>
struct order_of
{
    using type = void;
};
template<typename T, typename K, typename V, auto P, typename... Ps>
struct order_of<T, K, V, order_by<P>, Ps...>
{
    using type = typename decltype(order_from<T, K, V, P>())::type;
};
template<typename T, typename K, typename V, typename P, typename... Ps>
struct order_of<T, K, V, P, Ps...> : order_of<T, K, V, Ps...>
{
};

template<typename P>
inline constexpr bool is_property = is_tag<P>() || is_a<key, P> || is_a<value, P> || is_order_by<P>;

/// P's number, where P is a tag; tag {} for key<K> and value<V>.
template<typename P>
consteval tag tag_of()
{
    if constexpr (a_tag<P>)
        return P::value;
    else
        return tag {};
}

}  // namespace detail

/// A Store's request in canonical form: its key and value types (void where absent), and its answers, one per category
/// that says more than none, each in its category's enum, in the order of the categories (urd::category). Whatever
/// order the properties were listed in, equal requests give equal specs, and so equal Store types; compilers print the
/// answers by name (urd::order::sorted).
template<typename K, typename V, typename Order, auto... Answers>
struct spec
{
    using key_type = K;
    using value_type = V;
    /// The comparator order_by asked for (urd::ProjectedLess), else void: the elements' own order.
    using order_type = Order;

    /// What the Store promises: the request's tags, with what they mean (meaning, urd/tags.hpp).
    static constexpr detail::property_set urd_properties = detail::property_set::of({ tag(Answers)... });
    static constexpr bool declared = true;

    /// What the Store is (urd/kinds.hpp): the kind that claims its key and value types and the tags it states.
    using kind = urd::kind_of<urd_properties, !std::is_void_v<K>, !std::is_void_v<V>>;
    /// What its container must guarantee: the promise, but for what the Store keeps itself (asked_of_container).
    static constexpr detail::property_set required = urd::asked_of_container(urd_properties);
    /// What it promises its container (promised_to_container): what a container may rely on.
    static constexpr detail::property_set promised = urd::promised_to_container(urd_properties);
    /// Whether its elements can be visited in turn: as its kind says (a queue is reached by its front or top).
    static constexpr bool iterable = kind::iterable;

    /// What a key maps to, where its kind maps keys to values; else void.
    using mapped_type = typename kind::template mapped_type<K, V>;
    /// What a Store's elements are, by its kind: keys in a set, values in a sequence, key-value pairs in a map.
    using element_type = typename kind::template element_type<K, V>;
};

namespace detail
{

/// The tags of a list (key<K> and value<V> skipped), as stated.
consteval property_set stated_of(std::initializer_list<tag> listed)
{
    auto result = property_set::roots();
    for (const auto each : listed)
        if (each != tag {})
            result.put(each);
    return result;
}

/// Whether at most one listed tag answers each category.
consteval bool one_per_category(std::initializer_list<tag> listed)
{
    for (const auto* each = listed.begin(); each != listed.end(); ++each)
        for (const auto* other = listed.begin(); other != each; ++other)
            if (*each != tag {} && other->in == each->in)
                return false;
    return true;
}

/// Whether every tag of `stated` answers a category of Format.
template<category_enum... Es>
consteval bool in_format(request_format<Es...> /*format*/, const property_set& stated)
{
    for (std::size_t each = 0; each < stated.count; ++each)
        if (((stated.held[each].in != urd::category<Es>) && ...))
            return false;
    return true;
}

/// The spec whose answers are those of Meant beyond none, in the order of Format: S with them appended.
template<typename S, typename Format, property_set Meant>
struct spec_answers
{
    using type = S;
};
template<typename K, typename V, typename Order, auto... As, category_enum E, category_enum... Es, property_set Meant>
struct spec_answers<spec<K, V, Order, As...>, request_format<E, Es...>, Meant>
    : spec_answers<std::conditional_t<Meant.template get<E>() == E {}, spec<K, V, Order, As...>, spec<K, V, Order, As..., Meant.template get<E>()>>,
                   request_format<Es...>,
                   Meant>
{
};

/// Checks a property list and turns it into its spec. Each static_assert names what is wrong.
template<typename... Ps>
struct to_spec
{
    static_assert((is_property<Ps> && ...),
                  "urd::Store: every argument must be an urd property (key<K>, value<V>, sorted, unique, …, or a "
                  "workload such as small)");
    static_assert((0 + ... + static_cast<int>(is_a<key, Ps>)) <= 1, "urd::Store: at most one key<K>");
    static_assert((0 + ... + static_cast<int>(is_a<value, Ps>)) <= 1, "urd::Store: at most one value<V>");
    static_assert(one_per_category({ tag_of<Ps>()... }),
                  "urd::Store: two properties answer the same question (dense and sparse, frozen and monotone, two "
                  "workloads, …)");

    using key_type = typename first_of<key, Ps...>::type;
    using value_type = typename first_of<value, Ps...>::type;
    static_assert(!std::is_void_v<key_type> || !std::is_void_v<value_type>, "urd::Store: needs key<K> (a set or map) or value<V> (a sequence)");

    static constexpr property_set stated = stated_of({ tag_of<Ps>()... });
    static constexpr bool keyed = !std::is_void_v<key_type>;
    static constexpr bool valued = !std::is_void_v<value_type>;
    using kind = urd::kind_of<stated, keyed, valued>;

    /// What order_by orders: keys, or a sequence's or queue's values.
    using ordered_type = std::conditional_t<keyed, key_type, value_type>;
    static constexpr bool ordered_by = (is_order_by<Ps> || ...);
    static_assert((0 + ... + static_cast<int>(is_order_by<Ps>)) <= 1, "urd::Store: at most one order_by<P>");
    using order_type = typename order_of<ordered_type, key_type, value_type, Ps...>::type;

    static_assert(in_format(typename kind::format {}, stated),
                  "urd::Store: a property means nothing for this kind of Store: each kind answers the questions of its "
                  "request format (urd/kinds.hpp, format)");
    static_assert(well_formed(stated, keyed, valued, ordered_by, orderable<ordered_type>),
                  "urd::Store: the request contradicts itself: a partition with value<V>, handles with key<K>, "
                  "monotone_priority without by_priority, order_by<P> without sorted or by_priority, or order_by<P> on "
                  "unique keys that are not orderable (urd/tags.hpp, well_formed)");

    static constexpr property_set meant = meaning<kind>(stated);
    using type = typename spec_answers<spec<key_type, value_type, order_type>, typename kind::format, meant>::type;
};

}  // namespace detail

}  // namespace urd
