#pragma once

#include "urd/store/types.hpp"

namespace urd::detail::facet
{

/// Layered: lookups find a parent Store's elements too. The parent is not owned or changed; it must outlive this
/// Store and stay unchanged while layered over.
template<typename Derived, typename R, typename S>
class layers
{
public:
    /// Answers from `parent` too, from now on; nullptr ends it.
    void layer_over(const Derived* parent)
    { store_access::representation(static_cast<Derived&>(*this)).layer_over(parent == nullptr ? nullptr : &store_access::representation(*parent)); }
};

}  // namespace urd::detail::facet
