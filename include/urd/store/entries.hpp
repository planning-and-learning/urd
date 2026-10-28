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
