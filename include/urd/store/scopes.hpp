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
    {
        return store_access::representation(static_cast<const Derived&>(*this)).bound_in_scope(key);
    }

private:
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::scopes>
{
    static constexpr bool reads_container = false;
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, scoped>; }
};

}  // namespace urd::detail
