#pragma once

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <vector>

namespace urd
{

/// Properties of a range's contents. A set of them is a bitmask, so the order in which a type
/// names them does not matter and "has at least these" is a subset test.
enum class props : unsigned
{
    none = 0,
    // What the contents are; holds() checks these.
    sorted = 1U << 0U,    ///< ascending by operator<
    unique = 1U << 1U,    ///< no two elements are equal
    nonempty = 1U << 2U,  ///< at least one element
    // How elements are identified.
    positional = 1U << 3U,  ///< by position 0..n-1, in order (a sequence)
    functional = 1U << 4U,  ///< at most one value per key (a map)
    multi = 1U << 5U,       ///< any number of values per key (a multimap)
    // The key domain.
    dense = 1U << 6U,   ///< the keys are 0..n-1, or nearly so
    sparse = 1U << 7U,  ///< the keys are few compared to their range
    // Lifecycle.
    frozen = 1U << 8U,       ///< no change after construction
    monotone = 1U << 9U,     ///< only grows: nothing is ever removed
    stable_refs = 1U << 10U, ///< a reference to an element stays valid until that element is removed (erase, clear)
    persistent = 1U << 11U,  ///< a change makes a new version and old versions stay valid
    // Representation.
    contiguous = 1U << 12U,  ///< the elements lie next to each other in memory
    canonical = 1U << 13U,   ///< equal contents have equal representations
    // Concurrency.
    /// Inserts may run concurrently with each other and with reads of elements already published. Not covered:
    /// erase, clear and iteration, which need no other operation running.
    concurrent = 1U << 14U,
};

[[nodiscard]] constexpr props operator|(props lhs, props rhs) noexcept
{
    return static_cast<props>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
}

[[nodiscard]] constexpr props operator&(props lhs, props rhs) noexcept
{
    return static_cast<props>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs));
}

/// Whether `have` contains every property of `want`.
[[nodiscard]] constexpr bool includes(props have, props want) noexcept { return (have & want) == want; }

/// The properties every value of type T has. Only a Store has any: its properties are those it was asked for.
template<typename T>
struct properties_of
{
    static constexpr props value = props::none;
};

template<typename R>
inline constexpr props properties_of_v = properties_of<std::remove_cvref_t<R>>::value;

/// A range whose type guarantees the properties P.
template<typename R, props P>
concept has_props = std::ranges::forward_range<R> && includes(properties_of_v<R>, P);

/// Whether the contents of `range` have the content properties of P (sorted, unique, nonempty); the
/// others are guarantees of a type, which conformance tests check instead. O(n log n) at most.
template<props P, std::ranges::forward_range R>
[[nodiscard]] bool holds(const R& range)
{
    if (includes(P, props::nonempty) && std::ranges::empty(range))
        return false;
    if (includes(P, props::sorted) && !std::ranges::is_sorted(range))
        return false;
    if (includes(P, props::unique))
    {
        if (includes(P, props::sorted))
            return std::ranges::adjacent_find(range) == std::ranges::end(range);
        auto copy = std::vector<std::ranges::range_value_t<R>>(std::ranges::begin(range), std::ranges::end(range));
        std::ranges::sort(copy);
        return std::ranges::adjacent_find(copy) == copy.end();
    }
    return true;
}

}  // namespace urd
