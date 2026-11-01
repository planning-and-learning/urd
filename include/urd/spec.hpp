#pragma once

#include "urd/elements/requirements.hpp"
#include "urd/kinds.hpp"
#include "urd/property_set.hpp"
#include "urd/traits.hpp"

#include <algorithm>
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

// What a Store is asked for: its kind with its element types (set<K>, map<K, V>, …; urd/kinds.hpp), then tags
// (urd/tags.hpp), workloads such as small and order_by<C> among them, in any order.

namespace detail
{

/// Whether K is a kind as a request names it: a kind with its element types (urd/kinds.hpp).
template<typename K>
concept a_kind = std::derived_from<K, any_kind> && requires {
    typename K::kind;
    typename K::element_types;
};

/// The first of Tags answering the category whose root is Root (order_by<C>: any_comparison), else void.
template<typename Root, typename... Tags>
struct tag_in
{
    using type = void;
};
template<typename Root, typename Tag, typename... Tags>
struct tag_in<Root, Tag, Tags...> : std::conditional_t<std::is_same_v<typename Tag::root, Root>, std::type_identity<Tag>, tag_in<Root, Tags...>>
{
};

}  // namespace detail

namespace detail
{

/// The comparator an order_by<C> gives (Order), else urd::Less over Ordered (Order is void).
template<typename Order, typename Ordered>
struct comparator_of
{
    using type = typename Order::compare;
};
template<typename Ordered>
struct comparator_of<void, Ordered>
{
    using type = Less<Ordered>;
};

/// Whether order_by's comparator (Order, or void for none) compares two of Ordered.
template<typename Order, typename Ordered>
consteval bool compares()
{
    if constexpr (std::is_void_v<Order>)
        return true;
    else
        return std::strict_weak_order<typename Order::compare, const Ordered&, const Ordered&>;
}

}  // namespace detail

/// A Store's request in canonical form: its kind with its element types (set<K>, map<K, V>, …), what orders it (an
/// order_by<C>, or void), and its answers, one per category that says more than none, each in its category's enum, in
/// the order of its kind's request format. Whatever order the tags were listed in, equal requests give equal specs, and
/// so equal Store types; compilers print the kind and the answers by name (urd::order::sorted). Its types are its
/// kind's: a set has keys, a map keys and mapped values, a sequence values; nothing it lacks.
template<typename Kind, typename Order, auto... Answers>
struct spec : Kind
{
    /// What the Store is (urd/kinds.hpp).
    using kind = typename Kind::kind;
    /// What orders it: the comparator its order_by<C> gives, else urd::Less over what it orders.
    using order_type = typename detail::comparator_of<Order, typename Kind::ordered_type>::type;
    /// What the Store promises: the request's tags, with what they mean (its kind's meaning, urd/kinds.hpp).
    static constexpr detail::property_set urd_properties = detail::property_set::of({ tag(Answers)... });
    static constexpr bool declared = true;

    /// What its container must guarantee: the promise, but for what the Store keeps itself (its kind's asks).
    static constexpr detail::property_set required = kind::asks(urd_properties);
    /// What it promises its container (its kind's promises): what a container may rely on.
    static constexpr detail::property_set promised = kind::promises(urd_properties);
    /// Whether its elements form a range urd reads: its kind lists the iteration facet (a queue only visits them).
    static constexpr bool iterable = detail::listed_facet<detail::facet::iteration, detail::facets_of<kind>>;
};

namespace detail
{

/// The tags of a list, as stated.
consteval property_set stated_of(std::initializer_list<tag> listed)
{
    auto result = property_set::roots();
    for (const auto each : listed)
        result.put(each);
    return result;
}

/// Whether at most one listed tag answers each category.
consteval bool one_per_category(std::initializer_list<tag> listed)
{
    auto seen = property_set::roots();
    for (const auto each : listed)
    {
        if (std::ranges::find(seen, each.in, &tag::in) != seen.end())
            return false;
        seen.push(each);
    }
    return true;
}

/// Whether every tag of `stated` answers a category of Format.
template<category_enum... Es>
consteval bool in_format(request_format<Es...> /*format*/, const property_set& stated)
{
    return std::ranges::all_of(stated, [](tag each) { return ((each.in == urd::category<Es>) || ...); });
}

/// The spec whose answers are those of Meant beyond none, in the order of Format: S with them appended.
template<typename S, typename Format, property_set Meant>
struct spec_answers
{
    using type = S;
};
template<typename Kind, typename Order, auto... As, category_enum E, category_enum... Es, property_set Meant>
struct spec_answers<spec<Kind, Order, As...>, request_format<E, Es...>, Meant> :
    spec_answers<std::conditional_t<Meant.template get<E>() == E {}, spec<Kind, Order, As...>, spec<Kind, Order, As..., Meant.template get<E>()>>,
                 request_format<Es...>,
                 Meant>
{
};

/// Checks a request, its kind and its tags, and turns it into its spec. Each static_assert names what is wrong.
template<typename Kind, typename... Tags>
struct to_spec
{
    static_assert(a_kind<Kind>,
                  "urd::Store: the first argument is the kind, with its element types: set<K>, map<K, V>, multiset<V>, "
                  "sequence<V>, queue<V>, pool<V>, slots<V> or partition<K> (urd/kinds.hpp)");
    static_assert((a_tag<Tags> && ...),
                  "urd::Store: after the kind, every argument must be an urd tag (sorted, unique, …, order_by<C>, or a "
                  "workload such as small)");
    static_assert(one_per_category({ Tags::value... }),
                  "urd::Store: two properties answer the same question (dense and sparse, frozen and monotone, two "
                  "workloads, …)");

    static constexpr property_set stated = stated_of({ Tags::value... });
    using kind = typename Kind::kind;
    /// What the request orders by: its order_by<C>, or void.
    using order = typename tag_in<any_comparison, Tags...>::type;

    static_assert(compares<order, typename Kind::ordered_type>(),
                  "urd::Store: order_by<C>: C compares two elements (in a map, two keys), as std::set's comparator does; "
                  "urd::by<P> compares by P(element)");

    using format = format_of<kind>;
    static_assert(in_format(format {}, stated),
                  "urd::Store: a property means nothing for this kind of Store: each kind answers the questions of its "
                  "request format (urd/kinds.hpp, format)");
    static_assert(well_formed<kind>(stated),
                  "urd::Store: the request contradicts itself, by its kind's rules (urd/kinds.hpp, well_formed): a "
                  "queue reached neither by fifo, lifo nor by_priority, monotone_priority without by_priority, or "
                  "order_by<C> where its kind orders by nothing asked for (a set without sorted)");

    static constexpr property_set meant = meaning<kind>(stated);
    using type = typename spec_answers<spec<Kind, order>, format, meant>::type;
};

}  // namespace detail

}  // namespace urd
