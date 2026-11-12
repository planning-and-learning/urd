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

/// A frozen graph as compressed sparse rows (port of runir's StaticGraph and BidirectionalStaticGraph): each vertex's
/// edges in a row, sorted by target (parallel edges in the order given), so an edge's number is its position, and
/// edge(u, v) a binary search in u's row. InEdges: rows of the edges entering each vertex, by number. Sources: a source
/// per edge; without it, source(e) is a binary search over the rows' offsets.
template<typename V, typename E, bool InEdges, bool Sources>
class CsrGraph
{
public:
    using vertex_type = V;
    using size_type = std::size_t;

    CsrGraph() = default;

    /// From edges (urd::graph_input_edge), with one vertex past every endpoint.
    template<std::input_iterator InputIt>
        requires std::convertible_to<std::iter_reference_t<InputIt>, const graph_input_edge<V, E>&>
    CsrGraph(InputIt first, InputIt last)
    {
        const auto given = std::vector<graph_input_edge<V, E>>(first, last);
        auto vertices = std::size_t { 0 };
        for (const auto& each : given)
            vertices = std::max({ vertices, index(std::get<0>(each)) + 1, index(std::get<1>(each)) + 1 });
        build(vertices, given);
    }
    explicit CsrGraph(const graph_data<graph_input_edge<V, E>>& data) { build(data.vertices, data.edges); }

    [[nodiscard]] size_type num_vertices() const noexcept { return m_offsets.size() - 1; }
    [[nodiscard]] auto vertices() const
    {
        return std::views::iota(std::size_t { 0 }, num_vertices()) | std::views::transform([](std::size_t at) { return index_of<V>::from_index(at); });
    }
    [[nodiscard]] std::span<const V> neighbours(V u) const { return std::span<const V>(m_targets).subspan(m_offsets[index(u)], out_degree(u)); }
    [[nodiscard]] size_type out_degree(V u) const { return m_offsets[index(u) + 1] - m_offsets[index(u)]; }
    [[nodiscard]] bool adjacent(V u, V v) const { return edge(u, v).has_value(); }
    [[nodiscard]] std::span<const V> in_neighbours(V v) const
        requires InEdges
    { return std::span<const V>(m_in_sources).subspan(m_in_offsets[index(v)], in_degree(v)); }
    [[nodiscard]] size_type in_degree(V v) const
        requires InEdges
    { return m_in_offsets[index(v) + 1] - m_in_offsets[index(v)]; }

    [[nodiscard]] size_type num_edges() const noexcept { return m_targets.size(); }
    /// Every edge, by number: row after row.
    [[nodiscard]] auto edges() const
    {
        return std::views::iota(std::size_t { 0 }, num_vertices())
               | std::views::transform([this](std::size_t at) { return out_edges(index_of<V>::from_index(at)); }) | std::views::join;
    }
    [[nodiscard]] auto out_edges(V u) const
    {
        return std::views::iota(m_offsets[index(u)], m_offsets[index(u) + 1]) | std::views::transform([this, u](std::size_t e) { return edge_at(u, e); });
    }
    [[nodiscard]] auto in_edges(V v) const
        requires InEdges
    {
        return std::views::iota(m_in_offsets[index(v)], m_in_offsets[index(v) + 1])
               | std::views::transform([this](std::size_t at) { return edge_at(m_in_sources[at], m_in_ids[at]); });
    }
    [[nodiscard]] std::optional<std::size_t> edge(V u, V v) const
    {
        const auto between = edges_between(u, v);
        return between.empty() ? std::nullopt : std::optional<std::size_t>(between.front());
    }
    /// The numbers of the edges from u to v: consecutive, in the order given.
    [[nodiscard]] std::ranges::iota_view<std::size_t, std::size_t> edges_between(V u, V v) const
    {
        const auto row = neighbours(u);
        const auto [first, last] = std::ranges::equal_range(row, index(v), {}, &CsrGraph::index);
        const auto base = m_offsets[index(u)];
        return { base + static_cast<std::size_t>(first - row.begin()), base + static_cast<std::size_t>(last - row.begin()) };
    }
    [[nodiscard]] V source(std::size_t e) const
        requires Sources
    { return m_sources[e]; }
    /// The row that holds `e`: the last vertex whose row begins at or before it.
    [[nodiscard]] V source(std::size_t e) const
        requires(!Sources)
    { return index_of<V>::from_index(static_cast<std::size_t>(std::ranges::upper_bound(m_offsets, e) - m_offsets.begin()) - 1); }
    [[nodiscard]] V target(std::size_t e) const { return m_targets[e]; }
    [[nodiscard]] const auto& value(std::size_t e) const
        requires(!std::is_void_v<E>)
    { return m_values[e]; }

    friend bool operator==(const CsrGraph&, const CsrGraph&) = default;

private:
    [[nodiscard]] static std::size_t index(V v) { return index_of<V>::to_index(v); }

    /// The rows from `given`: its edges ordered by (source, target), parallel ones in the order given.
    void build(std::size_t vertices, std::span<const graph_input_edge<V, E>> given)
    {
        for (const auto& each : given)
            if (index(std::get<0>(each)) >= vertices || index(std::get<1>(each)) >= vertices)
                throw std::out_of_range("urd::graph: an edge's endpoint is not below the vertex count");
        auto order = std::vector<std::size_t>(given.size());
        std::iota(order.begin(), order.end(), std::size_t { 0 });
        std::ranges::stable_sort(order,
                                 [&given](std::size_t lhs, std::size_t rhs)
                                 {
                                     return std::pair(index(std::get<0>(given[lhs])), index(std::get<1>(given[lhs])))
                                            < std::pair(index(std::get<0>(given[rhs])), index(std::get<1>(given[rhs])));
                                 });
        m_offsets.assign(vertices + 1, 0);
        m_targets.reserve(given.size());
        m_values.reserve(given.size());
        for (const auto at : order)
        {
            ++m_offsets[index(std::get<0>(given[at])) + 1];
            m_targets.push_back(std::get<1>(given[at]));
            m_values.push_input(given[at]);
        }
        std::partial_sum(m_offsets.begin(), m_offsets.end(), m_offsets.begin());
        build_in_rows();
        build_sources();
    }
    /// The rows of the edges entering each vertex, by number.
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
        for (std::size_t u = 0; u < num_vertices(); ++u)
            for (auto e = m_offsets[u]; e < m_offsets[u + 1]; ++e)
            {
                const auto at = next[index(m_targets[e])]++;
                m_in_sources[at] = index_of<V>::from_index(u);
                m_in_ids[at] = e;
            }
    }
    void build_in_rows()
        requires(!InEdges)
    {
    }
    void build_sources()
        requires Sources
    {
        m_sources.reserve(num_edges());
        for (std::size_t u = 0; u < num_vertices(); ++u)
            m_sources.insert(m_sources.end(), out_degree(index_of<V>::from_index(u)), index_of<V>::from_index(u));
    }
    void build_sources()
        requires(!Sources)
    {
    }

    [[nodiscard]] graph_edge<V, E> edge_at(V u, std::size_t e) const
        requires std::is_void_v<E>
    { return { u, m_targets[e], e }; }
    [[nodiscard]] graph_edge<V, E> edge_at(V u, std::size_t e) const
        requires(!std::is_void_v<E>)
    { return { u, m_targets[e], e, m_values[e] }; }

    std::vector<std::size_t> m_offsets = std::vector<std::size_t>(1, 0);
    std::vector<V> m_targets;
    graphs::EdgeValues<E> m_values;
    std::vector<std::size_t> m_in_offsets;  ///< InEdges only
    std::vector<V> m_in_sources;            ///< InEdges only
    std::vector<std::size_t> m_in_ids;      ///< InEdges only
    std::vector<V> m_sources;               ///< Sources only

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept
    {
        return Members<&CsrGraph::m_offsets,
                       &CsrGraph::m_targets,
                       &CsrGraph::m_values,
                       &CsrGraph::m_in_offsets,
                       &CsrGraph::m_in_sources,
                       &CsrGraph::m_in_ids,
                       &CsrGraph::m_sources> {};
    }
};

}  // namespace urd::repr

template<>
struct urd::include<"urd/repr/graphs/csr_graph.hpp">
{
};

namespace urd::detail
{

template<bool InEdges, bool Sources, typename S>
struct representation_of<csr_graph_decl<InEdges, Sources>, S>
{
    using type = repr::CsrGraph<typename S::vertex_type, typename S::edge_value_type, InEdges, Sources>;
};

}  // namespace urd::detail
