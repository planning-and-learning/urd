#pragma once

// The segmented vector: its definition, declared with its traits in urd/repr/sequences/segmented_fwd.hpp.

#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/sequences/segmented_fwd.hpp"
#include "urd/traits.hpp"

template<>
struct urd::include<"urd/repr/sequences/segmented.hpp">
{
};

namespace urd::detail
{

template<typename S>
struct representation_of<segmented_vector_decl, S>
{
    using type = repr::SegmentedVector<typename S::element_type, repr::first_segment_size_of<typename S::element_type>>;
};
template<typename S>
struct representation_of<concurrent_segmented_vector_decl, S>
{
    using type = repr::SegmentedVector<typename S::element_type, repr::first_segment_size_of<typename S::element_type>, true>;
};

}  // namespace urd::detail
