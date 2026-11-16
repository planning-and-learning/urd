// A graph in DOT (urd::to_dot): labels by data or by function, escaped; a directed graph's edges as `->`, an undirected
// graph's as `--`, each once.

#include "urd/dot.hpp"
#include "urd/repr/graphs/bit_matrix.hpp"
#include "urd/repr/graphs/csr_graph.hpp"
#include "urd/store.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

using V = std::uint32_t;

TEST(Dot, LabelsByData)
{
    using Labeled = urd::Store<urd::graph<V, int, char>, urd::frozen>;
    const auto edges = std::vector<std::tuple<V, V, int>> { { 0, 1, 7 } };
    const auto names = std::vector<char> { 'a', 'b' };
    const auto g = Labeled(urd::graph_data<std::tuple<V, V, int>, char> { 2, edges, {}, names });
    EXPECT_EQ(urd::to_dot(g, urd::vertex_data, urd::edge_data),
              "digraph G {\n  rankdir=LR;\n  v0 [label=\"a\"];\n  v1 [label=\"b\"];\n  v0 -> v1 [label=\"7\"];\n}\n");
}

TEST(Dot, LabelsAndEscapes)
{
    const auto edges = std::vector<std::pair<V, V>> { { 0, 1 } };
    const auto g = urd::Store<urd::graph<V>, urd::frozen>(urd::graph_data<std::pair<V, V>> { 2, edges, {} });
    const auto text = urd::to_dot(
        g,
        [](const auto&, V v) { return v == 0 ? std::string("say \"hi\"") : std::string(); },
        [](const auto&, const urd::Edge<V>& e) { return e.id; });
    EXPECT_EQ(text, "digraph G {\n  rankdir=LR;\n  v0 [label=\"say \\\"hi\\\"\"];\n  v1;\n  v0 -> v1 [label=\"0\"];\n}\n");
}

TEST(Dot, AnUndirectedGraphWritesEachEdgeOnce)
{
    auto g = urd::Store<urd::graph<V>, urd::monotone, urd::undirected, urd::constant_adjacency>(
        urd::graph_data<std::pair<V, V>> { 3, {}, {} });
    g.add_edge(2, 0);
    g.add_edge(1, 2);
    EXPECT_EQ(urd::to_dot(g, urd::unlabeled, urd::unlabeled, "pairs"),
              "graph pairs {\n  rankdir=LR;\n  v0;\n  v1;\n  v2;\n  v0 -- v2;\n  v1 -- v2;\n}\n");
}

}  // namespace
