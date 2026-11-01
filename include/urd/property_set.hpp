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
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace urd
{

struct tag;

namespace detail
{

/// The most answers a category has, its none included.
inline constexpr std::size_t most_answers = 4;

/// The tags a tag type lists (implies, kept_by_store, promises, later), as tags: a view of a static array of them.
struct tag_list
{
    const tag* first = nullptr;
    const tag* last = nullptr;
    [[nodiscard]] constexpr const tag* begin() const { return first; }
    [[nodiscard]] constexpr const tag* end() const { return last; }
};

/// What the machinery knows of a category, from its tag types (urd/tags.hpp, tags_of): its name (its none's), its
/// answers' names, which answer is at least which (an answer derived from another), whether it is exact, which answers
/// are demanded, and whether only a request states it.
struct category_table
{
    std::string_view name;
    std::string_view description;  ///< a type trait's (urd/registry/facts.hpp); a stated category's words are its tags'
    std::uint8_t answers = 0;
    std::array<std::string_view, most_answers> names {};
    std::array<std::array<bool, most_answers>, most_answers> at_least {};
    bool exact = false;
    std::array<bool, most_answers> demanded {};
    bool request_only = false;
    /// What each answer says of the Store (urd::tag_defaults): what it implies, what the Store keeps itself rather
    /// than asks of its container, what it promises its container, which containers it puts later, whether it stops
    /// growing or shrinking, whether it fixes the order the Store is iterated or hashed in.
    std::array<tag_list, most_answers> implies {};
    std::array<tag_list, most_answers> kept_by_store {};
    std::array<tag_list, most_answers> promises {};
    std::array<tag_list, most_answers> later {};
    std::array<bool, most_answers> stops_growth {};
    std::array<bool, most_answers> stops_shrinking {};
    std::array<bool, most_answers> stops_changes {};
    std::array<bool, most_answers> needs_an_element {};
    std::array<bool, most_answers> fixes_iteration_order {};
    std::array<bool, most_answers> fixes_hash_order {};
    std::array<bool, most_answers> costly {};
};
/// The tags of a type list of tag types (defined once `tag` is, below).
template<typename List>
consteval tag_list tag_list_of();

template<typename Root, typename... Ts>
consteval category_table table_of_tags(type_list<Root, Ts...> /*tags*/)
{
    static_assert(sizeof...(Ts) + 1 <= most_answers, "urd: a category has more answers than most_answers");
    static_assert(std::is_same_v<typename Root::root, Root>, "urd: a category's tag types list its none first");
    auto result = category_table { .name = Root::name, .description = {}, .answers = sizeof...(Ts) + 1, .names = { Root::name, Ts::name... } };
    constexpr std::array<std::uint8_t, sizeof...(Ts) + 1> values { static_cast<std::uint8_t>(Root::value), static_cast<std::uint8_t>(Ts::value)... };
    for (std::size_t each = 0; each < values.size(); ++each)
        if (values[each] != each)
            throw "urd: a category's tag types must list its none first, then its answers in the order of its enum";
    std::size_t have = 0;
    const auto row = [&]<typename Have>(std::type_identity<Have> /*have*/)
    {
        result.at_least[have] = { std::is_base_of_v<Root, Have>, std::is_base_of_v<Ts, Have>... };
        result.demanded[have] = Have::demanded;
        result.implies[have] = tag_list_of<typename Have::implies>();
        result.kept_by_store[have] = tag_list_of<typename Have::kept_by_store>();
        result.promises[have] = tag_list_of<typename Have::promises>();
        result.later[have] = tag_list_of<typename Have::later>();
        result.stops_growth[have] = Have::stops_growth;
        result.stops_shrinking[have] = Have::stops_shrinking;
        result.stops_changes[have] = Have::stops_changes;
        result.needs_an_element[have] = Have::needs_an_element;
        result.fixes_iteration_order[have] = Have::fixes_iteration_order;
        result.fixes_hash_order[have] = Have::fixes_hash_order;
        result.costly[have] = Have::costly;
        ++have;
    };
    row(std::type_identity<Root> {});
    (row(std::type_identity<Ts> {}), ...);
    result.exact = Root::exact;
    result.request_only = Root::request_only;
    return result;
}

/// The table of the category whose enum is E.
template<category_enum E>
inline constexpr category_table category_table_v = table_of_tags(typename tags_of<E>::type {});

}  // namespace detail

/// The words of a thing identified by them, by their address, as a category is by its table: a type trait, a role, an
/// operation (Of tells them apart, so no trait's words pass for a role's). No number and no list of all of them, so a
/// kind added outside urd adds its own.
template<typename Of>
struct words
{
    std::string_view name;
    std::string_view description;
};
/// The words of T (a struct with its name and description), as Of.
template<typename Of, typename T>
inline constexpr words<Of> words_of { T::name, T::description };

/// A role's words: what of a Store's data a fact is about (its elements, its order, a map's values, its rows; the roles
/// themselves are in urd/registry/facts.hpp).
using role_info = words<struct role_words>;

/// A fact: a category (its table) and its answer, about a role of a Store's data, or about the Store itself (no role:
/// what a request states, what a container provides). From any category's enum (order::sorted, …); what the machinery
/// computes with, while users name the tag types (urd/tags.hpp).
struct tag
{
    const detail::category_table* in = nullptr;  ///< nullptr: no tag
    std::uint8_t answer = 0;                     ///< 0: the category's none
    const role_info* role = nullptr;             ///< nullptr: about the Store itself

    constexpr tag() = default;
    constexpr tag(const detail::category_table* of, std::uint8_t value, const role_info* about_role = nullptr) : in(of), answer(value), role(about_role) {}
    template<category_enum E>
    constexpr tag(E value)  // NOLINT(google-explicit-constructor): every category's answer is a tag
        :
        in(&detail::category_table_v<E>),
        answer(static_cast<std::uint8_t>(value))
    {
    }
    /// This fact, about `of` (a role) instead.
    [[nodiscard]] constexpr tag about(const role_info* of) const { return { in, answer, of }; }
    friend constexpr bool operator==(tag, tag) = default;
};

namespace detail
{

template<typename List>
inline constexpr std::array<tag, 0> tags_array {};
template<typename... Tags>
inline constexpr std::array<tag, sizeof...(Tags)> tags_array<type_list<Tags...>> { tag(Tags::value)... };
template<typename List>
consteval tag_list tag_list_of()
{ return { tags_array<List>.data(), std::to_address(tags_array<List>.end()) }; }

}  // namespace detail

/// The table of category E.
template<category_enum E>
inline constexpr const detail::category_table* category = &detail::category_table_v<E>;

namespace detail
{

/// Whether answer `have` is at least `want`, both of category `of`.
constexpr bool at_least(const category_table* of, std::uint8_t have, std::uint8_t want) { return of->at_least[have][want]; }
constexpr bool at_least(tag have, tag want) { return have.in == want.in && have.role == want.role && at_least(have.in, have.answer, want.answer); }

/// The most categories one set answers.
inline constexpr std::size_t most_categories = 64;

/// At most N values, in the order they were put: the storage of the small lists urd computes with (property_set, with
/// its own lookup and equality on top; the entries serving a request, urd/registry/scan.hpp).
template<typename T, std::size_t N>
struct fixed_list
{
    std::array<T, N> held {};
    std::size_t count = 0;

    [[nodiscard]] constexpr auto begin() const { return held.begin(); }
    [[nodiscard]] constexpr auto end() const { return std::next(held.begin(), static_cast<std::ptrdiff_t>(count)); }
    [[nodiscard]] constexpr auto begin() { return held.begin(); }
    [[nodiscard]] constexpr auto end() { return std::next(held.begin(), static_cast<std::ptrdiff_t>(count)); }
    /// Appends `value`; past N, constant evaluation fails (raise its N: most_categories for a property_set).
    constexpr T& push(const T& value)
    {
        if (count == N)
            throw "urd: a fixed_list holds more than its most (most_categories for a property_set)";
        held[count] = value;
        return held[count++];
    }
    /// Removes the value at `at`, keeping the order of the others.
    constexpr void erase(auto at)
    {
        std::move(std::next(at), end(), at);
        held[--count] = T {};
    }
    template<typename F>
    constexpr void for_each(F f) const
    { std::ranges::for_each(*this, f); }
};

/// The properties of something, as a value: the answers it holds beyond none, one per category, in the order they
/// were put. Equal properties are equal values, whatever that order.
struct property_set : fixed_list<tag, most_categories>
{
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

    /// The tag held in category `of` about the Store itself, or about role `role` (its none where it holds none).
    [[nodiscard]] constexpr tag in(const category_table* of, const role_info* role = nullptr) const
    {
        const auto found = std::ranges::find_if(*this, [of, role](tag each) { return each.in == of && each.role == role; });
        return found != end() ? *found : tag { of, 0, role };
    }
    /// The answer held in the category of enum E.
    template<category_enum E>
    [[nodiscard]] constexpr E get() const
    { return static_cast<E>(in(urd::category<E>) .answer); }
    /// Whether the tag held in `of`'s category is at least `of`.
    [[nodiscard]] constexpr bool has(tag of) const { return at_least(of.in, in(of.in, of.role).answer, of.answer); }
    /// Whether any of its tags about the Store itself says Field (a per-answer flag of detail::category_table:
    /// stops_growth, fixes_hash_order, …).
    template<auto Field>
    [[nodiscard]] constexpr bool any_says() const
    {
        return std::ranges::any_of(*this, [](tag each) { return each.role == nullptr && (each.in->*Field)[each.answer]; });
    }
    /// Calls f on every tag its tags about the Store itself list in Field (a per-answer list of detail::category_table:
    /// implies, promises, …).
    template<auto Field, typename F>
    constexpr void for_each_listed(F f) const
    {
        for (const tag each : *this)
            if (each.role == nullptr)
                for (const tag listed : (each.in->*Field)[each.answer])
                    f(listed);
    }
    /// Holds `of` in its category (about its role), replacing what was there (none: nothing).
    constexpr property_set& put(tag of)
    {
        const auto found = std::ranges::find_if(*this, [of](tag each) { return each.in == of.in && each.role == of.role; });
        if (found == end())
        {
            if (of.answer != 0)
                push(of);
        }
        else if (of.answer != 0)
            *found = of;
        else
            erase(found);
        return *this;
    }

    friend constexpr bool operator==(const property_set& lhs, const property_set& rhs)
    {
        return lhs.count == rhs.count && std::ranges::all_of(lhs, [&rhs](tag each) { return rhs.in(each.in, each.role) == each; });
    }
};

/// Whether `have` provides, in every category a container declares, at least what `want` asks for; exactly that, in
/// the categories that declare themselves exact (urd/tags.hpp).
constexpr bool satisfies(const property_set& have, const property_set& want)
{
    const auto provided = [&have](tag asked) { return asked.in->request_only || (asked.in->exact ? have.in(asked.in, asked.role) == asked : have.has(asked)); };
    // and no exact category answered beyond what is asked
    const auto unasked = [&want](tag given) { return given.in->exact && !given.in->request_only && want.in(given.in, given.role).answer == 0; };
    return std::ranges::all_of(want, provided) && std::ranges::none_of(have, unasked);
}

/// Whether a request that holds `stated` makes every promise in `needed`.
constexpr bool promises(const property_set& stated, const property_set& needed)
{
    return std::ranges::all_of(needed, [&stated](tag each) { return stated.has(each); });
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

/// `base` with every tag of `listed` (a range of tags, or a braced list) put in its category.
constexpr property_set with(property_set base, const auto& listed)
{
    for (const auto each : listed)
        base.put(each);
    return base;
}
constexpr property_set with(property_set base, std::initializer_list<tag> listed) { return with(base, std::span<const tag>(listed.begin(), listed.size())); }

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
};
}  // namespace urd
