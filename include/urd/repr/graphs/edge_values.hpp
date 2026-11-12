#pragma once

// The data of a graph's edges, by edge number: a vector of them, or for a graph without edge data (E void) nothing.
// A policy, so that the graphs hold and build their edges the same way with or without data (rule C1).

#include "urd/memory.hpp"

#include <cstddef>
#include <tuple>
#include <vector>

namespace urd::repr::graphs
{

template<typename E>
class EdgeValues
{
public:
    /// Appends the data of an edge as given (urd::graph_input_edge: its third member).
    template<typename Input>
    void push_input(const Input& edge)
    { m_values.push_back(std::get<2>(edge)); }
    void push(const E& value) { m_values.push_back(value); }
    /// Appends the data of edge `e` of `other`.
    void push_from(const EdgeValues& other, std::size_t e) { m_values.push_back(other.m_values[e]); }
    [[nodiscard]] const E& operator[](std::size_t e) const { return m_values[e]; }
    void reserve(std::size_t count) { m_values.reserve(count); }
    void clear() { m_values.clear(); }

    friend bool operator==(const EdgeValues&, const EdgeValues&) = default;

    static constexpr auto memory_members() noexcept { return Members<&EdgeValues::m_values> {}; }

private:
    std::vector<E> m_values;
};

template<>
class EdgeValues<void>
{
public:
    template<typename Input>
    void push_input(const Input& /*edge*/)
    {
    }
    void push() {}
    void push_from(const EdgeValues& /*other*/, std::size_t /*e*/) {}
    void reserve(std::size_t /*count*/) {}
    void clear() {}

    friend bool operator==(const EdgeValues&, const EdgeValues&) = default;

    static constexpr auto memory_members() noexcept { return Members<> {}; }
};

}  // namespace urd::repr::graphs
