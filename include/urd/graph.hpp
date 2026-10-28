#pragma once

#include "urd/store.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <ranges>
#include <span>
#include <utility>

namespace urd
{

/// A frozen directed graph over vertices 0..n-1 (indices, urd::index_of): its out-edges and its in-edges, each a
/// frozen multimap by dense key, so each vertex's successors and predecessors are spans. Not a Store kind of its
/// own, but two Stores over the same vertices.
// ponytail: no edge properties; key a Store by the edge's position in successors(), or add a value type when a
// site needs one.
template<typename Vertex>
class Graph
{
    using Adjacency = Store<key<Vertex>, value<Vertex>, multi, dense, frozen>;

public:
    Graph() = default;

    /// The graph on `vertices` vertices with `edges`, pairs (source, target) in any order; a vertex's successors
    /// and predecessors come in the order of its edges there.
    template<std::ranges::forward_range Edges>
    Graph(std::size_t vertices, const Edges& edges) :
        m_vertices(vertices),
        m_successors(std::ranges::begin(edges), std::ranges::end(edges)),
        m_predecessors(reversed(edges))
    {
        assert(std::ranges::all_of(edges,
                                   [vertices](const auto& edge)
                                   { return index_of<Vertex>::to_index(edge.first) < vertices && index_of<Vertex>::to_index(edge.second) < vertices; }));
    }

    [[nodiscard]] std::size_t num_vertices() const { return m_vertices; }
    [[nodiscard]] std::size_t num_edges() const { return m_successors.size(); }
    [[nodiscard]] std::span<const Vertex> successors(const Vertex& vertex) const { return m_successors.values(vertex); }
    [[nodiscard]] std::span<const Vertex> predecessors(const Vertex& vertex) const { return m_predecessors.values(vertex); }

private:
    template<typename Edges>
    static Adjacency reversed(const Edges& edges)
    {
        const auto swapped = std::views::transform(edges, [](const auto& edge) { return std::pair<Vertex, Vertex>(edge.second, edge.first); });
        return Adjacency(std::ranges::begin(swapped), std::ranges::end(swapped));
    }

    std::size_t m_vertices = 0;
    Adjacency m_successors;
    Adjacency m_predecessors;
};

}  // namespace urd
