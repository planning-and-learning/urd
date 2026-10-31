#pragma once

// A vector in segments that never move, declared with its defaults and what the registry reads of it. Defined in
// urd/repr/containers/segmented_vector.hpp, which urd/repr/segmented.hpp includes.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/registry/guarantees.hpp"
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

namespace detail
{

/// segmented vector (elements never move)
struct segmented_vector_decl
{
    static constexpr std::string_view id = "segmented_vector";
    static constexpr auto headers = header_list<1> { "urd/repr/segmented.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::segmented_vector(false);
        if (!held)
            return {};
        return offer_of(urd::kind::sequence, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "segmented vector (elements never move)";
    static constexpr std::string_view holds = "doubling segments; stable references";
};
template<typename T, std::size_t N, typename I>
struct declaration_of<repr::SegmentedVector<T, N, false, I>>
{
    using declaration = segmented_vector_decl;
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

namespace detail
{

/// segmented vector, thread-safe appends
struct concurrent_segmented_vector_decl
{
    static constexpr std::string_view id = "concurrent_segmented_vector";
    static constexpr auto headers = header_list<1> { "urd/repr/segmented.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::segmented_vector(true);
        if (!held)
            return {};
        return offer_of(urd::kind::sequence, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 1.6;
    };
    static constexpr std::string_view description = "segmented vector, thread-safe appends";
    static constexpr std::string_view holds = "doubling segments; stable references; appends from threads";
};
template<typename T, std::size_t N, typename I>
struct declaration_of<repr::SegmentedVector<T, N, true, I>>
{
    using declaration = concurrent_segmented_vector_decl;
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

}  // namespace urd
