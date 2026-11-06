#pragma once

#include "urd/store/types.hpp"

#include <iterator>
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

    [[nodiscard]] size_type size() const { return std::ranges::size(store_access::of<Derived>(*this)); }
    [[nodiscard]] bool empty() const { return size() == 0; }

    /// Rebuilds from `first`, `last` in the storage it holds, where its container can (a frozen multimap's rows):
    /// what building a new one would give, without allocating once warm.
    /// The most elements the representation can hold, as the standard containers' max_size.
    [[nodiscard]] std::size_t max_size() const
        requires requires(const R& held) { held.max_size(); }
    { return store_access::of<Derived>(*this).max_size(); }
    /// How many elements fit without growing, where the representation tells.
    [[nodiscard]] std::size_t capacity() const
        requires requires(const R& held) { held.capacity(); }
    { return store_access::of<Derived>(*this).capacity(); }
    /// Gives back unused room, where the representation can.
    void shrink_to_fit()
        requires requires(R& held) { held.shrink_to_fit(); }
    { store_access::of<Derived>(*this).shrink_to_fit(); }
    template<std::input_iterator InputIt>
        requires requires(R& held, InputIt at) { held.assign(at, at); }
    void assign(InputIt first, InputIt last)
    { store_access::of<Derived>(*this).assign(first, last); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: where its container counts its elements (a pool that shares parts of its rows keeps
/// no count); asked from the spec alone (R void), never.
template<>
struct facet_condition<facet::sizing> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    {
        if constexpr (std::is_void_v<R>)
            return false;
        else
            return requires(const R& range) { std::ranges::size(range); };
    }
};

}  // namespace urd::detail
