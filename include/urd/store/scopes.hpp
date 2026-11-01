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
    /// Whether `key` is bound and the innermost open scope bound it (with no scope open, whether it is bound): a
    /// redeclaration check.
    template<typename Key>
    [[nodiscard]] bool bound_in_scope(const Key& key) const
    // A string literal key decays to the pointer a heterogeneous lookup takes.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    { return store_access::representation(static_cast<const Derived&>(*this)).bound_in_scope(key); }

private:
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::scopes> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, scoped>; }
};
template<>
struct facet_operations<facet::scopes>
{
    using offered = operation_list<operation::scope>;
};

}  // namespace urd::detail
