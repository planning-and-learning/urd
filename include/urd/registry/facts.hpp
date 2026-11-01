#pragma once

// The tags and type traits by role (property_set) that a request promises and a container relies on: what a Store
// states, and what its data is (urd/type_traits_of.hpp), its elements, order, values and rows. Type traits and roles
// are open, as categories are: each is a struct with its words.

#include "urd/property_set.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>

namespace urd
{

/// A type trait is a category of its own, answered by the type rather than stated: its none, and itself. Its table is
/// made from the trait (a struct with its name and description), so a trait added outside urd is a category too.
template<typename Trait>
inline constexpr detail::category_table trait_table_v { .name = Trait::name,
                                                        .description = Trait::description,
                                                        .answers = 2,
                                                        .names = { "", Trait::name },
                                                        .at_least = { { { true, false }, { true, true } } } };
using trait_info = detail::category_table;
/// The trait Trait's category, and the fact that a type has it (about no role until put in one).
template<typename Trait>
inline constexpr const trait_info* trait_v = &trait_table_v<Trait>;
template<typename Trait>
inline constexpr tag trait_tag { trait_v<Trait>, 1 };

/// What a type is, for the containers that hold it: derived from the types (urd/type_traits_of.hpp), never stated. What a
/// key, value or row element type is; what an order is (order_by<C>, else urd::Less); what rows (Stores held as
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

/// Puts trait Trait into `into` where `holds`.
template<typename Trait>
constexpr void put_trait(property_set& into, bool holds)
{
    if (holds)
        into.put(trait_tag<Trait>);
}
/// The traits Traits, about no role.
template<typename... Traits>
constexpr property_set trait_set_of()
{
    auto result = property_set::roots();
    (result.put(trait_tag<Traits>), ...);
    return result;
}

}  // namespace urd::detail

namespace urd
{

/// The role Role (a struct with its name and description): what of a Store's data a fact is about (urd/property_set.hpp,
/// tag::role).
template<typename Role>
inline constexpr const role_info* role_v = &words_of<struct role_words, Role>;

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

/// The facts of `set`, about role `of` instead.
constexpr property_set about(const role_info* of, const property_set& set)
{
    auto result = property_set::roots();
    set.for_each([&result, of](tag each) { result.put(each.about(of)); });
    return result;
}
/// The facts of `set` about the Store itself (what it states or provides), and those about its roles.
constexpr property_set about_itself(const property_set& set)
{
    auto result = property_set::roots();
    set.for_each(
        [&result](tag each)
        {
            if (each.role == nullptr)
                result.put(each);
        });
    return result;
}
constexpr property_set about_roles(const property_set& set)
{
    auto result = property_set::roots();
    set.for_each(
        [&result](tag each)
        {
            if (each.role != nullptr)
                result.put(each);
        });
    return result;
}

/// The pieces a container's reliance is written in: what a request must state, and per role, traits or tags.
constexpr property_set stated(std::initializer_list<tag> listed) { return property_set::of(listed); }
template<typename Role, typename... Traits>
constexpr property_set traits()
{ return about(role_v<Role>, trait_set_of<Traits...>()); }
template<typename Role>
constexpr property_set tags(std::initializer_list<tag> listed)
{ return about(role_v<Role>, property_set::of(listed)); }
/// A reliance from its pieces, all facts in one set: relies(stated({ change::monotone }), traits<role::element,
/// trait::can_hash>()).
constexpr property_set relies(const auto&... pieces)
{
    auto result = property_set::roots();
    (pieces.for_each([&result](tag each) { result.put(each); }), ...);
    return result;
}

}  // namespace urd::detail
