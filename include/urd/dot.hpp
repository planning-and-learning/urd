#pragma once

// A graph in the DOT language (Graphviz): its vertices and edges, each labelled by a function the caller names.

#include "urd/graphs.hpp"
#include "urd/tags.hpp"
#include "urd/traits.hpp"

#include <fmt/format.h>
#include <string>
#include <string_view>

namespace urd
{

/// Labels, as to_dot takes them: a function of the graph and a vertex, or of the graph and an edge (urd::Edge), whose
/// result fmt formats.
/// No label.
inline constexpr auto unlabeled = [](const auto& /*graph*/, const auto& /*vertex_or_edge*/) { return std::string_view {}; };
/// A vertex's data (vertex_value).
inline constexpr auto vertex_data = [](const auto& graph, const auto& vertex) -> decltype(auto) { return graph.vertex_value(vertex); };
/// An edge's data (edge_value, by its number).
inline constexpr auto edge_data = [](const auto& graph, const auto& edge) -> decltype(auto) { return graph.edge_value(edge.id); };

/// `text` as a DOT string literal's contents: backslashes, quotes and newlines escaped, carriage returns dropped.
inline std::string dot_escape(std::string_view text)
{
    auto result = std::string {};
    result.reserve(text.size());
    for (const auto ch : text)
    {
        switch (ch)
        {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                break;
            default:
                result += ch;
                break;
        }
    }
    return result;
}

/// `graph` in DOT, named `name`: vertex v as `v<index>` labelled by vertex_label(graph, v), each edge (urd::Edge) by
/// edge_label(graph, edge), each as fmt formats it; an empty label is left out. An undirected graph (has<G, undirected>)
/// is a DOT `graph` whose edges `--` are written once, from their endpoint of the smaller index; any other a `digraph`.
template<adjacency_graph G, typename VL, typename EL>
[[nodiscard]] std::string to_dot(const G& graph, const VL& vertex_label, const EL& edge_label, std::string_view name = "G")
{
    using V = typename G::vertex_type;
    constexpr bool symmetric = has<G, undirected>;
    const auto at = [](V v) { return index_of<V>::to_index(v); };
    const auto attribute = [](const auto& label)
    {
        const auto text = fmt::format("{}", label);
        return text.empty() ? std::string {} : fmt::format(" [label=\"{}\"]", dot_escape(text));
    };
    auto text = fmt::format("{} {} {{\n  rankdir=LR;\n", symmetric ? "graph" : "digraph", name);
    for (auto v : graph.vertices())
        text += fmt::format("  v{}{};\n", at(v), attribute(vertex_label(graph, v)));
    for (auto u : graph.vertices())
        for (const auto& e : out_edges_of(graph, u))
            if (!symmetric || at(e.source) <= at(e.target))
                text += fmt::format("  v{} {} v{}{};\n", at(e.source), symmetric ? "--" : "->", at(e.target), attribute(edge_label(graph, e)));
    text += "}\n";
    return text;
}

}  // namespace urd
