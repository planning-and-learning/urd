#pragma once

#include "urd/store/types.hpp"

#include <cassert>
#include <iterator>

namespace urd::detail::facet
{

/// A sorted set or map: its least and greatest elements (a map's entries with the least and greatest keys), the
/// first and last in order, O(1) in a tree or a sorted array. Dense keys are left out: their first is found by a
/// scan of the key range.
template<typename Derived, typename R, typename S>
class sorted_ends
{
public:
    /// The Store must not be empty.
    [[nodiscard]] decltype(auto) front() const
    {
        assert(!self().empty());
        return *self().begin();
    }
    [[nodiscard]] decltype(auto) back() const
    {
        assert(!self().empty());
        return *std::prev(self().end());
    }

private:
    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
/// Sequences and positional sets have front and back by position (positions).
template<>
struct facet_condition<facet_id::sorted_ends>
{
    template<typename R, typename S>
    static constexpr bool on()
    { return (S::is_set || S::is_map) && has<S, sorted> && !has<S, dense> && !has<S, positional>; }
};

}  // namespace urd::detail
