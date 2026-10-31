#pragma once

#include "urd/store/types.hpp"

namespace urd::detail::facet
{

/// Iterable (sets, sequences and maps): the elements in turn, changeable in place where the Store's elements
/// are. Kinds whose elements are reached otherwise, such as a queue by its front, leave it off.
template<typename Derived, typename R, typename S>
class iteration
{
    using types = store_types<R, S>;

public:
    using iterator = typename iterator_types<R, S>::iterator;
    using const_iterator = typename iterator_types<R, S>::const_iterator;
    using reference = typename iterator_types<R, S>::reference;
    using const_reference = typename iterator_types<R, S>::const_reference;
    using difference_type = typename store_types<R, S>::difference_type;

    [[nodiscard]] const_iterator begin() const { return representation().begin(); }
    [[nodiscard]] const_iterator end() const { return representation().end(); }
    [[nodiscard]] iterator begin()
        requires types::writable
    { return store_access::representation(static_cast<Derived&>(*this)).begin(); }
    [[nodiscard]] iterator end()
        requires types::writable
    { return store_access::representation(static_cast<Derived&>(*this)).end(); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend() const { return end(); }

private:
    [[nodiscard]] const R& representation() const { return store_access::representation(static_cast<const Derived&>(*this)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
/// A queue visits its elements in the order pushed, or a priority queue in none; not a bucket queue
/// (monotone_priority), which would visit every priority's bucket. Iterated so, a queue is still no
/// range for the facets that read S::iterable (equality, growth, removal).
template<>
struct facet_condition<facet_id::iteration>
{
    template<typename R, typename S>
    static constexpr bool on()
    { return S::iterable || (S::is_queue && !has<S, monotone_priority>); }
};

}  // namespace urd::detail
