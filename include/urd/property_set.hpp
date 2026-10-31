#pragma once

// What computes with the tags (urd/tags.hpp): each tag's category and what it is at least, the properties of a
// container or a request as one constexpr value per category (property_set), and what a container's properties
// must meet for a request (satisfies, promises, demands_of). Properties are computed as values, not as types: a
// request costs the compiler a constant evaluation instead of hundreds of class instantiations.

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

/// Lists of types, joined in order.
template<typename... Lists>
struct join
{
    using type = type_list<>;
};
template<typename... Ts>
struct join<type_list<Ts...>>
{
    using type = type_list<Ts...>;
};
template<typename... As, typename... Bs, typename... Lists>
struct join<type_list<As...>, type_list<Bs...>, Lists...> : join<type_list<As..., Bs...>, Lists...>
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
                           may_change,
                           monotone,
                           frozen,
                           maybe_empty,
                           nonempty,
                           any_multiplicity,
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
{ return std::array<std::string_view, sizeof...(Tags)> { Tags::name... }; }
/// Every tag's name, in the order of urd::tag.
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


/// The categories whose root declares itself exact, and the tags that declare themselves demanded (urd/tags.hpp).
template<typename Tag>
consteval bool exact_of()
{
    if constexpr (requires { Tag::exact; })
        return Tag::exact;
    else
        return false;
}
template<typename Tag>
consteval bool demanded_of()
{
    if constexpr (requires { Tag::demanded; })
        return Tag::demanded;
    else
        return false;
}
template<typename... Ts>
consteval std::array<bool, tag_count> make_exact_table(type_list<Ts...> /*tags*/)
{ return { exact_of<Ts>()... }; }
template<typename... Ts>
consteval std::array<bool, tag_count> make_demanded_table(type_list<Ts...> /*tags*/)
{ return { demanded_of<Ts>()... }; }
inline constexpr std::array<bool, tag_count> roots_exact = make_exact_table(all_tags {});
inline constexpr std::array<bool, tag_count> tags_demanded = make_demanded_table(all_tags {});

constexpr tag root_of(tag of) { return tags.root[index(of)]; }
constexpr bool at_least(tag have, tag want) { return tags.at_least[index(have)][index(want)]; }

/// The roots of the categories a container declares, then those a request states.
inline constexpr std::array guaranteed_roots {
    tag::any_order,  tag::duplicates,  tag::any_keys,     tag::moving_refs,   tag::any_layout, tag::any_representation, tag::single_threaded,
    tag::any_memory,  tag::any_clear,    tag::any_access,    tag::any_scope,  tag::any_layering,       tag::any_tracking,
    tag::any_handle, tag::any_sharing, tag::any_grouping, tag::any_direction, tag::any_shape,  tag::any_packing,        tag::by_key
};
inline constexpr std::array requested_roots { tag::may_change, tag::maybe_empty, tag::any_multiplicity, tag::general, tag::any_priority };

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

/// Whether `have` provides, in every category a container declares, at least what `want` asks for; exactly that, in
/// the categories that declare themselves exact (urd/tags.hpp).
constexpr bool satisfies(const property_set& have, const property_set& want)
{
    for (const auto root : guaranteed_roots)
        if (roots_exact[index(root)] ? have.in(root) != want.in(root) : !at_least(have.in(root), want.in(root)))
            return false;
    return true;
}

/// Whether a request that holds `stated` makes every promise in `needed`.
constexpr bool promises(const property_set& stated, const property_set& needed)
{
    for (std::size_t each = 0; each < tag_count; ++each)
        if (!at_least(stated.held[each], needed.held[each]))
            return false;
    return true;
}

/// What a container that holds `declared` asks of the request in turn: the tags it holds that declare themselves
/// demanded (urd/tags.hpp: dense).
constexpr property_set demands_of(const property_set& declared)
{
    auto result = property_set::roots();
    for (std::size_t each = 0; each < tag_count; ++each)
        if (tags_demanded[each] && declared.has(static_cast<tag>(each)))
            result.put(static_cast<tag>(each));
    return result;
}

template<typename P>
concept a_tag = requires {
    typename P::root;
    { P::value } -> std::convertible_to<tag>;
};

}  // namespace detail

/// A tag's name, as a request writes it.
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
