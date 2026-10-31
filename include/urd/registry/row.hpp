#pragma once

// A registered container as data: its row. What a container guarantees depends on the elements it is given (a tree
// sorts only what urd::Less orders, a dense set takes only indices), so a row's properties are a function of the
// request's element features, not a constant: one ordinary function per container, which selection calls as a value.
// Choosing among the rows is then constant evaluation, and only the container chosen is ever instantiated
// (claude.layout.md). The features are computed once per request (urd/registry/features.hpp).

#include "urd/property_set.hpp"

#include <initializer_list>
#include <optional>
#include <string_view>

namespace urd::detail
{

/// What a type of a request's elements is, as far as any container's guarantees depend on it.
struct type_features
{
    bool present = false;                                    ///< not void
    bool hashable = false;                                   ///< by urd::Hash and urd::EqualTo
    bool orderable = false;                                  ///< by urd::Less
    bool ordered_by_request = false;                         ///< by the request's order_by comparator (natural_order<P, T>)
    bool indexable = false;                                  ///< an index 0..n-1 (index_of)
    bool trivially_copyable = false;                         ///< as a record stored flat may be (a Store is not)
    bool trivially_destructible = false;                     ///< cleared without destructors (a Store is not)
    bool slot_value = false;                                 ///< default-constructible and movable, as a generation map's slots need
    bool flat_record = false;                                ///< serialized by cista into an arena
    bool integral_priority = false;                          ///< a bucket queue's priority under urd::Less is integral
    bool integral_priority_by_request = false;               ///< and under the request's comparator
    bool store = false;                                      ///< a Store: a row
    property_set store_properties = property_set::roots();   ///< a row's request
    property_set store_requirement = property_set::roots();  ///< what a container must guarantee for a row's request
    bool store_of_stores = false;                            ///< a row whose elements are rows
    bool flattenable = false;                                ///< frozen rows a jagged array holds
    bool fixed_rows = false;                                 ///< rows a jagged array changes in place
    bool row_vector_hashable = false;                        ///< a row's elements, as a vector, are hashable (an interner keys by them)
    bool row_element_hashable = false;                       ///< a row's elements are hashable
    bool row_element_indexable = false;                      ///< a row's elements are indices
};

/// What a request's elements are: its element type (a set's key, a sequence's or pool's value), or a map's key and
/// mapped types; and its order_by comparator, where it orders entries by their values too.
struct request_features
{
    type_features element;
    type_features key;
    type_features mapped;
    bool entry_less = false;
};

/// A row's properties for given element features, or none where it does not take these elements.
using row_function = std::optional<property_set> (*)(const request_features&);

/// A registered container as data. `rows`: what it guarantees the Stores it holds (element_traits), or none where it
/// gives them their own, but for memory.
struct registry_row
{
    std::string_view id;
    row_function traits = nullptr;
    row_function rows = nullptr;
    property_set relies_on = property_set::roots();
};

/// The tags, at most one per category, the others at their roots.
constexpr property_set props(std::initializer_list<tag> listed) { return property_set::of(listed); }

/// `base` with `listed` put in their categories.
constexpr property_set with(property_set base, std::initializer_list<tag> listed)
{
    for (const auto each : listed)
        base.put(each);
    return base;
}

/// What the interface reads of properties: whether a container keeps elements by value, is a queue, is a map.
constexpr bool keeps_by_value(const property_set& of) { return of.has(tag::sorted) || of.has(tag::unique) || of.has(tag::multi); }
constexpr bool is_queue_properties(const property_set& of) { return of.in(tag::any_access) != tag::any_access; }
constexpr bool is_map_properties(const property_set& of) { return of.has(tag::functional) || of.has(tag::multi); }

}  // namespace urd::detail
