#pragma once

// A vector in segments that never move, declared with its defaults and what the registry reads of it. Defined in
// urd/repr/containers/segmented_vector.hpp, which urd/repr/segmented.hpp includes.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/repr/containers/segment_sizes.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace urd::repr
{

/// Elements in segments of geometrically growing size that never move once allocated. ThreadSafe permits concurrent
/// appends and reads of published elements; Index bounds the number of addressable elements.
template<typename T, std::size_t FirstSegmentSize = default_first_segment_size, bool ThreadSafe = false, std::unsigned_integral Index = std::size_t>
class SegmentedVector;

}  // namespace urd::repr

namespace urd
{

/// Segments never move, so references stay valid; it appends but cannot remove from the middle, and clear keeps
/// the segments.
template<typename T, std::size_t FirstSegmentSize, bool ThreadSafe, std::unsigned_integral Index>
struct container_traits<repr::SegmentedVector<T, FirstSegmentSize, ThreadSafe, Index>> :
    properties_of<dense, positional, stable_refs, growable, retains, proportional_clear, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

template<typename T, std::size_t FirstSegmentSize, bool ThreadSafe, std::unsigned_integral Index>
inline constexpr auto defined_in<repr::SegmentedVector<T, FirstSegmentSize, ThreadSafe, Index>> = detail::fixed_string("urd/repr/segmented.hpp");

}  // namespace urd

namespace urd
{

/// segmented_vector.
template<typename T, std::size_t N, typename I>
struct container_costs<repr::SegmentedVector<T, N, false, I>>
{
    static constexpr cost insert = costs::one_a;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 1.5;
};

/// concurrent_segmented_vector.
template<typename T, std::size_t N, typename I>
struct container_costs<repr::SegmentedVector<T, N, true, I>>
{
    static constexpr cost insert = costs::one_a;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 1.6;
};

}  // namespace urd

namespace urd
{

template<typename T, std::size_t N, typename I>
struct container_synopsis<repr::SegmentedVector<T, N, false, I>>
{
    static constexpr std::string_view id = "segmented_vector";
    static constexpr std::string_view description = "segmented vector (elements never move)";
    static constexpr std::string_view holds = "doubling segments; stable references";
};

template<typename T, std::size_t N, typename I>
struct container_synopsis<repr::SegmentedVector<T, N, true, I>>
{
    static constexpr std::string_view id = "concurrent_segmented_vector";
    static constexpr std::string_view description = "segmented vector, thread-safe appends";
    static constexpr std::string_view holds = "doubling segments; stable references; appends from threads";
};

}  // namespace urd
