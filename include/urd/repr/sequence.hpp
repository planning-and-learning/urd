#pragma once

#include "urd/properties.hpp"
#include "urd/traits.hpp"

#include <string_view>
#include <vector>

namespace urd
{

template<typename T, typename Allocator>
struct container_traits<std::vector<T, Allocator>> : properties_of<dense, contiguous, shrinkable>
{
};

}  // namespace urd

namespace urd::repr
{

/// A sequence in insertion order: std::vector.
template<typename S>
struct sequence_vector
{
    using type = std::vector<typename S::value_type>;
    static constexpr std::string_view name = "std::vector";
    static constexpr bool accepts = S::is_sequence;
    static constexpr int rank = 0;
};

}  // namespace urd::repr
