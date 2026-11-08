#pragma once

#include "urd/store/types.hpp"

#include <cassert>
#include <cstddef>

namespace urd::detail
{

/// Whether a layered container chains its layers (a pool continues its parent's indices, from its base): its Store
/// then knows which layer owns an index (layers::owner).
template<typename R>
concept chains_layers = requires(const R& representation) { representation.base(); };

}  // namespace urd::detail

namespace urd::detail::facet
{

/// Layered: lookups find a parent Store's elements too. The parent is not owned or changed; it must outlive this
/// Store and stay unchanged while layered over. A layered pool continues the parent's indices, numbers its layer
/// along the chain, and knows which layer owns an index, so that a view of an element points at the Store that
/// holds it; a layered map knows which layer owns a key.
template<typename Derived, typename R, typename S>
class layers
{
    static constexpr bool chained = chains_layers<R>;

public:
    /// Answers from `parent` too, from now on; nullptr ends it (a pool: makes it a root again, while empty).
    void layer_over(const Derived* parent)
    {
        representation().layer_over(parent == nullptr ? nullptr : &store_access::representation(*parent));
        m_parent = parent;
    }
    [[nodiscard]] const Derived* parent() const noexcept { return m_parent; }
    /// Where this layer's own indices start: the parent's size at the branch.
    [[nodiscard]] std::size_t base() const
        requires chained
    { return constant().base(); }
    /// Whether the parent grew after the branch, which it must not.
    [[nodiscard]] bool parent_grew() const
        requires chained
    { return constant().parent_grew(); }
    /// The Store along the chain that holds the element at `at`: this one, or an ancestor.
    template<typename Index>
    [[nodiscard]] const Derived& owner(Index at) const
        requires chained
    {
        const auto* layer = static_cast<const Derived*>(this);
        while ((at.value) < store_access::representation(*layer).base())
            layer = layer->m_parent;
        return *layer;
    }
    /// The Store along the chain whose own values hold `key` (a layered map): this one, or an ancestor. Some layer must
    /// hold it (contains(key)).
    template<typename Key>
    [[nodiscard]] const Derived& owner(const Key& key) const
        requires(!chained)
    {
        const auto* layer = static_cast<const Derived*>(this);
        while (!store_access::representation(*layer).owns(key))
        {
            layer = layer->m_parent;
            assert(layer != nullptr && "urd::Store::owner: no layer holds the key");
        }
        return *layer;
    }
    /// read(value) with this layer's own value of `key` (a layered map), else `missing`: the parent's are not looked at.
    template<typename Key, typename Read, typename Result>
    [[nodiscard]] Result read_own(const Key& key, Read read, Result missing) const
        requires(!chained)
    { return constant().read_own(key, read, missing); }

private:
    [[nodiscard]] const R& constant() const { return store_access::of<Derived>(*this); }
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }

    const Derived* m_parent = nullptr;
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::layers> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, layered>; }
};

}  // namespace urd::detail
