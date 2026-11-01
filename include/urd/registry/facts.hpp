#pragma once

// The tags and type traits by role (property_set) that a request promises and a container relies on: what a Store
// states, and what its data is (urd/type_traits_of.hpp), its elements, order, values and rows. Type traits and roles
// are open, as categories are: each is a struct with its words.

#include "urd/detail/type_list.hpp"
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

/// The traits of a role (a struct of the role's), as `using type = detail::type_list<Trait…>`: what its facts can be.
/// Specialized once per role, where its traits are defined, each a struct with its words and what it means
/// (`holds<…>()`); never defined otherwise, so a role whose list is not visible does not compile.
template<typename Role>
struct traits_of_role;

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
/// What a Store is for role Role, where it answers for itself (as an element, as what an order compares): `value`,
/// its facts, from its request alone, so that it is never completed to answer. Specialized beside a Store's semantics
/// (urd/store/construction.hpp); where it is not, the role's traits are asked as of any type (a row's).
template<typename Role, typename... Args>
struct store_facts;

/// The traits of role Role that hold of Args (Trait::holds<Args…>()), about no role; a Store's own facts where it
/// answers for itself; none where an argument is void (a kind without values has no value type).
template<typename Role, typename... Args>
consteval property_set traits_holding()
{
    if constexpr ((std::is_void_v<Args> || ...))
        return property_set::roots();
    else if constexpr (requires { store_facts<Role, Args...>::value; })
        return store_facts<Role, Args...>::value;
    else
    {
        auto result = property_set::roots();
        []<typename... Traits>(type_list<Traits...> /*traits*/, property_set& into)
        { (put_trait<Traits>(into, Traits::template holds<Args...>()), ...); }(typename traits_of_role<Role>::type {}, result);
        return result;
    }
}

/// How the facts of role Role are computed for the types that play it (Args): by default the role's traits that hold
/// (or a Store's own facts). A role whose facts are something else (a row's own tags) specializes it.
template<typename Role>
struct role_facts
{
    template<typename... Args>
    static consteval property_set of()
    { return traits_holding<Role, Args...>(); }
};

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

/// urd's roles: what its tables list (a role added outside urd is a struct like these, in its own list).
using roles = detail::type_list<role::element, role::order, role::rows, role::mapped, role::row, role::row_element>;

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
