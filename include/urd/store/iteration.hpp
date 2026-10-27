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
