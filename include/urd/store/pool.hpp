#pragma once

#include "urd/index.hpp"
#include "urd/store/types.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <ranges>
#include <span>

namespace urd::detail::facet
{

/// A pool: rows go in, handles come out; equal rows get equal handles, so a handle stands for its row (a state, a
/// fact set) in comparisons and as a key. The rows come back by handle.
template<typename Derived, typename R, typename S>
class pool
{
    using row_element = std::ranges::range_value_t<typename S::element_type>;
    /// Rows that are sequences take writes by position.
    static constexpr bool sequence_rows = !by_value<traits_t<typename S::element_type>>;

public:
    using handle_type = typename R::handle_type;
    using row_type = typename S::element_type;
    /// A change to a sequence row: its value at a position.
    using write_type = Write<row_element>;

    /// The handle of the row with these elements: an earlier handle where an equal row is stored.
    template<std::ranges::input_range Elements>
    handle_type insert(const Elements& elements)
    {
        return representation().insert(elements);
    }
    handle_type insert(std::initializer_list<row_element> elements) { return representation().insert(elements); }
    /// The handle of the row `base` with `writes` applied: sorted by position, each position once and within the row.
    /// A pool that shares structure stores only what changed.
    handle_type insert(handle_type base, std::span<const Write<row_element>> writes)
        requires sequence_rows
    {
        assert(std::ranges::is_sorted(writes, {}, &Write<row_element>::position));
        assert(std::ranges::adjacent_find(writes, {}, &Write<row_element>::position) == writes.end());
        assert(writes.empty() || writes.back().position < row_size(base));
        return representation().insert(base, writes);
    }

    /// The handle of the row `base` with `added` put in and then `removed` taken out (rows that are sets). A pool that
    /// shares structure stores only what changed.
    handle_type insert(handle_type base, std::span<const row_element> added, std::span<const row_element> removed)
        requires(!sequence_rows)
    {
        return representation().insert(base, added, removed);
    }
    /// Whether the row of `handle` holds `element` (rows that are sets); a tree pool reads one path, not the row.
    [[nodiscard]] bool contains(handle_type handle, const row_element& element) const
        requires(!sequence_rows)
    {
        return constant().contains(handle, element);
    }

    [[nodiscard]] row_type get(handle_type handle) const { return constant().get(handle); }
    /// The stored row without copying, where the pool keeps rows in full: a span of the elements, or for a dense set
    /// its bits. Valid until the pool is cleared.
    [[nodiscard]] auto view(handle_type handle) const
        requires requires(const R& rows) { rows.view(handle); }
    {
        return constant().view(handle);
    }
    [[nodiscard]] std::size_t row_size(handle_type handle) const { return constant().row_size(handle); }
    /// The row's elements into `out`, which must hold row_size(handle) of them; allocates nothing.
    void read(handle_type handle, std::span<row_element> out) const { constant().read(handle, out); }
    /// The bytes the stored rows take.
    [[nodiscard]] std::size_t memory_usage() const { return constant().memory_usage(); }
    /// Forgets every row; the handles given so far mean nothing after.
    void clear()
        requires has<S, shrinkable>
    {
        representation().clear();
    }

private:
    [[nodiscard]] const R& constant() const { return store_access::representation(static_cast<const Derived&>(*this)); }
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet
