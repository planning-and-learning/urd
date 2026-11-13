#pragma once

// A graph's interface (urd::graph<V, E, D>, urd/kinds.hpp): its vertices, their data and neighbours (graph), adding
// and removing (graph_building, graph_removal), its edges by number with their data (graph_edges), and its parts
// (graph_parts). Each member forwards to one member of the container (rules
// F1, F6).

#include "urd/config.hpp"
#include "urd/cost.hpp"
#include "urd/graph_types.hpp"
#include "urd/store/types.hpp"

#include <cstddef>
#include <optional>
#include <type_traits>

namespace urd::detail::facet
{

/// Vertices 0..n-1, each with its neighbours: what every graph has. bidirectional or undirected: a vertex's
/// in-neighbours too.
template<typename Derived, typename R, typename S>
class graph
{
    using vertex = typename S::vertex_type;

public:
    using vertex_type = vertex;
    using size_type = std::size_t;

    [[nodiscard]] size_type num_vertices() const { return representation().num_vertices(); }
    /// The vertices, in increasing order.
    [[nodiscard]] auto vertices() const { return representation().vertices(); }
    /// The targets of the edges leaving `u`, a target once per edge.
    [[nodiscard]] auto neighbours(vertex u) const { return representation().neighbours(u); }
    [[nodiscard]] size_type out_degree(vertex u) const { return representation().out_degree(u); }
    /// Whether an edge leads from `u` to `v`.
    [[nodiscard]] bool adjacent(vertex u, vertex v) const { return representation().adjacent(u, v); }
    /// The sources of the edges entering `v`, a source once per edge.
    [[nodiscard]] auto in_neighbours(vertex v) const
        requires(has<S, bidirectional> || has<S, undirected>)
    { return representation().in_neighbours(v); }
    [[nodiscard]] size_type in_degree(vertex v) const
        requires(has<S, bidirectional> || has<S, undirected>)
    { return representation().in_degree(v); }
    /// The data of `v` (D: std::monostate where it carries none).
    [[nodiscard]] decltype(auto) vertex_value(vertex v) const { return representation().vertex_value(v); }

private:
    [[nodiscard]] const R& representation() const { return store_access::of<Derived>(*this); }
};

/// A graph that grows: vertices and edges added, an edge's number returned; clear() where it may be emptied.
template<typename Derived, typename R, typename S>
class graph_building
{
    using vertex = typename S::vertex_type;

public:
    /// A new vertex, without edges, with its data (left out where it is a unit); the vertices of a partitioned graph, or
    /// of rows of bits, are fixed when it is built.
    template<typename... Value>
    vertex add_vertex(const Value&... value)
        requires(!has<S, partitioned> && !has<S, constant_adjacency>)
    { return representation().add_vertex(value...); }
    /// A new edge from `u` to `v`, with its data (left out where it is a unit): its number, or for rows of bits whether
    /// it is new.
    template<typename... Value>
    decltype(auto) add_edge(vertex u, vertex v, const Value&... value)
    { return representation().add_edge(u, v, value...); }
    void clear()
        requires clears<S>
    { representation().clear(); }

private:
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
};

/// A graph that shrinks: edges and vertices removed, the others keeping their numbers.
template<typename Derived, typename R, typename S>
class graph_removal
{
    using vertex = typename S::vertex_type;

public:
    /// Removes the edge numbered `e`.
    void remove_edge(std::size_t e) { representation().remove_edge(e); }
    /// Removes `v` and its edges.
    void remove_vertex(vertex v) { representation().remove_vertex(v); }

private:
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
};

/// Edges by number: an edge's number stays the one it was given (as added, or its position in the edges a frozen graph
/// is built from), and its endpoints and data are read by it. Where the graph keeps edge lists (not rows of bits): an
/// incidence and an edge list graph (urd/graphs.hpp).
template<typename Derived, typename R, typename S>
class graph_edges
{
    using vertex = typename S::vertex_type;

public:
    [[nodiscard]] std::size_t num_edges() const { return representation().num_edges(); }
    /// The numbers of every edge, in increasing order.
    [[nodiscard]] auto edges() const { return representation().edges(); }
    /// The numbers of the edges leaving `u`.
    [[nodiscard]] auto out_edges(vertex u) const { return representation().out_edges(u); }
    /// The numbers of the edges entering `v`.
    [[nodiscard]] auto in_edges(vertex v) const
        requires has<S, bidirectional>
    { return representation().in_edges(v); }
    /// The number of an edge from `u` to `v`, if there is one (with multi, the first given).
    [[nodiscard]] std::optional<std::size_t> edge(vertex u, vertex v) const { return representation().edge(u, v); }
    /// The numbers of every edge from `u` to `v`.
    [[nodiscard]] auto edges_between(vertex u, vertex v) const
        requires has<S, multi>
    { return representation().edges_between(u, v); }
    [[nodiscard]] vertex source(std::size_t e) const { return representation().source(e); }
    [[nodiscard]] vertex target(std::size_t e) const { return representation().target(e); }
    /// The data of the edge numbered `e` (E: std::monostate where it carries none); operator[] reads it too.
    [[nodiscard]] decltype(auto) edge_value(std::size_t e) const { return representation().edge_value(e); }
    [[nodiscard]] decltype(auto) operator[](std::size_t e) const { return representation().edge_value(e); }

private:
    [[nodiscard]] const R& representation() const { return store_access::of<Derived>(*this); }
};

/// A partitioned graph's parts: its vertices numbered part after part, no edge inside a part. What a part's vertices
/// are combined with (urd/graphs.hpp) are sets of vertices, whatever holds the graph.
template<typename Derived, typename R, typename S>
class graph_parts
{
    using vertex = typename S::vertex_type;

public:
    [[nodiscard]] std::size_t num_parts() const { return representation().num_parts(); }
    [[nodiscard]] std::size_t part_of(vertex v) const { return representation().part_of(v); }
    /// The vertices of part `p`, in increasing order.
    [[nodiscard]] auto part(std::size_t p) const { return representation().part(p); }

private:
    [[nodiscard]] const R& representation() const { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

template<>
struct facet_operations<facet::graph>
{
    using offered = operation_list<operation::iterate, operation::adjacency>;
};

template<>
struct facet_condition<facet::graph_building> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return grows<S>; }
};
template<>
struct facet_operations<facet::graph_building>
{
    using offered = operation_list<operation::insert, operation::clear>;
};

template<>
struct facet_condition<facet::graph_removal> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return shrinks<S>; }
};
template<>
struct facet_operations<facet::graph_removal>
{
    using offered = operation_list<operation::erase>;
};

/// Edges are numbered where the graph keeps edge lists: everywhere but in rows of bits (constant_adjacency).
template<>
struct facet_condition<facet::graph_edges> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return !has<S, constant_adjacency>; }
};
template<>
struct facet_operations<facet::graph_edges>
{
    using offered = operation_list<operation::access, operation::endpoints>;
};

template<>
struct facet_condition<facet::graph_parts> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, partitioned>; }
};

}  // namespace urd::detail
