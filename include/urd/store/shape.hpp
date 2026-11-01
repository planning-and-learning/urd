#pragma once

#include "urd/store/types.hpp"

#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

namespace urd
{

/// How long each row of a fixed_shape Store is, in order: what such a Store is built from (its kind says so,
/// urd/kinds.hpp: builds_from). A row container takes the lengths as they are.
struct row_lengths
{
    std::vector<std::size_t> lengths;

    row_lengths(std::initializer_list<std::size_t> each) : lengths(each) {}
    explicit row_lengths(std::vector<std::size_t> each) : lengths(std::move(each)) {}
    operator const std::vector<std::size_t>&() const { return lengths; }  // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
};

}  // namespace urd

namespace urd::detail::facet
{

/// A fixed shape: its rows' contents go back to their defaults, the shape stays.
template<typename Derived, typename R, typename S>
class shape
{
public:
    /// Every element back to its default, every bit unset; with proportional_clear, in the rows handed out to change
    /// since the last reset only.
    void reset() { store_access::representation(static_cast<Derived&>(*this)).reset(); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::shape> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, fixed_shape>; }
};
template<>
struct facet_operations<facet::shape>
{
    using offered = operation_list<operation::access>;
};

}  // namespace urd::detail
