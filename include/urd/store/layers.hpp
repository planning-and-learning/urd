#pragma once

#include "urd/store/types.hpp"

#include <cstddef>

namespace urd::detail::facet
{

/// Layered: lookups find a parent Store's elements too. The parent is not owned or changed; it must outlive this
/// Store and stay unchanged while layered over. A layered pool continues the parent's indices, numbers its layer
/// along the chain, and knows which layer owns an index, so that a view of an element points at the Store that
/// holds it.
template<typename Derived, typename R, typename S>
class layers
{
    static constexpr bool chained = requires(const R& representation) { representation.base(); };

public:
    /// Answers from `parent` too, from now on; nullptr ends it (a pool: makes it a root again, while empty).
    void layer_over(const Derived* parent)
    {
        representation().layer_over(parent == nullptr ? nullptr : &store_access::representation(*parent));
        m_parent = parent;
    }
    [[nodiscard]] const Derived* parent() const noexcept { return m_parent; }
    /// This layer's number along the chain: 0 for a root.
    [[nodiscard]] std::size_t layer() const
        requires chained
    {
        return constant().layer();
    }
    /// Where this layer's own indices start: the parent's size at the branch.
    [[nodiscard]] std::size_t base() const
        requires chained
    {
        return constant().base();
    }
    /// Whether the parent grew after the branch, which it must not.
    [[nodiscard]] bool parent_grew() const
        requires chained
    {
        return constant().parent_grew();
    }
    /// The Store along the chain that holds the element at `at`: this one, or an ancestor.
    template<typename Index>
    [[nodiscard]] const Derived& owner(Index at) const
        requires chained
    {
        const auto* layer = static_cast<const Derived*>(this);
        while (at.value < store_access::representation(*layer).base())
            layer = layer->m_parent;
        return *layer;
    }

private:
    [[nodiscard]] const R& constant() const { return store_access::representation(static_cast<const Derived&>(*this)); }
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }

    const Derived* m_parent = nullptr;
};

}  // namespace urd::detail::facet
