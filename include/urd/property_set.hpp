#pragma once

// What computes with the tags (urd/tags.hpp): each tag's category and what it is at least, the properties of a
// container or a request as one constexpr value per category (property_set), and the rules a container's properties
// must meet for a request (satisfies, promises, demands_met). Properties are computed as values, not as types: a
// request costs the compiler a constant evaluation instead of hundreds of class instantiations.

#include "urd/rules.hpp"
#include "urd/tag_synopsis.hpp"
#include "urd/tags.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace urd
{

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
                           any_packing,
                           flat,
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
                           any_priority,
                           monotone_priority>;

inline constexpr std::size_t tag_count = static_cast<std::size_t>(tag::count);
constexpr std::size_t index(tag of) { return static_cast<std::size_t>(of); }

template<typename... Ts>
consteval bool numbered_in_order(type_list<Ts...> /*tags*/)
{
    std::size_t position = 0;
    return sizeof...(Ts) == tag_count && ((index(Ts::value) == position++) && ...);
}
static_assert(numbered_in_order(all_tags {}), "urd::tag and the tag types must list the same properties in the same order");

template<typename... Tags>
constexpr auto tag_names_of(type_list<Tags...> /*tags*/)
{ return std::array<std::string_view, sizeof...(Tags)> { tag_synopsis<Tags>::name... }; }
/// Every tag's name (tag_synopsis), in the order of urd::tag.
inline constexpr auto tag_names = tag_names_of(all_tags {});

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

/// The kind each tag makes (its `makes`), or the lowest, sequence, where it makes none.
template<typename Tag>
consteval kind makes_of()
{
    if constexpr (requires { Tag::makes; })
        return Tag::makes;
    else
        return kind::sequence;
}
template<typename... Ts>
consteval std::array<kind, tag_count> make_kind_table(type_list<Ts...> /*tags*/)
{ return { makes_of<Ts>()... }; }
inline constexpr std::array<kind, tag_count> kinds_made = make_kind_table(all_tags {});

constexpr tag root_of(tag of) { return tags.root[index(of)]; }
constexpr bool at_least(tag have, tag want) { return tags.at_least[index(have)][index(want)]; }

/// The roots of the categories a container declares, then those a request states. A container may also declare
/// positional (its elements are reached by position: a random-access range) and functional (a value per key: a
/// map); selection reads those two directly, so that it never has to look inside a candidate.
inline constexpr std::array guaranteed_roots { tag::any_order,          tag::duplicates,      tag::any_keys,      tag::moving_refs,  tag::any_layout,
                                               tag::any_representation, tag::single_threaded, tag::immutable,     tag::any_memory,   tag::any_clear,
                                               tag::any_access,         tag::any_scope,       tag::any_layering,  tag::any_tracking, tag::any_handle,
                                               tag::any_sharing,        tag::any_grouping,    tag::any_direction, tag::any_shape,    tag::any_packing };
inline constexpr std::array requested_roots { tag::may_change, tag::maybe_empty, tag::any_multiplicity, tag::by_key, tag::general, tag::any_priority };
/// The tags no request states: the library derives them (a map is functional unless it says multi; what a Store's
/// interface offers follows from frozen, monotone, nonempty and fixed_shape), and containers still declare them. A
/// public tag changes some resolution or rejects some request; these would only repeat what is derived.
inline constexpr std::array internal_tags { tag::immutable, tag::growable, tag::shrinkable, tag::functional };

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

/// The kind of a request with these tags: the kind its shape makes (a key: a set; a key and a value: a map; a value: a
/// sequence), or the highest one a tag it holds makes (urd::kind, `makes`).
constexpr kind kind_of(const property_set& held, bool keyed, bool valued)
{
    auto result = keyed ? (valued ? kind::map : kind::set) : kind::sequence;
    for (std::size_t each = 0; each < tag_count; ++each)
    {
        const auto of = static_cast<tag>(each);
        if (root_of(of) != of && held.in(root_of(of)) == of && kinds_made[each] > result)
            result = kinds_made[each];
    }
    return result;
}

/// Whether an implication (urd/rules.hpp) applies to a Store of kind `of` holding `held`.
constexpr bool applies(const implication& rule, kind of, const property_set& held)
{ return in_kinds(rule.of, of) && (rule.when == tag::count || held.has(rule.when)); }

/// What a request of kind `of` that states `stated` holds: the implications applied in order, each answering only a
/// category nothing answered yet.
constexpr property_set implied(property_set stated, kind of)
{
    // Answered: stated, or put by an earlier rule. A rule may answer with a root (frozen: immutable), so this is kept
    // apart from what the categories hold.
    auto answered = std::array<bool, tag_count> {};
    for (std::size_t each = 0; each < tag_count; ++each)
        answered[each] = stated.held[each] != static_cast<tag>(each);
    for (const auto& rule : implications)
        if (applies(rule, of, stated) && !answered[index(root_of(rule.then))])
        {
            stated.put(rule.then);
            answered[index(root_of(rule.then))] = true;
        }
    return stated;
}

/// What a container must guarantee for a Store of kind `of` that promises `promised`: the promise, but for what the
/// Store provides itself (the implications marked provided).
constexpr property_set required_of(property_set promised, kind of)
{
    auto result = promised;
    for (const auto& rule : implications)
        if (rule.provided && applies(rule, of, promised))
            result.put(root_of(rule.then));
    return result;
}

/// Whether `have` provides, in every category a container declares, at least what `want` asks for.
constexpr bool satisfies(const property_set& have, const property_set& want)
{
    for (const auto root : guaranteed_roots)
        if (!at_least(have.in(root), want.in(root)))
            return false;
    return true;
}

/// Whether a request that states `stated` makes every promise in `needed`, in the categories only a request states.
constexpr bool promises(const property_set& stated, const property_set& needed)
{
    for (const auto root : requested_roots)
        if (!at_least(stated.in(root), needed.in(root)))
            return false;
    return true;
}

/// Whether `declared` demands nothing `requested` left open. dense is the one guaranteed category whose declaration
/// also asks something of callers: a container that stores by index needs keys that are indices (its storage grows
/// with the largest key, and a negative key is no index at all), so a representation that declares dense serves only
/// a request that promises dense. Every other guarantee (stable_refs, concurrent, constant_clear, ...) costs the
/// container, never the caller: a request that leaves it open may receive it (the registry audit watches that it
/// does not pay for it needlessly).
constexpr bool demands_met(const property_set& declared, const property_set& requested) { return !declared.has(tag::dense) || requested.has(tag::dense); }

template<typename P>
concept a_tag = requires {
    typename P::root;
    { P::value } -> std::convertible_to<tag>;
};

}  // namespace detail

/// A tag's name, as a request writes it (tag_synopsis).
constexpr std::string_view tag_name(tag of) { return detail::tag_names[detail::index(of)]; }

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

}  // namespace urd
