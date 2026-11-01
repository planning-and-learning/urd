#pragma once

#include "urd/store/types.hpp"

#include <cassert>
#include <iterator>

namespace urd::detail::facet
{

/// A Store iterated in an order its request fixes: its first and last elements (a map's entries with the least and
/// greatest keys), changeable in place where its elements are. Dense keys without positions are left out: their first
/// is found by a scan of the key range.
template<typename Derived, typename R, typename S>
class ends
{
    using reference_type = typename iterator_types<R, S>::reference;
    using const_reference_type = typename iterator_types<R, S>::const_reference;
    static constexpr bool writable = store_types<S>::writable;

public:
    /// The first element; a nonempty Store always has one, any other needs one.
    [[nodiscard]] const_reference_type front() const
    {
        assert(!self().empty());
        return *self().begin();
    }
    [[nodiscard]] reference_type front()
        requires writable
    {
        assert(!self().empty());
        return *self().begin();
    }
    [[nodiscard]] const_reference_type back() const
    {
        assert(!self().empty());
        return *std::prev(self().end());
    }
    [[nodiscard]] reference_type back()
        requires writable
    {
        assert(!self().empty());
        return *std::prev(self().end());
    }

private:
    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
    [[nodiscard]] Derived& self() { return static_cast<Derived&>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: it is iterated in an order its request fixes, and its first element is at hand (not
/// dense keys without positions).
template<>
struct facet_condition<facet::ends> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return store_types<S>::ordered && (has<S, positional> || !has<S, dense>); }
};

}  // namespace urd::detail
