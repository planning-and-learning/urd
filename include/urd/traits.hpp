#pragma once

#include <algorithm>
#include <concepts>
#include <ranges>
#include <type_traits>
#include <vector>

namespace urd
{

// Properties, in the style of Boost.Graph's graph_traits: each answers one question (a category) with a tag
// type, and a tag derived from another means "at least that". The root of a category promises nothing.

// What a container guarantees by itself.
struct any_order {};
struct sorted : any_order {};  ///< ascending by operator<

struct duplicates {};
struct unique : duplicates {};  ///< no two elements are equal

struct any_keys {};
struct dense : any_keys {};   ///< the keys are 0..n-1, or nearly so
struct sparse : any_keys {};  ///< the keys are few compared to their range

struct moving_refs {};
struct stable_refs : moving_refs {};  ///< a reference to an element stays valid until that element is removed (erase, clear)

struct any_layout {};
struct contiguous : any_layout {};  ///< the elements lie next to each other in memory

struct any_representation {};
struct canonical : any_representation {};  ///< equal contents have equal representations

struct single_threaded {};
/// Inserts may run concurrently with each other and with reads of elements already published. Not covered:
/// erase, clear and iteration, which need no other operation running.
struct concurrent : single_threaded {};

struct any_memory {};
/// Storage that erase and clear free is kept for reuse: refilling up to an earlier size allocates nothing.
struct retains : any_memory {};
/// Erase and clear return the storage of what they remove.
struct reclaims : any_memory {};

struct ephemeral {};
struct persistent : ephemeral {};  ///< a change makes a new version and old versions stay valid

struct immutable {};                ///< offers no change
struct growable : immutable {};     ///< offers insert
struct shrinkable : growable {};    ///< offers insert, erase and clear

// What only a Store's interface enforces; no container declares these.
struct may_change {};
struct monotone : may_change {};  ///< only grows: nothing is ever removed
struct frozen : monotone {};      ///< no change after construction

struct maybe_empty {};
struct nonempty : maybe_empty {};  ///< at least one element

struct any_multiplicity {};
struct functional : any_multiplicity {};  ///< at most one value per key (a map)
struct multi : any_multiplicity {};       ///< any number of values per key (a multimap)

struct by_key {};
struct positional : by_key {};  ///< identified by position 0..n-1, in order (a sequence)

/// The properties of something, one tag per category, in a fixed order: equal properties are one type.
template<typename Order, typename Uniqueness, typename Keys, typename References, typename Layout, typename Canonical,
         typename Concurrency, typename Version, typename Mutability, typename Lifecycle, typename Emptiness,
         typename Multiplicity, typename Identity, typename Memory>
struct properties
{
    using order_category = Order;
    using uniqueness_category = Uniqueness;
    using key_category = Keys;
    using reference_category = References;
    using layout_category = Layout;
    using canonical_category = Canonical;
    using concurrency_category = Concurrency;
    using version_category = Version;
    using mutability_category = Mutability;
    using lifecycle_category = Lifecycle;
    using emptiness_category = Emptiness;
    using multiplicity_category = Multiplicity;
    using identity_category = Identity;
    using memory_category = Memory;
};

namespace detail
{

/// The tag among Tags in the category rooted at Root, else Root.
template<typename Root, typename... Tags>
struct pick
{
    using type = Root;
};
template<typename Root, typename Tag, typename... Tags>
struct pick<Root, Tag, Tags...> : std::conditional_t<std::derived_from<Tag, Root>, std::type_identity<Tag>, pick<Root, Tags...>>
{
};
template<typename Root, typename... Tags>
using pick_t = typename pick<Root, Tags...>::type;

template<typename Root, typename... Tags>
inline constexpr int count_in = (0 + ... + int(std::derived_from<Tags, Root>));

/// The roots of the categories a container declares, then those only a Store's request has.
template<typename... Roots>
struct root_list
{
};
using guaranteed_roots = root_list<any_order, duplicates, any_keys, moving_refs, any_layout, any_representation,
                                   single_threaded, ephemeral, immutable, any_memory>;
using requested_roots = root_list<may_change, maybe_empty, any_multiplicity, by_key>;

template<typename Tag, typename List>
inline constexpr bool in_list = false;
template<typename Tag, typename... Roots>
inline constexpr bool in_list<Tag, root_list<Roots...>> = (std::derived_from<Tag, Roots> || ...);

template<typename List, typename... Tags>
inline constexpr bool one_per_category = false;
template<typename... Roots, typename... Tags>
inline constexpr bool one_per_category<root_list<Roots...>, Tags...> = ((count_in<Roots, Tags...> <= 1) && ...);

}  // namespace detail

template<typename Tag>
inline constexpr bool is_guarantee = detail::in_list<Tag, detail::guaranteed_roots>;

template<typename Tag>
inline constexpr bool is_request_only = detail::in_list<Tag, detail::requested_roots>;

/// The properties stated by Tags, in any order; categories no tag answers keep their root.
template<typename... Tags>
using properties_of = properties<detail::pick_t<any_order, Tags...>, detail::pick_t<duplicates, Tags...>,
                                 detail::pick_t<any_keys, Tags...>, detail::pick_t<moving_refs, Tags...>,
                                 detail::pick_t<any_layout, Tags...>, detail::pick_t<any_representation, Tags...>,
                                 detail::pick_t<single_threaded, Tags...>, detail::pick_t<ephemeral, Tags...>,
                                 detail::pick_t<immutable, Tags...>, detail::pick_t<may_change, Tags...>,
                                 detail::pick_t<maybe_empty, Tags...>, detail::pick_t<any_multiplicity, Tags...>,
                                 detail::pick_t<by_key, Tags...>, detail::pick_t<any_memory, Tags...>>;

/// What a container guarantees, by itself, through every operation it offers. Specialize it for a container
/// to declare its properties, and check the declaration by adding the container to tests/properties.cpp:
///
///     template<> struct urd::container_traits<MyList> : urd::properties_of<urd::sorted, urd::shrinkable> {};
///
/// A container nobody declared guarantees nothing.
template<typename R>
struct container_traits : properties_of<>
{
};

/// The properties of X: X itself when it lists them (a request's spec), else its container_traits.
template<typename X>
using traits_t = std::conditional_t<requires { typename X::order_category; }, X, container_traits<std::remove_cvref_t<X>>>;

namespace detail
{

template<typename Traits, typename Tag>
consteval bool has_one()
{
    if constexpr (std::derived_from<Tag, any_order>)
        return std::derived_from<typename Traits::order_category, Tag>;
    else if constexpr (std::derived_from<Tag, duplicates>)
        return std::derived_from<typename Traits::uniqueness_category, Tag>;
    else if constexpr (std::derived_from<Tag, any_keys>)
        return std::derived_from<typename Traits::key_category, Tag>;
    else if constexpr (std::derived_from<Tag, moving_refs>)
        return std::derived_from<typename Traits::reference_category, Tag>;
    else if constexpr (std::derived_from<Tag, any_layout>)
        return std::derived_from<typename Traits::layout_category, Tag>;
    else if constexpr (std::derived_from<Tag, any_representation>)
        return std::derived_from<typename Traits::canonical_category, Tag>;
    else if constexpr (std::derived_from<Tag, single_threaded>)
        return std::derived_from<typename Traits::concurrency_category, Tag>;
    else if constexpr (std::derived_from<Tag, ephemeral>)
        return std::derived_from<typename Traits::version_category, Tag>;
    else if constexpr (std::derived_from<Tag, immutable>)
        return std::derived_from<typename Traits::mutability_category, Tag>;
    else if constexpr (std::derived_from<Tag, may_change>)
        return std::derived_from<typename Traits::lifecycle_category, Tag>;
    else if constexpr (std::derived_from<Tag, maybe_empty>)
        return std::derived_from<typename Traits::emptiness_category, Tag>;
    else if constexpr (std::derived_from<Tag, any_memory>)
        return std::derived_from<typename Traits::memory_category, Tag>;
    else if constexpr (std::derived_from<Tag, any_multiplicity>)
        return std::derived_from<typename Traits::multiplicity_category, Tag>;
    else
    {
        static_assert(std::derived_from<Tag, by_key>, "urd::has: not an urd property");
        return std::derived_from<typename Traits::identity_category, Tag>;
    }
}

}  // namespace detail

/// Whether X (a container, a Store, or a request's spec) has every property in Tags.
template<typename X, typename... Tags>
inline constexpr bool has = (detail::has_one<traits_t<X>, Tags>() && ...);

/// Whether Have provides, in every category a container declares, at least what Want asks for.
template<typename Have, typename Want>
inline constexpr bool satisfies =
    std::derived_from<typename Have::order_category, typename Want::order_category>
    && std::derived_from<typename Have::uniqueness_category, typename Want::uniqueness_category>
    && std::derived_from<typename Have::key_category, typename Want::key_category>
    && std::derived_from<typename Have::reference_category, typename Want::reference_category>
    && std::derived_from<typename Have::layout_category, typename Want::layout_category>
    && std::derived_from<typename Have::canonical_category, typename Want::canonical_category>
    && std::derived_from<typename Have::concurrency_category, typename Want::concurrency_category>
    && std::derived_from<typename Have::version_category, typename Want::version_category>
    && std::derived_from<typename Have::mutability_category, typename Want::mutability_category>
    && std::derived_from<typename Have::memory_category, typename Want::memory_category>;

/// Whether the contents of `range` are as Traits says: sorted, unique, nonempty. The other properties are
/// about the type, not the contents; the tests check those. O(n log n) at most.
template<typename Traits, std::ranges::forward_range R>
[[nodiscard]] bool holds(const R& range)
{
    if (has<Traits, nonempty> && std::ranges::empty(range))
        return false;
    if (has<Traits, sorted> && !std::ranges::is_sorted(range))
        return false;
    if constexpr (has<Traits, unique>)
    {
        if constexpr (has<Traits, sorted>)
            return std::ranges::adjacent_find(range) == std::ranges::end(range);
        auto copy = std::vector<std::ranges::range_value_t<R>>(std::ranges::begin(range), std::ranges::end(range));
        std::ranges::sort(copy);
        return std::ranges::adjacent_find(copy) == copy.end();
    }
    return true;
}

}  // namespace urd
