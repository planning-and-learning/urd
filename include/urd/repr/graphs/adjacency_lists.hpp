#pragma once

#include "urd/elements/requirements.hpp"
#include "urd/graph_types.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/graphs/edge_values.hpp"
#include "urd/repr/graphs/graphs_fwd.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace urd::repr
{

/// A graph that grows and shrinks (port of runir's DynamicGraph): edges numbered as added, each vertex the numbers of
/// the edges that leave it and of those that enter it. A removed edge or vertex leaves a hole; numbers are never
/// reused.
template<typename V, typename E>
class AdjacencyLists
{
public:
    using vertex_type = V;
    using size_type = std::size_t;

    AdjacencyLists() = default;

    /// From edges (urd::graph_input_edge), with one vertex past every endpoint.
    template<std::input_iterator InputIt>
        requires std::convertible_to<std::iter_reference_t<InputIt>, const graph_input_edge<V, E>&>
    AdjacencyLists(InputIt first, InputIt last)
    {
        for (; first != last; ++first)
            add_input(*first);
    }
    explicit AdjacencyLists(const graph_data<graph_input_edge<V, E>>& data)
    {
        while (m_present.size() < data.vertices)
            add_vertex();
        for (const auto& each : data.edges)
            add_input(each);
    }

    [[nodiscard]] size_type num_vertices() const noexcept { return m_num_vertices; }
    [[nodiscard]] auto vertices() const
    {
        return std::views::iota(std::size_t { 0 }, m_present.size()) | std::views::filter([this](std::size_t at) { return m_present[at]; })
               | std::views::transform([](std::size_t at) { return index_of<V>::from_index(at); });
    }
    [[nodiscard]] auto neighbours(V u) const
    {
        return m_out[index(u)] | std::views::transform([this](std::size_t e) { return m_edges[e].target; });
    }
    [[nodiscard]] size_type out_degree(V u) const { return m_out[index(u)].size(); }
    [[nodiscard]] bool adjacent(V u, V v) const { return edge(u, v).has_value(); }
    [[nodiscard]] auto in_neighbours(V v) const
    {
        return m_in[index(v)] | std::views::transform([this](std::size_t e) { return m_edges[e].source; });
    }
    [[nodiscard]] size_type in_degree(V v) const { return m_in[index(v)].size(); }

    V add_vertex()
    {
        m_present.push_back(true);
        m_out.emplace_back();
        m_in.emplace_back();
        ++m_num_vertices;
        return index_of<V>::from_index(m_present.size() - 1);
    }
    template<typename... Value>
    std::size_t add_edge(V u, V v, const Value&... value)
    {
        check_vertex(u);
        check_vertex(v);
        const auto e = m_edges.size();
        m_edges.push_back(Edge { u, v, true });
        m_values.push(value...);
        m_out[index(u)].push_back(e);
        m_in[index(v)].push_back(e);
        ++m_num_edges;
        return e;
    }
    void remove_edge(std::size_t e)
    {
        if (e >= m_edges.size() || !m_edges[e].alive)
            throw std::out_of_range("urd::graph: remove_edge of an edge that is not there");
        m_edges[e].alive = false;
        std::erase(m_out[index(m_edges[e].source)], e);
        std::erase(m_in[index(m_edges[e].target)], e);
        --m_num_edges;
    }
    void remove_vertex(V v)
    {
        check_vertex(v);
        while (!m_out[index(v)].empty())
            remove_edge(m_out[index(v)].back());
        while (!m_in[index(v)].empty())
            remove_edge(m_in[index(v)].back());
        m_present[index(v)] = false;
        --m_num_vertices;
    }
    void clear()
    {
        m_edges.clear();
        m_values.clear();
        m_out.clear();
        m_in.clear();
        m_present.clear();
        m_num_vertices = 0;
        m_num_edges = 0;
    }

    [[nodiscard]] size_type num_edges() const noexcept { return m_num_edges; }
    [[nodiscard]] auto edges() const
    {
        return std::views::iota(std::size_t { 0 }, m_edges.size()) | std::views::filter([this](std::size_t e) { return m_edges[e].alive; })
               | std::views::transform([this](std::size_t e) { return edge_at(e); });
    }
    [[nodiscard]] auto out_edges(V u) const
    {
        return m_out[index(u)] | std::views::transform([this](std::size_t e) { return edge_at(e); });
    }
    [[nodiscard]] auto in_edges(V v) const
    {
        return m_in[index(v)] | std::views::transform([this](std::size_t e) { return edge_at(e); });
    }
    [[nodiscard]] std::optional<std::size_t> edge(V u, V v) const
    {
        const auto& row = m_out[index(u)];
        const auto found = std::ranges::find_if(row, [this, v](std::size_t e) { return m_edges[e].target == v; });
        return found == row.end() ? std::nullopt : std::optional<std::size_t>(*found);
    }
    [[nodiscard]] auto edges_between(V u, V v) const
    {
        return m_out[index(u)] | std::views::filter([this, v](std::size_t e) { return m_edges[e].target == v; });
    }
    [[nodiscard]] V source(std::size_t e) const { return m_edges[e].source; }
    [[nodiscard]] V target(std::size_t e) const { return m_edges[e].target; }
    [[nodiscard]] const auto& value(std::size_t e) const
        requires(!std::is_void_v<E>)
    { return m_values[e]; }

    friend bool operator==(const AdjacencyLists&, const AdjacencyLists&) = default;

private:
    struct Edge
    {
        V source;
        V target;
        bool alive;

        friend bool operator==(const Edge&, const Edge&) = default;
    };

    [[nodiscard]] static std::size_t index(V v) { return index_of<V>::to_index(v); }
    void check_vertex(V v) const
    {
        if (index(v) >= m_present.size() || !m_present[index(v)])
            throw std::out_of_range("urd::graph: a vertex that is not there");
    }
    /// Adds an edge as given, with the vertices up to its endpoints.
    void add_input(const graph_input_edge<V, E>& input)
    {
        const auto last = std::max(index(std::get<0>(input)), index(std::get<1>(input)));
        while (m_present.size() <= last)
            add_vertex();
        const auto e = m_edges.size();
        m_edges.push_back(Edge { std::get<0>(input), std::get<1>(input), true });
        m_values.push_input(input);
        m_out[index(std::get<0>(input))].push_back(e);
        m_in[index(std::get<1>(input))].push_back(e);
        ++m_num_edges;
    }
    [[nodiscard]] graph_edge<V, E> edge_at(std::size_t e) const
        requires std::is_void_v<E>
    { return { m_edges[e].source, m_edges[e].target, e }; }
    [[nodiscard]] graph_edge<V, E> edge_at(std::size_t e) const
        requires(!std::is_void_v<E>)
    { return { m_edges[e].source, m_edges[e].target, e, m_values[e] }; }

    std::vector<Edge> m_edges;
    graphs::EdgeValues<E> m_values;
    std::vector<std::vector<std::size_t>> m_out;
    std::vector<std::vector<std::size_t>> m_in;
    std::vector<bool> m_present;
    std::size_t m_num_vertices = 0;
    std::size_t m_num_edges = 0;

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept
    { return Members<&AdjacencyLists::m_edges, &AdjacencyLists::m_values, &AdjacencyLists::m_out, &AdjacencyLists::m_in, &AdjacencyLists::m_present> {}; }
};

}  // namespace urd::repr

template<>
struct urd::include<"urd/repr/graphs/adjacency_lists.hpp">
{
};

namespace urd::detail
{

template<typename S>
struct representation_of<adjacency_lists_decl, S>
{
    using type = repr::AdjacencyLists<typename S::vertex_type, typename S::edge_value_type>;
};

}  // namespace urd::detail
