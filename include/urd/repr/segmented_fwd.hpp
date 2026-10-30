#pragma once

// A vector in segments that never move, declared with its defaults and what the registry reads of it. Defined in
// urd/repr/containers/segmented_vector.hpp, which urd/repr/segmented.hpp and urd/use/sequences.hpp include.

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
inline constexpr auto defined_in<repr::SegmentedVector<T, FirstSegmentSize, ThreadSafe, Index>> = detail::fixed_string("urd/use/sequences.hpp");

}  // namespace urd
