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
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace urd::repr
{

/// A frozen graph as compressed sparse rows (port of runir's StaticGraph and BidirectionalStaticGraph): each edge's
/// source, target and data by its number, which is its position in the edges it was built from, and each vertex's row
/// of the numbers and targets of the edges leaving it, sorted by target (parallel edges in the order given), so that
/// edge(u, v) is a binary search in u's row. Each vertex's data by vertex. InEdges: rows of the edges entering each
/// vertex.
template<typename V, typename E, typename D, bool InEdges>
class CsrGraph
{
public:
    using vertex_type = V;
    using size_type = std::size_t;

    CsrGraph() = default;

    /// From edges (urd::input_edge_of), with one vertex past every endpoint, where its vertex data is a unit.
    template<std::input_iterator InputIt>
        requires input_edge_of<std::iter_value_t<InputIt>, V, E> && unit<D>
    CsrGraph(InputIt first, InputIt last)
    {
        const auto given = std::vector<std::iter_value_t<InputIt>>(first, last);
        auto vertices = std::size_t { 0 };
        for (const auto& each : given)
            vertices = std::max({ vertices, index(std::get<0>(each)) + 1, index(std::get<1>(each)) + 1 });
        m_vertex_values.assign(vertices, {});
        build(vertices, std::span<const std::iter_value_t<InputIt>>(given));
    }
    template<input_edge_of<V, E> Edge>
    explicit CsrGraph(const graph_data<Edge, D>& data)
    {
        m_vertex_values.assign(data.vertices, data.vertex_values);
        build(data.vertices, data.edges);
    }

    [[nodiscard]] size_type num_vertices() const noexcept { return m_offsets.size() - 1; }
    [[nodiscard]] auto vertices() const
    {
        return std::views::iota(std::size_t { 0 }, num_vertices()) | std::views::transform([](std::size_t at) { return index_of<V>::from_index(at); });
    }
    [[nodiscard]] std::span<const V> neighbours(V u) const { return std::span<const V>(m_row_targets).subspan(m_offsets[index(u)], out_degree(u)); }
    [[nodiscard]] size_type out_degree(V u) const { return m_offsets[index(u) + 1] - m_offsets[index(u)]; }
    [[nodiscard]] bool adjacent(V u, V v) const { return edge(u, v).has_value(); }
    [[nodiscard]] std::span<const V> in_neighbours(V v) const
        requires InEdges
    { return std::span<const V>(m_in_sources).subspan(m_in_offsets[index(v)], in_degree(v)); }
    [[nodiscard]] size_type in_degree(V v) const
        requires InEdges
    { return m_in_offsets[index(v) + 1] - m_in_offsets[index(v)]; }

    [[nodiscard]] size_type num_edges() const noexcept { return m_targets.size(); }
    /// The edges' numbers: 0..num_edges-1.
    [[nodiscard]] std::ranges::iota_view<std::size_t, std::size_t> edges() const { return { 0, num_edges() }; }
    /// The numbers of the edges leaving `u`, by target.
    [[nodiscard]] std::span<const std::size_t> out_edges(V u) const { return std::span<const std::size_t>(m_row_ids).subspan(m_offsets[index(u)], out_degree(u)); }
    /// The numbers of the edges entering `v`, by source.
    [[nodiscard]] std::span<const std::size_t> in_edges(V v) const
        requires InEdges
    { return std::span<const std::size_t>(m_in_ids).subspan(m_in_offsets[index(v)], in_degree(v)); }
    [[nodiscard]] std::optional<std::size_t> edge(V u, V v) const
    {
        const auto between = edges_between(u, v);
        return between.empty() ? std::nullopt : std::optional<std::size_t>(between.front());
    }
    /// The numbers of the edges from u to v, in the order given.
    [[nodiscard]] std::span<const std::size_t> edges_between(V u, V v) const
    {
        const auto row = neighbours(u);
        const auto [first, last] = std::ranges::equal_range(row, index(v), {}, &CsrGraph::index);
        return out_edges(u).subspan(static_cast<std::size_t>(first - row.begin()), static_cast<std::size_t>(last - first));
    }
    [[nodiscard]] V source(std::size_t e) const { return m_sources[e]; }
    [[nodiscard]] V target(std::size_t e) const { return m_targets[e]; }
    [[nodiscard]] const E& edge_value(std::size_t e) const { return m_values[e]; }
    [[nodiscard]] const D& vertex_value(V v) const { return m_vertex_values[index(v)]; }

    friend bool operator==(const CsrGraph&, const CsrGraph&) = default;

private:
    [[nodiscard]] static std::size_t index(V v) { return index_of<V>::to_index(v); }

    /// The edges from `given`, numbered as given, and the rows: each vertex's edges by target, parallel ones in the
    /// order given.
    template<typename Edge>
    void build(std::size_t vertices, std::span<const Edge> given)
    {
        m_sources.reserve(given.size());
        m_targets.reserve(given.size());
        m_values.reserve(given.size());
        for (const auto& each : given)
        {
            if (index(std::get<0>(each)) >= vertices || index(std::get<1>(each)) >= vertices)
                throw std::out_of_range("urd::graph: an edge's endpoint is not below the vertex count");
            m_sources.push_back(std::get<0>(each));
            m_targets.push_back(std::get<1>(each));
            m_values.push(input_value<E>(each));
        }
        m_row_ids.resize(given.size());
        std::iota(m_row_ids.begin(), m_row_ids.end(), std::size_t { 0 });
        std::ranges::stable_sort(m_row_ids, {}, [this](std::size_t e) { return std::pair(index(m_sources[e]), index(m_targets[e])); });
        m_offsets.assign(vertices + 1, 0);
        m_row_targets.reserve(given.size());
        for (const auto e : m_row_ids)
        {
            ++m_offsets[index(m_sources[e]) + 1];
            m_row_targets.push_back(m_targets[e]);
        }
        std::partial_sum(m_offsets.begin(), m_offsets.end(), m_offsets.begin());
        build_in_rows();
    }
    /// The rows of the edges entering each vertex, by source.
    void build_in_rows()
        requires InEdges
    {
        m_in_offsets.assign(num_vertices() + 1, 0);
        for (const auto target : m_targets)
            ++m_in_offsets[index(target) + 1];
        std::partial_sum(m_in_offsets.begin(), m_in_offsets.end(), m_in_offsets.begin());
        m_in_sources.resize(num_edges());
        m_in_ids.resize(num_edges());
        auto next = std::vector<std::size_t>(m_in_offsets.begin(), m_in_offsets.end() - 1);
        for (const auto e : m_row_ids)  // by source: each in-row ordered by source
        {
            const auto at = next[index(m_targets[e])]++;
            m_in_sources[at] = m_sources[e];
            m_in_ids[at] = e;
        }
    }
    void build_in_rows()
        requires(!InEdges)
    {
    }

    std::vector<V> m_sources;  ///< by edge number
    std::vector<V> m_targets;  ///< by edge number
    graphs::Values<E> m_values;  ///< by edge number
    graphs::Values<D> m_vertex_values;  ///< by vertex
    std::vector<std::size_t> m_offsets = std::vector<std::size_t>(1, 0);
    std::vector<std::size_t> m_row_ids;  ///< each row's edge numbers, by target
    std::vector<V> m_row_targets;  ///< each row's targets
    std::vector<std::size_t> m_in_offsets;  ///< InEdges only
    std::vector<V> m_in_sources;  ///< InEdges only
    std::vector<std::size_t> m_in_ids;  ///< InEdges only

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept
    {
        return Members<&CsrGraph::m_sources,
                       &CsrGraph::m_targets,
                       &CsrGraph::m_values,
                       &CsrGraph::m_vertex_values,
                       &CsrGraph::m_offsets,
                       &CsrGraph::m_row_ids,
                       &CsrGraph::m_row_targets,
                       &CsrGraph::m_in_offsets,
                       &CsrGraph::m_in_sources,
                       &CsrGraph::m_in_ids> {};
    }
};

}  // namespace urd::repr

template<>
struct urd::include<"urd/repr/graphs/csr_graph.hpp">
{
};

namespace urd::detail
{

template<bool InEdges, typename S>
struct representation_of<csr_graph_decl<InEdges>, S>
{
    using type = repr::CsrGraph<typename S::vertex_type, typename S::edge_value_type, typename S::vertex_value_type, InEdges>;
};

}  // namespace urd::detail
