#pragma once

// What a graph (urd::graph<V, E, D>, urd/kinds.hpp) is built from: an edge as given (its endpoints and its data), and
// the vertex count, vertex data and parts a frozen graph is built with. A graph hands out its edges as their numbers,
// which stay those they were given: an edge's endpoints and data are read by its number.
//
// A graph without edge or vertex data has data of a type with one value (std::monostate, regular void): every graph
// has data, and code written for graphs never asks whether there is some. A value of a type with one value may be
// left out, since nothing is lost: an edge given as a pair, a vertex added without data, no vertex values given.

#include "urd/elements/requirements.hpp"

#include <concepts>
#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

/// An edge as given to a graph: its source, its target and its data.
template<typename V, typename E>
using graph_input_edge = std::tuple<V, V, E>;

/// What a graph with vertices V and edge data E takes as an edge: graph_input_edge, or, where its data is a unit, a
/// pair of endpoints.
template<typename Edge, typename V, typename E>
concept input_edge_of = std::same_as<Edge, graph_input_edge<V, E>> || (unit<E> && std::same_as<Edge, std::pair<V, V>>);

/// The data of an edge as given: its third member, or the unit's one value.
template<typename E, typename Edge>
[[nodiscard]] E input_value(const Edge& edge)
{
    if constexpr (std::tuple_size_v<Edge> == 3)  // NOLINT(readability-magic-numbers): source, target, data
        return std::get<2>(edge);
    else
        return E {};
}

/// What a frozen graph is built from besides its edges: how many vertices it has (at least one past every endpoint),
/// each vertex's data (one per vertex, by vertex; none given where D is a unit), and for a partitioned graph the size
/// of each part, whose vertices are numbered part after part. An edge's number is its position in `edges`.
template<typename Edge, typename D = std::monostate>
struct graph_data
{
    std::size_t vertices = 0;
    std::span<const Edge> edges {};
    std::span<const std::size_t> parts {};
    std::span<const D> vertex_values {};
};

/// Whether From is graph_data a graph with vertices V, edge data E and vertex data D is built from: its vertex data D,
/// its edges any it takes (urd::input_edge_of).
template<typename From, typename V, typename E, typename D>
inline constexpr bool graph_data_for = false;
template<typename Edge, typename V, typename E, typename D>
inline constexpr bool graph_data_for<graph_data<Edge, D>, V, E, D> = input_edge_of<Edge, V, E>;

}  // namespace urd
