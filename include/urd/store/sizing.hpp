#pragma once

#include "urd/store/types.hpp"

#include <ranges>
#include <type_traits>

namespace urd::detail::facet
{

/// Sized: how many elements there are, where the container counts them.
template<typename Derived, typename R, typename S>
class sizing
{
public:
    using size_type = typename store_types<S>::size_type;

    [[nodiscard]] size_type size() const { return store_access::of<Derived>(*this).size(); }
    [[nodiscard]] bool empty() const { return store_access::of<Derived>(*this).empty(); }

    // Of contiguous storage (urd::contiguous), as std::vector's.
    /// The most elements the representation can hold.
    [[nodiscard]] std::size_t max_size() const
        requires has<R, urd::contiguous>
    { return store_access::of<Derived>(*this).max_size(); }
    /// How many elements fit without growing.
    [[nodiscard]] std::size_t capacity() const
        requires has<R, urd::contiguous>
    { return store_access::of<Derived>(*this).capacity(); }
    /// Gives back unused room.
    void shrink_to_fit()
        requires has<R, urd::contiguous>
    { store_access::of<Derived>(*this).shrink_to_fit(); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: where its container counts its elements (a pool that shares parts of its rows keeps
/// no count); asked from the spec alone (R void), never. Its member is asked, not std::ranges::size, which would have
/// the compiler build the count of a std::vector whose element type is still incomplete.
template<>
struct facet_condition<facet::sizing> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    {
        if constexpr (std::is_void_v<R>)
            return false;
        else
            return requires(const R& range) { range.size(); };
    }
};

}  // namespace urd::detail
