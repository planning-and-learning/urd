#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/registry/row.hpp"
#include "urd/traits.hpp"

#include <vector>

namespace urd
{

template<typename T, typename Allocator>
struct container_traits<std::vector<T, Allocator>> : properties_of<dense, positional, contiguous, shrinkable, retains, proportional_clear>
{
};

/// Defined by the standard; this header is its own, so that every representation has one.
template<typename T, typename Allocator>
inline constexpr auto defined_in<std::vector<T, Allocator>> = detail::fixed_string("urd/repr/sequence.hpp");

}  // namespace urd

namespace urd
{

/// vector.
template<typename T, typename A>
struct container_costs<std::vector<T, A>>
{
    static constexpr cost insert = costs::one_a;
    static constexpr cost erase = costs::lin_n;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 1.5;
};

}  // namespace urd

namespace urd
{

template<typename T, typename A>
struct container_synopsis<std::vector<T, A>>
{
    static constexpr std::string_view id = "vector";
    static constexpr std::string_view description = "std::vector";
    static constexpr std::string_view holds = "one array; retains";
};

}  // namespace urd

namespace urd
{

template<>
struct include<"urd/repr/sequence.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<"vector", S>
{
    using type = std::vector<typename S::element_type>;
};

}  // namespace urd::detail
