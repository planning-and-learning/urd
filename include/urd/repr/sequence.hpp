#pragma once

#include "urd/traits.hpp"

#include <vector>

namespace urd
{

template<typename T, typename Allocator>
struct container_traits<std::vector<T, Allocator>> : properties_of<dense, positional, contiguous, shrinkable, retains, proportional_clear>
{
};

/// Defined by the standard; its family header exists so that every representation has one.
template<typename T, typename Allocator>
inline constexpr auto defined_in<std::vector<T, Allocator>> = detail::fixed_string("urd/use/sequences.hpp");

}  // namespace urd
