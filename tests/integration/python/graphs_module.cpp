// A module binding urd's graphs (urd/python/graphs.hpp), for tests/integration/python/graphs.py: one graph that grows, one frozen
// with in-edges and data, one of rows of bits.

#include "urd/graphs.hpp"
#include "urd/python/graphs.hpp"
#include "urd/repr/graphs/adjacency_lists.hpp"
#include "urd/repr/graphs/bit_rows.hpp"
#include "urd/repr/graphs/csr_graph.hpp"
#include "urd/store.hpp"

#include <cstdint>
#include <nanobind/nanobind.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
using V = std::uint32_t;
using Growing = urd::Store<urd::graph<V>>;
using Labeled = urd::Store<urd::graph<V, int, std::string>, urd::frozen, urd::bidirectional>;
using Rows = urd::Store<urd::graph<V>, urd::frozen, urd::constant_adjacency>;
}  // namespace

NB_MODULE(urd_graphs_test, m)
{
    namespace nb = nanobind;
    auto growing = nb::class_<Growing>(m, "Growing");
    growing.def(nb::init<>())
        .def("add_vertex", [](Growing& g) { return g.add_vertex(); })
        .def("add_edge", [](Growing& g, V u, V v) { return g.add_edge(u, v); });
    urd::python::bind_graph(growing);
    m.def("growing_from", [](const std::vector<std::pair<V, V>>& edges) { return Growing(edges.begin(), edges.end()); });

    auto labeled = nb::class_<Labeled>(m, "Labeled");
    labeled.def("__init__",
                [](Labeled* self, const std::vector<std::tuple<V, V, int>>& edges, const std::vector<std::string>& names)
                { new (self) Labeled(urd::graph_data<std::tuple<V, V, int>, std::string> { names.size(), edges, {}, names }); });
    urd::python::bind_graph(labeled);

    auto rows = nb::class_<Rows>(m, "Rows");
    rows.def("__init__",
             [](Rows* self, std::size_t vertices, const std::vector<std::pair<V, V>>& edges)
             { new (self) Rows(urd::graph_data<std::pair<V, V>> { vertices, edges, {} }); });
    urd::python::bind_graph(rows);
}
