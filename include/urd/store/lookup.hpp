#pragma once

#include "urd/store/types.hpp"

namespace urd::detail::facet
{

/// Kept by value or key: finding elements, as the standard's associative containers do.
template<typename Derived, typename R, typename S>
class lookup
{
    using types = store_types<R, S>;

public:
    /// What lookups take: a map's key, else an element, as key_type is for the standard's sets.
    using key_type = typename types::lookup_type;
    using lookup_type = typename types::lookup_type;
    /// What a map's keys map to; void for a set or a sorted sequence.
    using mapped_type = typename types::mapped_type;

    [[nodiscard]] typename iterator_types<R, S>::const_iterator find(const typename types::lookup_type& element) const
    { return representation().find(element); }
    [[nodiscard]] typename types::size_type count(const typename types::lookup_type& element) const { return representation().count(element); }
    [[nodiscard]] bool contains(const typename types::lookup_type& element) const { return representation().contains(element); }

private:
    [[nodiscard]] const R& representation() const { return store_access::representation(static_cast<const Derived&>(*this)); }
};

}  // namespace urd::detail::facet
