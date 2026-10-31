#pragma once

// How a Store is constructed, as its request decides: whether it has a default constructor (a nonempty Store has
// none), whether it is built from a shape (fixed_shape rows), whether what it was built from must hold an element
// (nonempty), and whether its contents fix the order its elements come in, so that it hashes and orders by them
// (sorted, canonical, insertion_ordered, a sequence). The Store's constructors and its Hash and Less read these
// (store.hpp); a constructor cannot be a facet's, as only the Store initializes its representation.

#include "urd/store/types.hpp"

namespace urd::detail
{

template<typename S>
struct construction
{
    /// Whether it may start empty.
    static constexpr bool default_constructible = !has<S, nonempty>;
    /// Whether it is built from a shape (row lengths) and never from elements.
    static constexpr bool by_shape = has<S, fixed_shape>;
    /// Whether what it holds after construction must be an element at least.
    static constexpr bool needs_an_element = has<S, nonempty>;
    /// Whether its contents fix the order its elements come in: hashed by it (sorted, canonical, a sequence).
    static constexpr bool hashed_in_order = S::iterable && (has<S, sorted> || has<S, canonical> || S::is_sequence);
    /// Whether it is iterated in an order the request fixes: ordered by it (sorted, insertion_ordered, a sequence).
    static constexpr bool ordered_in_order = store_types<void, S>::ordered;
};

}  // namespace urd::detail
