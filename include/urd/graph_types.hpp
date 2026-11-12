#pragma once

// What a graph (urd::graph<V, E>, urd/kinds.hpp) is built from and hands out: an edge as given (its endpoints and its
// data), the vertex count and parts a frozen graph is built with, and an edge as handed out with its number.

#include <cstddef>
#include <functional>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace urd
{

/// An edge as given to a graph: its source and target, then its data where the graph has some (E not void).
template<typename V, typename E>
using graph_input_edge = std::conditional_t<std::is_void_v<E>, std::pair<V, V>, std::tuple<V, V, E>>;

/// What a frozen graph is built from besides its edges: how many vertices it has (at least one past every endpoint),
/// and for a partitioned graph the size of each part, whose vertices are numbered part after part. Edge is the graph's
/// graph_input_edge.
template<typename Edge>
struct graph_data
{
    std::size_t vertices = 0;
    std::span<const Edge> edges;
    std::span<const std::size_t> parts;
};

/// An edge as a graph with numbered edges hands it out: its endpoints, its number, and its data where the graph has
/// some (a reference: value.get()).
template<typename V, typename E>
struct graph_edge
{
    V source;
    V target;
    std::size_t id;
    std::reference_wrapper<const E> value;
};
template<typename V>
struct graph_edge<V, void>
{
    V source;
    V target;
    std::size_t id;

    friend constexpr bool operator==(const graph_edge&, const graph_edge&) = default;
};

}  // namespace urd
