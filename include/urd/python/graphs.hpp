#pragma once

// nanobind bindings of urd's graphs (urd/graphs.hpp): a graph class answers its members in Python, as urd's graph
// concepts say it has them. Values pass through nanobind's type casters, which the binding includes: a vertex as its
// type converts, edges as their numbers, ranges as lists (urd/python/ranges.hpp), a graph's data by reference into the
// graph, which it keeps alive (a graph without data has data of a type with one value, std::monostate, seen as None:
// nanobind/stl/variant.h).

#include "urd/graphs.hpp"
#include "urd/python/ranges.hpp"

#include <cstddef>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/variant.h>

namespace urd::python
{
namespace nb = nanobind;

/// Binds graph G's members on its class: num_vertices, vertices(), neighbours(v), out_degree(v), adjacent(u, v) and
/// vertex_value(v); an incidence and edge list graph adds num_edges, edges(), out_edges(v), edge(u, v), source(e),
/// target(e) and edge_value(e); an in-adjacency graph in_neighbours(v) and in_degree(v); a bidirectional graph
/// in_edges(v). A vertex or edge the graph does not have raises IndexError (the graph's std::out_of_range).
template<graph_like G>
void bind_graph(nb::class_<G>& cls)
{
    using namespace nb::literals;
    using V = typename G::vertex_type;

    cls.def_prop_ro("num_vertices", [](const G& g) { return g.num_vertices(); })
        .def("vertices", [](const G& g) { return g.vertices(); })
        .def(
            "neighbours",
            [](const G& g, V u) { return g.neighbours(u); },
            "vertex"_a)
        .def(
            "out_degree",
            [](const G& g, V u) { return g.out_degree(u); },
            "vertex"_a)
        .def(
            "adjacent",
            [](const G& g, V u, V v) { return g.adjacent(u, v); },
            "source"_a,
            "target"_a)
        .def("vertex_value", [](const G& g, V v) -> decltype(auto) { return g.vertex_value(v); }, "vertex"_a, nb::rv_policy::reference_internal);
    if constexpr (incidence_graph<G> && edge_list_graph<G>)
        cls.def_prop_ro("num_edges", [](const G& g) { return g.num_edges(); })
            .def("edges", [](const G& g) { return g.edges(); })
            .def(
                "out_edges",
                [](const G& g, V u) { return g.out_edges(u); },
                "vertex"_a)
            .def(
                "edge",
                [](const G& g, V u, V v) { return g.edge(u, v); },
                "source"_a,
                "target"_a)
            .def(
                "source",
                [](const G& g, std::size_t e) { return g.source(e); },
                "edge"_a)
            .def(
                "target",
                [](const G& g, std::size_t e) { return g.target(e); },
                "edge"_a)
            .def("edge_value", [](const G& g, std::size_t e) -> decltype(auto) { return g.edge_value(e); }, "edge"_a, nb::rv_policy::reference_internal);
    if constexpr (in_adjacency_graph<G>)
        cls.def(
               "in_neighbours",
               [](const G& g, V v) { return g.in_neighbours(v); },
               "vertex"_a)
            .def("in_degree", [](const G& g, V v) { return g.in_degree(v); }, "vertex"_a);
    if constexpr (bidirectional_graph<G>)
        cls.def("in_edges", [](const G& g, V v) { return g.in_edges(v); }, "vertex"_a);
}

}  // namespace urd::python
