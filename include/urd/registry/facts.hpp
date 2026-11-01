#pragma once

// The tags and type traits by role (role_tags) that a request promises and a container relies on: what a Store
// states, and what its data is (urd/type_traits_of.hpp), its elements, order, values and rows.

#include "urd/property_set.hpp"
#include "urd/semantics/less.hpp"
#include "urd/traits.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>

namespace urd
{

/// A type trait's words. A trait is identified by its words' address, as a category by its table's: no number, no
/// list of all traits, so a kind added outside urd adds its own.
struct trait_info
{
    std::string_view name;
    std::string_view description;
};
template<typename Trait>
inline constexpr trait_info trait_info_v { Trait::name, Trait::description };
/// The trait Trait (a struct with its name and description).
template<typename Trait>
inline constexpr const trait_info* trait_v = &trait_info_v<Trait>;

/// What a type is, for the containers that hold it: derived from the types (urd/type_traits_of.hpp), never stated. What a
/// key, value or row element type is; what an order is (order_by<P>, else urd::Less); what rows (Stores held as
/// elements) allow.
namespace trait
{

struct can_hash
{
    static constexpr std::string_view name = "can_hash";
    static constexpr std::string_view description = "urd::Hash and urd::EqualTo hash and compare it";
};
struct an_index
{
    static constexpr std::string_view name = "an_index";
    static constexpr std::string_view description = "an index 0..n-1 (index_of)";
};
struct copies_as_bytes
{
    static constexpr std::string_view name = "copies_as_bytes";
    static constexpr std::string_view description = "trivially copyable: copied as bytes";
};
struct destroys_trivially
{
    static constexpr std::string_view name = "destroys_trivially";
    static constexpr std::string_view description = "trivially destructible: destroyed without code";
};
struct names_members
{
    static constexpr std::string_view name = "names_members";
    static constexpr std::string_view description = "names its members for cista (cista_members)";
};
struct whole_value
{
    static constexpr std::string_view name = "whole_value";
    static constexpr std::string_view description = "a value interned whole: no Store and no container of elements, which a pool holds as rows";
};
struct own_order
{
    static constexpr std::string_view name = "own_order";
    static constexpr std::string_view description = "ordered by urd::Less: no order_by";
};
struct natural_ordered
{
    static constexpr std::string_view name = "natural_ordered";
    static constexpr std::string_view description = "ordered as sorted means";
};
struct by_entry
{
    static constexpr std::string_view name = "by_entry";
    static constexpr std::string_view description = "the order compares a map's entries, key and value";
};
struct integral_priority
{
    static constexpr std::string_view name = "integral_priority";
    static constexpr std::string_view description = "what the order compares is an integral number";
};
struct rows_flatten
{
    static constexpr std::string_view name = "rows_flatten";
    static constexpr std::string_view description = "rows frozen, holding elements or rows kept in order: stored flattened";
};
struct rows_in_place
{
    static constexpr std::string_view name = "rows_in_place";
    static constexpr std::string_view description = "rows changed in place: sequences, dense sets' bits, or fixed-shape rows of such rows";
};
struct rows_reused
{
    static constexpr std::string_view name = "rows_reused";
    static constexpr std::string_view description = "rows that may shrink, so emptied ones can be kept for reuse";
};

}  // namespace trait

}  // namespace urd

namespace urd::detail
{

/// The most traits one role holds.
inline constexpr std::size_t most_traits = 16;

/// The traits a role holds, in the order they were put. Equal sets are equal values, whatever that order.
struct trait_set
{
    std::array<const trait_info*, most_traits> held {};
    std::size_t count = 0;

    [[nodiscard]] constexpr bool has(const trait_info* of) const
    {
        for (std::size_t each = 0; each < count; ++each)
            if (held[each] == of)
                return true;
        return false;
    }
    constexpr trait_set& put(const trait_info* of)
    {
        if (has(of))
            return *this;
        if (count == most_traits)
            throw "urd: a role holds more traits than most_traits";
        held[count++] = of;
        return *this;
    }
    /// Puts Trait where `holds`.
    template<typename Trait>
    constexpr trait_set& put_if(bool holds)
    { return holds ? put(trait_v<Trait>) : *this; }
    template<typename F>
    constexpr void for_each(F f) const
    {
        for (std::size_t each = 0; each < count; ++each)
            f(held[each]);
    }

    friend constexpr bool operator==(const trait_set& lhs, const trait_set& rhs)
    {
        if (lhs.count != rhs.count)
            return false;
        for (std::size_t each = 0; each < lhs.count; ++each)
            if (!rhs.has(lhs.held[each]))
                return false;
        return true;
    }
};
/// The set of Traits.
template<typename... Traits>
constexpr trait_set trait_set_of()
{
    auto result = trait_set {};
    (result.put(trait_v<Traits>), ...);
    return result;
}
/// Whether `held` holds every trait of `needed`.
constexpr bool promises(const trait_set& held, const trait_set& needed)
{
    for (std::size_t each = 0; each < needed.count; ++each)
        if (!held.has(needed.held[each]))
            return false;
    return true;
}

}  // namespace urd::detail

namespace urd
{

/// A role's words: what of a Store's data a request promises about and a container relies on. A role is identified by
/// its words' address, as a category or a trait is: a kind added outside urd adds its own.
struct role_info
{
    std::string_view name;
    std::string_view description;
};
template<typename Role>
inline constexpr role_info role_info_v { Role::name, Role::description };
/// The role Role (a struct with its name and description).
template<typename Role>
inline constexpr const role_info* role_v = &role_info_v<Role>;

/// urd's roles; each kind lists those it has (urd/kinds.hpp, roles).
namespace role
{

struct element
{
    static constexpr std::string_view name = "element";
    static constexpr std::string_view description = "what its elements are (a map's keys)";
};
struct order
{
    static constexpr std::string_view name = "order";
    static constexpr std::string_view description = "what its order is";
};
struct mapped
{
    static constexpr std::string_view name = "mapped";
    static constexpr std::string_view description = "what a map's values are";
};
struct row
{
    static constexpr std::string_view name = "row";
    static constexpr std::string_view description = "its elements' own tags, where they are Stores (rows)";
};
struct rows
{
    static constexpr std::string_view name = "rows";
    static constexpr std::string_view description = "what its rows allow";
};
struct row_element
{
    static constexpr std::string_view name = "row_element";
    static constexpr std::string_view description = "what its rows' elements are";
};

}  // namespace role

}  // namespace urd

namespace urd::detail
{

/// What a request promises in one role, or a container relies on there: tags (the rows' own) and traits.
struct role_entry
{
    const role_info* of = nullptr;
    property_set tags = property_set::roots();
    trait_set traits {};
};

/// The most roles one set of role tags holds.
inline constexpr std::size_t most_roles = 8;

/// Tags and traits by role: a request's (what it states, what its data is) or a container's (what it relies on).
/// What it states is every kind's; the roles it holds are its kind's, those it says nothing of left out.
struct role_tags
{
    property_set stated = property_set::roots();  ///< the Store's own tags
    std::array<role_entry, most_roles> roles {};
    std::size_t count = 0;

    /// The entry of role `of`, or nullptr where it holds none.
    [[nodiscard]] constexpr const role_entry* find(const role_info* of) const
    {
        for (std::size_t each = 0; each < count; ++each)
            if (roles[each].of == of)
                return &roles[each];
        return nullptr;
    }
    /// The entry of role `of`, added where it holds none.
    constexpr role_entry& at(const role_info* of)
    {
        for (std::size_t each = 0; each < count; ++each)
            if (roles[each].of == of)
                return roles[each];
        if (count == most_roles)
            throw "urd: role tags hold more roles than most_roles";
        roles[count] = role_entry { .of = of };
        return roles[count++];
    }
    template<typename F>
    constexpr void for_each(F f) const
    {
        for (std::size_t each = 0; each < count; ++each)
            f(roles[each]);
    }
};

/// Whether `held` promises, role by role, what `needed` relies on: a role `held` says nothing of promises nothing.
constexpr bool promises(const role_tags& held, const role_tags& needed)
{
    if (!promises(held.stated, needed.stated))
        return false;
    for (std::size_t each = 0; each < needed.count; ++each)
    {
        const auto& want = needed.roles[each];
        const auto* have = held.find(want.of);
        if (have == nullptr ? (want.tags.count != 0 || want.traits.count != 0) : !(promises(have->tags, want.tags) && promises(have->traits, want.traits)))
            return false;
    }
    return true;
}

/// The pieces a container's reliance is written in: what a request must state, and per role, traits or tags.
struct stated_piece
{
    property_set tags;
};
constexpr stated_piece stated(std::initializer_list<tag> listed) { return { property_set::of(listed) }; }
template<typename Role, typename... Traits>
constexpr role_entry traits()
{ return { .of = role_v<Role>, .traits = trait_set_of<Traits...>() }; }
template<typename Role>
constexpr role_entry tags(std::initializer_list<tag> listed)
{ return { .of = role_v<Role>, .tags = property_set::of(listed) }; }
constexpr void put_piece(role_tags& into, const stated_piece& piece) { into.stated = piece.tags; }
constexpr void put_piece(role_tags& into, const role_entry& piece)
{
    auto& entry = into.at(piece.of);
    piece.tags.for_each([&entry](tag each) { entry.tags.put(each); });
    piece.traits.for_each([&entry](const trait_info* each) { entry.traits.put(each); });
}
/// A reliance from its pieces: relies(stated({ change::monotone }), traits<role::element, trait::can_hash>()).
constexpr role_tags relies(const auto&... pieces)
{
    auto result = role_tags {};
    (put_piece(result, pieces), ...);
    return result;
}

/// The comparator of a container that takes one: the request's order_by where it gives one, else urd::Less.
template<typename S>
using element_order_t = std::conditional_t<std::is_void_v<typename S::order_type>, Less<typename S::element_type>, typename S::order_type>;
template<typename S>
using key_order_t = std::conditional_t<std::is_void_v<typename S::order_type>, Less<typename S::key_type>, typename S::order_type>;

/// `base` with every tag of `listed` (any range of tags) put in its category.
constexpr property_set with(property_set base, const auto& listed)
{
    for (const auto each : listed)
        base.put(each);
    return base;
}

/// `base` with `listed` put in their categories.
constexpr property_set with(property_set base, std::initializer_list<tag> listed)
{
    for (const auto each : listed)
        base.put(each);
    return base;
}

}  // namespace urd::detail
