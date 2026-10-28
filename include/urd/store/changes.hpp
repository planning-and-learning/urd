#pragma once

#include "urd/store/types.hpp"

namespace urd::detail::facet
{

/// A change log: what was added and removed since clear_changes(), exactly (an element added and removed again is in
/// neither), as spans in the order the changes happened.
template<typename Derived, typename R, typename S>
class changes
{
public:
    [[nodiscard]] auto added() const { return representation().added(); }
    [[nodiscard]] auto removed() const { return representation().removed(); }
    /// Starts a new log from the contents as they are.
    void clear_changes() { store_access::representation(static_cast<Derived&>(*this)).clear_changes(); }

private:
    [[nodiscard]] const R& representation() const { return store_access::representation(static_cast<const Derived&>(*this)); }
};

}  // namespace urd::detail::facet
