#pragma once

// The segmented vector: its definition, declared with its traits in urd/repr/segmented_fwd.hpp.

#include "urd/registry/row.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/segmented_fwd.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/repr/segmented.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<"segmented_vector", S>
{
    using type = repr::SegmentedVector<typename S::element_type>;
};
template<typename S>
struct representation_of<"concurrent_segmented_vector", S>
{
    using type = repr::SegmentedVector<typename S::element_type, repr::default_first_segment_size, true>;
};

}  // namespace urd::detail
