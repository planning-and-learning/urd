#pragma once

#include "urd/cost.hpp"
#include "urd/store/types.hpp"

#include <cstddef>

namespace urd::detail::facet
{

/// A multi-index container's indexes (Boost.MultiIndex, with indexes added at run time): hashed non-unique indexes on
/// sets of a pool's columns, kept as rows are inserted and erased, and its handles in turn.
template<typename Derived, typename R, typename S>
class indexes
{
public:
    /// Iterates handles: of the rows, or of an index's rows with one key.
    using iterator = typename R::iterator;
    using const_iterator = typename R::const_iterator;

    /// The number of the index on `columns` (its key's columns, in order), added where it is new.
    template<typename Columns>
    std::size_t add_index(const Columns& columns)
    { return representation().add_index(columns); }
    /// The number of indexes added.
    [[nodiscard]] std::size_t index_count() const noexcept { return representation().index_count(); }
    /// The index of number `n`: count(key), contains(key), find(key) and equal_range(key) give its rows' handles.
    [[nodiscard]] auto index(std::size_t n) const { return representation().index(n); }
    /// The handles of the rows, 0..size()-1.
    [[nodiscard]] auto begin() const noexcept { return representation().begin(); }
    [[nodiscard]] auto end() const noexcept { return representation().end(); }

private:
    [[nodiscard]] const R& representation() const { return store_access::of<Derived>(*this); }
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::indexes> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, multi_indexed>; }
};
template<>
struct facet_operations<facet::indexes>
{
    using offered = operation_list<operation::lookup, operation::iterate>;
};

}  // namespace urd::detail
