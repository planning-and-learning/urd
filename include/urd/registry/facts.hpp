#pragma once

// The facts a container's declaration reads (urd/registry/declaration.hpp): what a request's element types are
// (type_features, general to any type, recursive for Stores), what order it asks for, and what it is as a whole
// (request_features). What a container guarantees depends on these (a tree sorts only what urd::Less orders, a dense
// set takes only indices), so its offer is a function of them, evaluated as a value. They are computed once per type
// and request (urd/registry/features.hpp).

#include "urd/property_set.hpp"
#include "urd/semantics/less.hpp"
#include "urd/traits.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>

namespace urd::detail
{

/// What a type is, in facts general enough that any container states its constraints in them; for a Store, also its
/// request, its kind, and the same facts about its elements, recursively. Nothing here is one container's: a
/// container's own precondition is composed from these in its offer (urd/registry/guarantees.hpp).
struct type_features
{
    bool hashable = false;                ///< by urd::Hash and urd::EqualTo
    bool orderable = false;               ///< by urd::Less
    bool indexable = false;               ///< an index 0..n-1 (index_of)
    bool integral = false;                ///< an integral number
    bool trivially_copyable = false;      ///< copied as bytes (a Store is not)
    bool trivially_destructible = false;  ///< destroyed without code (a Store is not)
    bool default_initializable = false;   ///< default-constructible and movable (a Store unless nonempty or fixed_shape)
    bool serializable = false;            ///< names its members for cista (cista_members), so it is stored as bytes
    bool store = false;                   ///< a Store, whose request follows
    property_set store_properties = property_set::roots();
    property_set store_required = property_set::roots();
    urd::kind store_kind = urd::kind::sequence;
    /// The facts about its elements, where it is a range of them (a Store that iterates, or another range).
    const type_features* element = nullptr;
};

/// The order a request asks its elements in, beyond their own: none (urd::Less, or no order), an order_by over the
/// elements (a map's keys), or an order_by over a map's entries, key and value.
enum class order : std::uint8_t
{
    none,
    element,
    entry,
};

/// What a request's elements are: its element type (a set's key, a sequence's or pool's value), or a map's key and
/// mapped types; and its order_by comparator, where it orders entries by their values too.
struct request_features
{
    type_features element;
    type_features key;
    type_features mapped;
    order by = order::none;
    /// Where it gives order_by over its elements (a map's keys): whether that comparator orders them as `sorted`
    /// means (natural_order), and the facts about what it compares (its projection's result, else the elements).
    bool order_natural = false;
    type_features order_key;
    /// Whether hashing uses urd's hash and equality (or ones that agree): always for a request, and for a
    /// container where its hash and equality arguments are natural (natural_hash).
    bool hash_natural = true;
};

/// The orders a container takes, as a mask of urd::detail::order.
inline constexpr std::uint8_t natural_order_only = 1U << static_cast<unsigned>(order::none);
inline constexpr std::uint8_t element_orders = natural_order_only | (1U << static_cast<unsigned>(order::element));
inline constexpr std::uint8_t entry_orders = 1U << static_cast<unsigned>(order::entry);
constexpr bool accepts_order(std::uint8_t orders, order of) { return (orders & (1U << static_cast<unsigned>(of))) != 0; }

/// The comparator of a container that takes one: the request's order_by where it gives one, else urd::Less.
template<typename S>
using element_order_t = std::conditional_t<std::is_void_v<typename S::order_type>, Less<typename S::element_type>, typename S::order_type>;
template<typename S>
using key_order_t = std::conditional_t<std::is_void_v<typename S::order_type>, Less<typename S::key_type>, typename S::order_type>;

/// The tags, at most one per category, the others at their roots.
constexpr property_set props(std::initializer_list<tag> listed) { return property_set::of(listed); }

/// `base` with `listed` put in their categories.
constexpr property_set with(property_set base, std::initializer_list<tag> listed)
{
    for (const auto each : listed)
        base.put(each);
    return base;
}

}  // namespace urd::detail
