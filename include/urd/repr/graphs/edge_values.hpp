#pragma once

// The data of a graph's edges, by edge number, or of its vertices, by vertex: a vector of them, or for data of a type
// with one value (urd::unit: no data) how many there are. A policy, so that the graphs hold and build their edges and
// vertices the same way whatever their data (rule C1).

#include "urd/graph_types.hpp"
#include "urd/memory.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

namespace urd::repr::graphs
{

template<typename T>
class Values
{
public:
    void push(const T& value) { m_values.push_back(value); }
    /// The values of `count` vertices or edges, one each.
    void assign(std::size_t count, std::span<const T> values)
    {
        if (values.size() != count)
            throw std::invalid_argument("urd::graph: data for every vertex");
        m_values.assign(values.begin(), values.end());
    }
    [[nodiscard]] const T& operator[](std::size_t at) const { return m_values[at]; }
    void reserve(std::size_t count) { m_values.reserve(count); }
    void clear() { m_values.clear(); }

    friend bool operator==(const Values&, const Values&) = default;

    static constexpr auto memory_members() noexcept { return Members<&Values::m_values> {}; }

private:
    std::vector<T> m_values;
};

/// Data of a type with one value: nothing stored, the one value handed out; given values may be left out.
template<unit T>
class Values<T>
{
public:
    void push(const T& /*value*/) {}
    /// Values of a unit carry nothing: however many are given.
    void assign(std::size_t /*count*/, std::span<const T> /*values*/) {}
    [[nodiscard]] const T& operator[](std::size_t /*at*/) const { return value; }
    void reserve(std::size_t /*count*/) {}
    void clear() {}

    friend bool operator==(const Values&, const Values&) = default;

    static constexpr auto memory_members() noexcept { return Members<> {}; }

private:
    static constexpr T value {};
};

}  // namespace urd::repr::graphs
