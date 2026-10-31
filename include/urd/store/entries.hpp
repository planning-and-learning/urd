#pragma once

#include "urd/store/types.hpp"

#include <cstddef>

namespace urd::detail::facet
{

/// A map in insertion order: its keys and its values as ranges in that order, and an entry's position.
template<typename Derived, typename R, typename S>
class entries
{
public:
    [[nodiscard]] auto keys() const { return constant().keys(); }
    [[nodiscard]] auto values() const { return constant().values(); }
    /// The position of the entry at `it`; size() for end(), as find gives for an absent key.
    [[nodiscard]] std::size_t position(typename iterator_types<R, S>::const_iterator it) const { return static_cast<std::size_t>(it - constant().begin()); }

private:
    [[nodiscard]] const R& constant() const { return store_access::representation(static_cast<const Derived&>(*this)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::entries>
{
    template<typename R, typename S>
    static constexpr bool on()
    { return S::is_map && has<S, insertion_ordered>; }
};

}  // namespace urd::detail
