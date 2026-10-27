#pragma once

#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace urd
{

/// Segments never move, so references stay valid; it appends but cannot remove from the middle, and clear keeps
/// the segments. The thread-safe variant appends concurrently.
template<typename T, std::size_t FirstSegmentSize, bool ThreadSafe, std::unsigned_integral Index>
struct container_traits<repr::SegmentedVector<T, FirstSegmentSize, ThreadSafe, Index>> :
    properties_of<dense, stable_refs, growable, retains, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

}  // namespace urd
