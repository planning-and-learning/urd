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

// What a Store can be asked for: element types and the property tags of tags.hpp (workloads such as small
// among them). Each is a type, so they can be listed together, in any order.

namespace detail
{

/// Whether P is a type argument: a template instance naming the slot it fills (key<K>, value<V>, order_by<P>;
/// urd/kinds.hpp).
template<typename P>
concept a_type_argument = requires { typename P::slot; };

template<typename P>
inline constexpr bool is_property = a_tag<P> || a_type_argument<P>;

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

/// A Store's request in canonical form: its kind (urd/kinds.hpp), its type arguments (key<K>, value<V>, as given), and
/// its answers, one per category that says more than none, each in its category's enum, in the order of its kind's
/// request format. Whatever order the properties were listed in, equal requests give equal specs, and so equal Store
/// types; compilers print the kind and the answers by name (urd::order::sorted). Its types are its kind's data, built
/// from its type arguments: a set has keys, a map keys and mapped values, a sequence values; nothing it lacks.
template<typename Kind, typename Arguments, auto... Answers>
struct spec : Kind::template data<Arguments>
{
    /// What the Store is: the kind its tags and type arguments reach (urd/kinds.hpp).
    using kind = Kind;
    /// Its type arguments, as the request gives them (key<K>, value<V>).
    using arguments = Arguments;
    /// What the Store promises: the request's tags, with what they mean (its kind's meaning, urd/kinds.hpp).
    static constexpr detail::property_set urd_properties = detail::property_set::of({ tag(Answers)... });
    static constexpr bool declared = true;

    /// What its container must guarantee: the promise, but for what the Store keeps itself (its kind's asks).
    static constexpr detail::property_set required = Kind::asks(urd_properties);
    /// What it promises its container (its kind's promises): what a container may rely on.
    static constexpr detail::property_set promised = Kind::promises(urd_properties);
    /// Whether its elements form a range urd reads: its kind lists the iteration facet (a queue only visits them).
    static constexpr bool iterable = detail::listed_facet<detail::facet::iteration, detail::facets_of<Kind>>;
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
    auto seen = property_set::roots();
    for (const auto each : listed)
    {
        if (each.in == nullptr)
            continue;
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
template<typename Kind, typename Arguments, auto... As, category_enum E, category_enum... Es, property_set Meant>
struct spec_answers<spec<Kind, Arguments, As...>, request_format<E, Es...>, Meant> :
    spec_answers<std::conditional_t<Meant.template get<E>() == E {}, spec<Kind, Arguments, As...>, spec<Kind, Arguments, As..., Meant.template get<E>()>>,
                 request_format<Es...>,
                 Meant>
{
};

/// The slot a type argument fills, else void.
template<typename P>
struct slot_of
{
    using type = void;
};
template<a_type_argument P>
struct slot_of<P>
{
    using type = typename P::slot;
};

/// The slots Ps fill, as listed; and whether each is filled once.
template<typename... Ps>
consteval slot_set slots_filled()
{
    auto result = slot_set {};
    const auto fill = [&result]<typename P>(std::type_identity<P> /*argument*/)
    {
        if constexpr (a_type_argument<P>)
            result.push(slot_v<typename P::slot>);
    };
    (fill(std::type_identity<Ps> {}), ...);
    return result;
}
consteval bool each_once(const slot_set& filled)
{
    auto seen = slot_set {};
    for (const auto* each : filled)
    {
        if (seen.has(each))
            return false;
        seen.push(each);
    }
    return true;
}

/// The type arguments among Ps, in the order of the slots a kind takes: equal requests give equal specs.
template<typename Slot, typename... Ps>
struct filling
{
    using type = typename join<std::conditional_t<std::is_same_v<typename slot_of<Ps>::type, Slot>, type_list<Ps>, type_list<>>...>::type;
};
template<typename Taken, typename... Ps>
struct arguments_in;
template<typename... Slots, typename... Ps>
struct arguments_in<slot_list<Slots...>, Ps...>
{
    using type = typename join<typename filling<Slots, Ps...>::type...>::type;
};

/// Checks a property list and turns it into its spec. Each static_assert names what is wrong.
template<typename... Ps>
struct to_spec
{
    static_assert((is_property<Ps> && ...),
                  "urd::Store: every argument must be an urd property (key<K>, value<V>, sorted, unique, …, or a "
                  "workload such as small)");
    static constexpr slot_set filled = slots_filled<Ps...>();
    static_assert(each_once(filled), "urd::Store: two type arguments fill the same slot (at most one key<K>, one value<V>, one order_by<P>)");
    static_assert(one_per_category({ tag_of<Ps>()... }),
                  "urd::Store: two properties answer the same question (dense and sparse, frozen and monotone, two "
                  "workloads, …)");

    static_assert(filled.count > 0, "urd::Store: needs a type argument: key<K> (a set or map) or value<V> (a sequence)");

    static constexpr property_set stated = stated_of({ tag_of<Ps>()... });
    using kind = urd::kind_of<request_facts { .stated = stated, .slots = filled }>;
    static_assert(takes_all(typename kind::takes {}, filled),
                  "urd::Store: a type argument means nothing for this kind of Store: each kind takes the type arguments "
                  "of its slots (urd/kinds.hpp, takes): a partition no value<V>, a pool or slots no key<K>");
    /// Its type arguments, in the order of the slots its kind takes.
    using arguments = typename arguments_in<typename kind::takes, Ps...>::type;
    using data = typename kind::template data<arguments>;

    using format = format_of<kind>;
    static_assert(in_format(format {}, stated),
                  "urd::Store: a property means nothing for this kind of Store: each kind answers the questions of its "
                  "request format (urd/kinds.hpp, format)");
    static_assert(well_formed<kind>(request_facts { .stated = stated, .slots = filled, .orderable = orderable<typename data::ordered_type> }),
                  "urd::Store: the request contradicts itself, by its kind's rules (urd/kinds.hpp, well_formed): "
                  "monotone_priority without by_priority, order_by<P> where its kind takes none (a set without sorted), "
                  "or order_by<P> on unique keys that are not orderable");

    static constexpr property_set meant = meaning<kind>(stated);
    using type = typename spec_answers<spec<kind, arguments>, format, meant>::type;
};

}  // namespace detail

}  // namespace urd
