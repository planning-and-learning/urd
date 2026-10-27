#pragma once

#include "urd/traits.hpp"

#include <vector>

namespace urd
{

template<typename T, typename Allocator>
struct container_traits<std::vector<T, Allocator>> : properties_of<dense, contiguous, shrinkable, retains, proportional_clear>
{
};

}  // namespace urd
