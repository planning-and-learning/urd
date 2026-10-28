#pragma once

#include "urd/store/types.hpp"

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

public:
    using handle_type = typename R::handle_type;
    using row_type = typename S::element_type;

    /// The handle of the row with these elements: an earlier handle where an equal row is stored.
    template<std::ranges::input_range Elements>
    handle_type insert(const Elements& elements)
    { return representation().insert(elements); }
    handle_type insert(std::initializer_list<row_element> elements) { return representation().insert(elements); }

    [[nodiscard]] row_type get(handle_type handle) const { return constant().get(handle); }
    [[nodiscard]] std::size_t row_size(handle_type handle) const { return constant().row_size(handle); }
    /// The row's elements into `out`, which must hold row_size(handle) of them; allocates nothing.
    void read(handle_type handle, std::span<row_element> out) const { constant().read(handle, out); }
    /// The bytes the stored rows take.
    [[nodiscard]] std::size_t memory_usage() const { return constant().memory_usage(); }
    /// Forgets every row; the handles given so far mean nothing after.
    void clear()
        requires has<S, shrinkable>
    { representation().clear(); }

private:
    [[nodiscard]] const R& constant() const { return store_access::representation(static_cast<const Derived&>(*this)); }
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet
