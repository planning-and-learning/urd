#pragma once

#include "urd/store/types.hpp"

#include <cstddef>

namespace urd::detail::facet
{

/// Scoped: changes made after push_scope are undone by the matching pop_scope, so an inner binding shadows an outer
/// one until its scope ends.
template<typename Derived, typename R, typename S>
class scopes
{
public:
    void push_scope() { representation().push_scope(); }
    /// Undoes every change since the matching push_scope; a scope must be open.
    void pop_scope() { representation().pop_scope(); }
    /// How many scopes are open.
    [[nodiscard]] std::size_t depth() const { return store_access::representation(static_cast<const Derived&>(*this)).depth(); }

private:
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet
