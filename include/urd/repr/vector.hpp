#pragma once

#include "urd/algorithms.hpp"
#include "urd/properties.hpp"
#include "urd/refined.hpp"
#include "urd/repr/ops.hpp"

#include <concepts>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace urd::repr
{

template<typename T, props P>
struct ops<Refined<std::vector<T>, P>>
{
    using R = Refined<std::vector<T>, P>;

    [[nodiscard]] static R make(std::vector<T> elements) { return R::make(std::move(elements)); }
    [[nodiscard]] static R assume(std::vector<T> elements) { return R::assume(std::move(elements)); }
    static bool insert(R& range, const T& element) { return range.insert(element); }
    static std::size_t erase(R& range, const T& element) { return range.erase(element); }
    [[nodiscard]] static bool contains(const R& range, const T& element) { return urd::contains(range, element); }
};

/// A sequence: a vector, kept sorted or unique when asked.
template<typename S>
struct sequence_vector
{
    static constexpr props content = S::flags & (props::sorted | props::unique);
    using type = Refined<std::vector<typename S::value_type>, content>;
    static constexpr std::string_view name = "vector";
    static constexpr bool accepts = S::is_sequence;
    static constexpr props guarantees = content | props::contiguous | props::dense
        | (includes(content, props::sorted | props::unique) ? props::canonical : props::none);
    static constexpr int rank = 0;
};

/// A set as a sorted vector without duplicates: compact and fast to search, O(n) to insert.
template<typename S>
struct flat_set
{
    using type = Refined<std::vector<typename S::key_type>, props::sorted | props::unique>;
    static constexpr std::string_view name = "sorted unique vector";
    static constexpr bool accepts = S::is_set && std::totally_ordered<typename S::key_type>;
    static constexpr props guarantees =
        props::sorted | props::unique | props::contiguous | props::canonical | props::sparse;
    static constexpr int rank =
        includes(S::flags, props::frozen) || S::work == workload::small ? 0 : (S::work == workload::many_similar ? 3 : 2);
};

}  // namespace urd::repr
