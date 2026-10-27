#pragma once

#include "urd/properties.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/ops.hpp"

#include <string_view>
#include <utility>
#include <vector>

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

/// A sequence in segments that are never moved: appending keeps references valid, and the thread-safe
/// variant appends concurrently. It cannot remove from the middle, nor keep an order or uniqueness.
template<typename S>
struct segmented_sequence
{
    using type = SegmentedVector<typename S::value_type, default_first_segment_size, includes(S::flags, props::concurrent)>;
    static constexpr std::string_view name = "segmented vector";
    static constexpr bool erases = false;
    static constexpr bool accepts = S::is_sequence && !includes(S::flags, props::sorted) && !includes(S::flags, props::unique);
    static constexpr props guarantees =
        props::stable_refs | props::dense | (includes(S::flags, props::concurrent) ? props::concurrent : props::none);
    static constexpr int rank = 1;  // a plain vector is faster when references need not stay valid
};

}  // namespace urd::repr
