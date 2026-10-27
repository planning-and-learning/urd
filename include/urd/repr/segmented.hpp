#pragma once

#include "urd/properties.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/ops.hpp"
#include "urd/traits.hpp"

#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd
{

/// Segments never move, so references stay valid; it appends but cannot remove from the middle. The
/// thread-safe variant appends concurrently.
template<typename T, std::size_t FirstSegmentSize, bool ThreadSafe, std::unsigned_integral Index>
struct container_traits<repr::SegmentedVector<T, FirstSegmentSize, ThreadSafe, Index>>
    : properties_of<dense, stable_refs, growable, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

}  // namespace urd

namespace urd::repr
{

template<typename T, std::size_t FirstSegmentSize, bool ThreadSafe, std::unsigned_integral Index>
struct ops<SegmentedVector<T, FirstSegmentSize, ThreadSafe, Index>>
{
    using R = SegmentedVector<T, FirstSegmentSize, ThreadSafe, Index>;

    [[nodiscard]] static R make(std::vector<T> elements)
    {
        auto made = R();
        for (auto& element : elements)
            made.push_back(std::move(element));
        return made;
    }
    [[nodiscard]] static R assume(std::vector<T> elements) { return make(std::move(elements)); }
    static bool insert(R& range, const T& element)
    {
        range.push_back(element);
        return true;
    }
    [[nodiscard]] static const T& at(const R& range, std::size_t pos) { return range[pos]; }
};

/// A sequence in segments that are never moved, thread-safe when concurrent is asked for.
template<typename S>
struct segmented_sequence
{
    using type = SegmentedVector<typename S::value_type, default_first_segment_size, has<S, concurrent>>;
    static constexpr std::string_view name = "segmented vector";
    static constexpr bool accepts = S::is_sequence;
    static constexpr int rank = 1;  // a plain vector is faster when references need not stay valid
};

}  // namespace urd::repr
