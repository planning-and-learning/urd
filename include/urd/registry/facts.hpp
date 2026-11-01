#pragma once

// The tags and type traits by role (role_tags) that a request promises and a container relies on: what a Store
// states, and what its data is (urd/data_tags.hpp), its elements, order, values and rows.

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

/// What a type is, for the containers that hold it: derived from the types (urd/data_tags.hpp), never stated. What a
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

/// Tags and traits by role: a request's (what it states, what its data is) or a container's (what it relies on). A
/// role a kind has not holds none: nothing promised, nothing relied on.
struct role_tags
{
    property_set stated = property_set::roots();  ///< the Store's own tags
    trait_set element {};                            ///< what its elements (a map's keys) are
    trait_set order {};                              ///< what its order is
    trait_set mapped {};                             ///< what a map's values are
    property_set row = property_set::roots();     ///< its elements' own tags, where they are Stores (rows)
    trait_set rows {};                               ///< what the rows allow
    trait_set row_element {};                        ///< what the rows' elements are
};
/// Whether `held` promises, role by role, what `needed` relies on.
constexpr bool promises(const role_tags& held, const role_tags& needed)
{
    return promises(held.stated, needed.stated) && promises(held.element, needed.element) && promises(held.order, needed.order)
           && promises(held.mapped, needed.mapped) && promises(held.row, needed.row) && promises(held.rows, needed.rows)
           && promises(held.row_element, needed.row_element);
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

