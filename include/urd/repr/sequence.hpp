#pragma once

#include "urd/properties.hpp"
#include "urd/repr/ops.hpp"
#include "urd/traits.hpp"

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace urd
{

template<typename T, typename Allocator>
struct container_traits<std::vector<T, Allocator>> : properties_of<dense, contiguous, shrinkable>
{
};

}  // namespace urd

namespace urd::repr
{

template<typename T, typename Allocator>
struct ops<std::vector<T, Allocator>>
{
    using R = std::vector<T, Allocator>;

    [[nodiscard]] static R make(std::vector<T> elements) { return R(std::move(elements)); }
    [[nodiscard]] static R assume(std::vector<T> elements) { return make(std::move(elements)); }
    static bool insert(R& range, const T& element)
    {
        range.push_back(element);
        return true;
    }
    static std::size_t erase(R& range, const T& element) { return std::erase(range, element); }
    static void clear(R& range) noexcept { range.clear(); }
    [[nodiscard]] static const T& at(const R& range, std::size_t pos) { return range[pos]; }
};

/// A sequence in insertion order: std::vector.
template<typename S>
struct sequence_vector
{
    using type = std::vector<typename S::value_type>;
    static constexpr std::string_view name = "std::vector";
    static constexpr bool accepts = S::is_sequence;
    static constexpr int rank = 0;
};

}  // namespace urd::repr
