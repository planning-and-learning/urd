#pragma once

// What computes with the tags (urd/tags.hpp): each category's table (its answers, which is at least which), a tag as
// its category's table and its answer, the properties of a container or a request as the answers it holds
// (property_set), and what a container's properties must meet for a request (satisfies, promises, demands_of).
// Properties are computed as values, not as types: a request costs the compiler a constant evaluation instead of
// hundreds of class instantiations. Categories are open: any enum with tag types (urd/tags.hpp, tags_of) is one.

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

/// The most answers a category has, its none included.
inline constexpr std::size_t most_answers = 4;

/// What the machinery knows of a category, from its tag types (urd/tags.hpp, tags_of): its name (its none's), its
/// answers' names, which answer is at least which (an answer derived from another), whether it is exact, which answers
/// are demanded, and whether only a request states it.
struct category_table
{
    std::string_view name;
    std::uint8_t answers = 0;
    std::array<std::string_view, most_answers> names {};
    std::array<std::array<bool, most_answers>, most_answers> at_least {};
    bool exact = false;
    std::array<bool, most_answers> demanded {};
    bool request_only = false;
};
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
template<typename Tag>
consteval bool request_only_of()
{
    if constexpr (requires { Tag::request_only; })
        return Tag::request_only;
    else
        return false;
}
template<typename Root, typename... Ts>
consteval category_table table_of_tags(type_list<Root, Ts...> /*tags*/)
{
    static_assert(sizeof...(Ts) + 1 <= most_answers, "urd: a category has more answers than most_answers");
    static_assert(std::is_same_v<typename Root::root, Root>, "urd: a category's tag types list its none first");
    auto result = category_table { .name = Root::name, .answers = sizeof...(Ts) + 1, .names = { Root::name, Ts::name... } };
    constexpr std::array<std::uint8_t, sizeof...(Ts) + 1> values { static_cast<std::uint8_t>(Root::value), static_cast<std::uint8_t>(Ts::value)... };
    for (std::size_t each = 0; each < values.size(); ++each)
        if (values[each] != each)
            throw "urd: a category's tag types must list its none first, then its answers in the order of its enum";
    std::size_t have = 0;
    const auto row = [&]<typename Have>(std::type_identity<Have> /*have*/)
    {
        result.at_least[have] = { std::is_base_of_v<Root, Have>, std::is_base_of_v<Ts, Have>... };
        result.demanded[have] = demanded_of<Have>();
        ++have;
    };
    row(std::type_identity<Root> {});
    (row(std::type_identity<Ts> {}), ...);
    result.exact = exact_of<Root>();
    result.request_only = request_only_of<Root>();
    return result;
}

/// The table of the category whose enum is E.
template<category_enum E>
inline constexpr category_table category_table_v = table_of_tags(typename tags_of<E>::type {});

}  // namespace detail

/// A property: a category (its table) and its answer, from any category's enum (order::sorted, uniqueness::unique, …).
/// What the machinery computes with; users name the tag types (urd/tags.hpp).
struct tag
{
    const detail::category_table* in = nullptr;  ///< nullptr: no tag
    std::uint8_t answer = 0;                     ///< 0: the category's none

    constexpr tag() = default;
    constexpr tag(const detail::category_table* of, std::uint8_t value) : in(of), answer(value) {}
    template<category_enum E>
    constexpr tag(E value)  // NOLINT(google-explicit-constructor): every category's answer is a tag
        : in(&detail::category_table_v<E>), answer(static_cast<std::uint8_t>(value))
    {
    }
    friend constexpr bool operator==(tag, tag) = default;
};

/// The table of category E.
template<category_enum E>
inline constexpr const detail::category_table* category = &detail::category_table_v<E>;

namespace detail
{

/// Whether answer `have` is at least `want`, both of category `of`.
constexpr bool at_least(const category_table* of, std::uint8_t have, std::uint8_t want) { return of->at_least[have][want]; }
constexpr bool at_least(tag have, tag want) { return have.in == want.in && at_least(have.in, have.answer, want.answer); }

/// The most categories one set answers.
inline constexpr std::size_t most_categories = 32;

/// The properties of something, as a value: the answers it holds beyond none, one per category, in the order they
/// were put. Equal properties are equal values, whatever that order.
struct property_set
{
    std::array<tag, most_categories> held {};
    std::size_t count = 0;

    /// Nothing said: every category at none.
    static constexpr property_set roots() { return {}; }
    /// The given tags (any category's answers: order::sorted, …), at most one per category, the others at none.
    static constexpr property_set of(std::initializer_list<tag> stated)
    {
        auto result = roots();
        for (const auto each : stated)
            result.put(each);
        return result;
    }

    /// The tag held in category `of` (its none where it holds none).
    [[nodiscard]] constexpr tag in(const category_table* of) const
    {
        for (std::size_t each = 0; each < count; ++each)
            if (held[each].in == of)
                return held[each];
        return { of, 0 };
    }
    /// The answer held in the category of enum E.
    template<category_enum E>
    [[nodiscard]] constexpr E get() const
    { return static_cast<E>(in(urd::category<E>).answer); }
    /// Whether the tag held in `of`'s category is at least `of`.
    [[nodiscard]] constexpr bool has(tag of) const { return at_least(of.in, in(of.in).answer, of.answer); }
    /// Holds `of` in its category, replacing what was there (none: nothing).
    constexpr property_set& put(tag of)
    {
        for (std::size_t each = 0; each < count; ++each)
            if (held[each].in == of.in)
            {
                if (of.answer != 0)
                    held[each] = of;
                else
                {
                    for (std::size_t later = each + 1; later < count; ++later)
                        held[later - 1] = held[later];
                    held[--count] = tag {};
                }
                return *this;
            }
        if (of.answer != 0)
        {
            if (count == most_categories)
                throw "urd: a property set answers more categories than most_categories";
            held[count++] = of;
        }
        return *this;
    }
    /// The tags it holds beyond none, in the order they were put.
    template<typename F>
    constexpr void for_each(F f) const
    {
        for (std::size_t each = 0; each < count; ++each)
            f(held[each]);
    }

    friend constexpr bool operator==(const property_set& lhs, const property_set& rhs)
    {
        if (lhs.count != rhs.count)
            return false;
        for (std::size_t each = 0; each < lhs.count; ++each)
            if (rhs.in(lhs.held[each].in) != lhs.held[each])
                return false;
        return true;
    }
};

/// Whether `have` provides, in every category a container declares, at least what `want` asks for; exactly that, in
/// the categories that declare themselves exact (urd/tags.hpp).
constexpr bool satisfies(const property_set& have, const property_set& want)
{
    for (std::size_t each = 0; each < want.count; ++each)
    {
        const auto asked = want.held[each];
        if (!asked.in->request_only && !(asked.in->exact ? have.in(asked.in) == asked : have.has(asked)))
            return false;
    }
    for (std::size_t each = 0; each < have.count; ++each)  // an exact category answered beyond what is asked
    {
        const auto given = have.held[each];
        if (given.in->exact && !given.in->request_only && want.in(given.in).answer == 0)
            return false;
    }
    return true;
}

/// Whether a request that holds `stated` makes every promise in `needed`.
constexpr bool promises(const property_set& stated, const property_set& needed)
{
    for (std::size_t each = 0; each < needed.count; ++each)
        if (!stated.has(needed.held[each]))
            return false;
    return true;
}

/// What a container that holds `declared` asks of the request in turn: the tags it holds that declare themselves
/// demanded (urd/tags.hpp: dense).
constexpr property_set demands_of(const property_set& declared)
{
    auto result = property_set::roots();
    declared.for_each(
        [&result](tag each)
        {
            if (each.in->demanded[each.answer])
                result.put(each);
        });
    return result;
}

template<typename P>
concept a_tag = requires {
    typename P::root;
    { P::value } -> std::convertible_to<tag>;
};

}  // namespace detail

/// A tag's name, as a request writes it.
constexpr std::string_view tag_name(tag of) { return of.in->names[of.answer]; }

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
inline constexpr bool is_guarantee = !tag(Tag::value).in->request_only;
template<typename Tag>
inline constexpr bool is_request_only = tag(Tag::value).in->request_only;

}  // namespace urd
