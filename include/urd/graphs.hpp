#pragma once

// Operations on any graph, written once from its primitives, which a graph that does better overrides (the brokkr
// coding rule "Derived operations"): an overload for the default, and one constrained by the properties that promise
// better. A graph is anything with vertices and neighbours (graph_like): a Store of urd::graph<V, E>, or a graph of one's
// own, such as one that generates its successors. What a graph's neighbours are combined with are sets of vertices
// (Stores of urd::set<V>), never bits: where the graph keeps rows of bits (constant_adjacency) and the set is dense (a
// bitset, over a window of keys or not: urd::key_range), the overrides combine them a word at a time inside urd, so that
// an operation on part p costs the part's words, as hand-written rows of bits do. The defaults are in urd::detail, so
// that tests compare each override with them.

#include "urd/elements/requirements.hpp"
#include "urd/graph_types.hpp"
#include "urd/repr/graphs/bit_matrix.hpp"
#include "urd/repr/graphs/part_rows.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/store.hpp"
#include "urd/tags.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd
{

// The graph concepts, after the Boost Graph Library's: what a graph offers, each a primitive of the containers that
// have it. Every urd graph is a vertex list with adjacency (graph_like) and has its vertices' and edges' data
// (vertex_value, edge_value; std::monostate where there is none); one that keeps edge lists numbers its edges and is an
// incidence and an edge list graph, rows of bits are not.

/// Vertices 0..n-1 (vertex_type, an index): how many, and each in increasing order.
template<typename G>
concept vertex_list_graph = requires(const G& graph) {
    typename G::vertex_type;
    { graph.num_vertices() } -> std::convertible_to<std::size_t>;
    graph.vertices();
};
/// Each vertex's neighbours: the targets of the edges leaving it.
template<typename G>
concept adjacency_graph = vertex_list_graph<G> && requires(const G& graph, typename G::vertex_type u) {
    graph.neighbours(u);
    { graph.out_degree(u) } -> std::convertible_to<std::size_t>;
};
/// Whether an edge leads from one vertex to another.
template<typename G>
concept adjacency_matrix = vertex_list_graph<G> && requires(const G& graph, typename G::vertex_type u) {
    { graph.adjacent(u, u) } -> std::convertible_to<bool>;
};
/// A graph: a vertex list with neighbours and adjacency.
template<typename G>
concept graph_like = adjacency_graph<G> && adjacency_matrix<G>;
/// Each vertex's in-neighbours: the sources of the edges entering it.
template<typename G>
concept in_adjacency_graph = adjacency_graph<G> && requires(const G& graph, typename G::vertex_type v) {
    graph.in_neighbours(v);
    { graph.in_degree(v) } -> std::convertible_to<std::size_t>;
};
/// Edges by number (std::size_t): the numbers of the edges leaving each vertex, and each edge's endpoints.
template<typename G>
concept incidence_graph = adjacency_graph<G> && requires(const G& graph, typename G::vertex_type u, std::size_t e) {
    graph.out_edges(u);
    { graph.source(e) } -> std::convertible_to<typename G::vertex_type>;
    { graph.target(e) } -> std::convertible_to<typename G::vertex_type>;
};
/// An incidence graph with the numbers of the edges entering each vertex.
template<typename G>
concept bidirectional_graph = incidence_graph<G> && in_adjacency_graph<G> && requires(const G& graph, typename G::vertex_type v) { graph.in_edges(v); };
/// Every edge by number, and its endpoints.
template<typename G>
concept edge_list_graph = vertex_list_graph<G> && requires(const G& graph, std::size_t e) {
    graph.edges();
    { graph.num_edges() } -> std::convertible_to<std::size_t>;
    { graph.source(e) } -> std::convertible_to<typename G::vertex_type>;
    { graph.target(e) } -> std::convertible_to<typename G::vertex_type>;
};
/// A graph that grows: vertices and edges added.
template<typename G>
concept mutable_graph = vertex_list_graph<G> && requires(G& graph, typename G::vertex_type u) {
    graph.add_vertex();
    graph.add_edge(u, u);
};
/// A graph whose vertices fall into parts, numbered part after part.
template<typename G>
concept partitioned_graph = graph_like<G> && requires(const G& graph, typename G::vertex_type v, std::size_t p) {
    { graph.num_parts() } -> std::convertible_to<std::size_t>;
    { graph.part_of(v) } -> std::convertible_to<std::size_t>;
    graph.part(p);
};
/// A set of vertices of graph G.
template<typename Set, typename G>
concept vertex_set_of = requires(Set& set, const Set& read, typename G::vertex_type v) {
    set.insert(v);
    set.erase(v);
    set.clear();
    { read.contains(v) } -> std::convertible_to<bool>;
};

namespace detail
{

/// Whether G keeps rows of bits and Set the words of a bitset: then they combine a word at a time.
template<typename G, typename Set>
concept by_words = has<G, constant_adjacency> && is_store<Set> && has<typename Set::representation, dense, canonical>;

/// Calls f(dense set, words, first, bits) for the row of `v` in each part from `first_part` to before `last_part`: the
/// container of `set`, a span of the row's words, the index of the part's first vertex, and how many vertices it has.
template<typename G, typename Set, typename F>
void for_each_row(const G& graph, typename G::vertex_type v, std::size_t first_part, std::size_t last_part, Set& set, F f)
{
    const auto& rows = repr::graphs::rows_of(graph);
    auto& words = store_access::representation(set);
    for (auto p = first_part; p < last_part; ++p)
        f(words, graph_rows::row(rows, v, p).blocks(), graph_rows::first_of(rows, p), graph_rows::size_of(rows, p));
}
/// All the parts a graph's rows have (one where it is not partitioned).
template<typename G>
std::size_t row_parts(const G& graph)
{ return repr::graphs::rows_of(graph).num_parts(); }

// The defaults, from neighbours and adjacent.

template<partitioned_graph G, vertex_set_of<G> Set>
void copy_neighbours_by_iteration(const G& graph, typename G::vertex_type v, std::size_t p, Set& into)
{
    into.clear();
    for (const auto w : graph.neighbours(v))
        if (graph.part_of(w) == p)
            into.insert(w);
}
template<graph_like G, vertex_set_of<G> Set>
void copy_neighbours_by_iteration(const G& graph, typename G::vertex_type v, Set& into)
{
    into.clear();
    for (const auto w : graph.neighbours(v))
        into.insert(w);
}
/// Removes from `into` each vertex `keep(w)` says no to.
template<typename Set, typename Keep>
void keep_only(Set& into, Keep keep)
{
    auto dropped = std::vector<std::ranges::range_value_t<Set>>();
    for (const auto w : into)
        if (!keep(w))
            dropped.push_back(w);
    for (const auto w : dropped)
        into.erase(w);
}
template<partitioned_graph G, vertex_set_of<G> Set>
void intersect_neighbours_by_iteration(const G& graph, typename G::vertex_type v, std::size_t p, Set& into)
{
    keep_only(into, [&](auto w) { return graph.part_of(w) == p && graph.adjacent(v, w); });
}
template<graph_like G, vertex_set_of<G> Set>
void intersect_neighbours_by_iteration(const G& graph, typename G::vertex_type v, Set& into)
{
    keep_only(into, [&](auto w) { return graph.adjacent(v, w); });
}
template<partitioned_graph G, vertex_set_of<G> Set>
void intersect_neighbours_by_iteration(const G& graph, typename G::vertex_type v, std::size_t p, const Set& from, Set& into)
{
    if (&from == &into)
        return intersect_neighbours_by_iteration(graph, v, p, into);
    into.clear();
    for (const auto w : from)
        if (graph.part_of(w) == p && graph.adjacent(v, w))
            into.insert(w);
}
template<graph_like G, vertex_set_of<G> Set>
void intersect_neighbours_by_iteration(const G& graph, typename G::vertex_type v, const Set& from, Set& into)
{
    if (&from == &into)
        return intersect_neighbours_by_iteration(graph, v, into);
    into.clear();
    for (const auto w : from)
        if (graph.adjacent(v, w))
            into.insert(w);
}
template<partitioned_graph G, vertex_set_of<G> Set>
void subtract_neighbours_by_iteration(const G& graph, typename G::vertex_type v, std::size_t p, Set& into)
{
    for (const auto w : graph.neighbours(v))
        if (graph.part_of(w) == p)
            into.erase(w);
}
template<graph_like G, vertex_set_of<G> Set>
void subtract_neighbours_by_iteration(const G& graph, typename G::vertex_type v, Set& into)
{
    for (const auto w : graph.neighbours(v))
        into.erase(w);
}
template<partitioned_graph G>
std::size_t degree_in_by_iteration(const G& graph, typename G::vertex_type v, std::size_t p)
{
    std::size_t result = 0;
    for (const auto w : graph.neighbours(v))
        result += graph.part_of(w) == p ? 1U : 0U;
    return result;
}
template<partitioned_graph G, vertex_set_of<G> Set, typename F>
void for_each_neighbour_in_by_iteration(const G& graph, typename G::vertex_type v, std::size_t p, const Set& among, F f)
{
    for (const auto w : graph.neighbours(v))
        if (graph.part_of(w) == p && among.contains(w))
            f(w);
}
template<graph_like G, vertex_set_of<G> Set, typename F>
void for_each_neighbour_in_by_iteration(const G& graph, typename G::vertex_type v, const Set& among, F f)
{
    for (const auto w : graph.neighbours(v))
        if (among.contains(w))
            f(w);
}

/// The sizes of a graph's parts: none where it has none.
template<partitioned_graph G>
std::vector<std::size_t> part_sizes(const G& graph)
{
    auto result = std::vector<std::size_t>();
    for (std::size_t p = 0; p < graph.num_parts(); ++p)
        result.push_back(static_cast<std::size_t>(std::ranges::distance(graph.part(p))));
    return result;
}
template<graph_like G>
    requires(!partitioned_graph<G>)
std::vector<std::size_t> part_sizes(const G& /*graph*/)
{ return {}; }
/// Counts `v` in its part's size, where the graph has parts.
template<partitioned_graph G>
void count_in_part(const G& graph, typename G::vertex_type v, std::vector<std::size_t>& sizes)
{ ++sizes[graph.part_of(v)]; }
template<graph_like G>
    requires(!partitioned_graph<G>)
void count_in_part(const G& /*graph*/, typename G::vertex_type /*v*/, std::vector<std::size_t>& /*sizes*/)
{
}
/// One past a graph's last vertex (its vertices may have holes where some were removed).
template<graph_like G>
std::size_t vertex_bound(const G& graph)
{
    std::size_t result = 0;
    for (const auto v : graph.vertices())
        result = std::max(result, index_of<typename G::vertex_type>::to_index(v) + 1);
    return result;
}
/// Every edge of `graph` as To takes it (urd::graph_input_edge), its endpoints renumbered by `number` (an optional
/// index; none: left out), with its data, in the order of their numbers: so they keep their numbers where none is left
/// out or was removed.
template<typename To, edge_list_graph G, typename Number>
auto edges_of(const G& graph, Number number)
{
    using W = typename container_traits<To>::vertex_type;
    auto result = std::vector<typename container_traits<To>::edge_input_type>();
    for (const auto e : graph.edges())
        if (const auto from = number(graph.source(e)))
            if (const auto to = number(graph.target(e)))
                result.emplace_back(index_of<W>::from_index(*from), index_of<W>::from_index(*to), graph.edge_value(e));
    return result;
}
/// The data of the vertices of `graph` that To keeps (`number`, an optional index; none: left out), in the order of
/// their numbers, which increase with the vertices.
template<typename To, graph_like G, typename Number>
auto vertex_values_of(const G& graph, Number number)
{
    auto result = std::vector<typename container_traits<To>::vertex_value_type>();
    for (const auto v : graph.vertices())
        if (number(v))
            result.push_back(graph.vertex_value(v));
    return result;
}
/// Whether graph To holds rows of bits shaped as G's: then one is built from the other's rows.
template<typename To, typename G>
concept same_rows = has<To, constant_adjacency> && has<G, constant_adjacency> && (has<To, partitioned> == has<G, partitioned>)
                    && (has<To, undirected> == has<G, undirected>) && (!has<To, bidirectional> || has<G, bidirectional> || has<G, undirected>);

}  // namespace detail

/// Sets `into` to the neighbours of v in part p, or to all of v's neighbours.
template<partitioned_graph G, vertex_set_of<G> Set>
void copy_neighbours(const G& graph, typename G::vertex_type v, std::size_t p, Set& into)
{ detail::copy_neighbours_by_iteration(graph, v, p, into); }
template<partitioned_graph G, vertex_set_of<G> Set>
    requires detail::by_words<G, Set>
void copy_neighbours(const G& graph, typename G::vertex_type v, std::size_t p, Set& into)
{
    detail::for_each_row(graph,
                         v,
                         p,
                         p + 1,
                         into,
                         [](auto& set, auto words, std::size_t first, std::size_t bits) { detail::dense_words::assign(set, words, first, bits); });
}
template<graph_like G, vertex_set_of<G> Set>
void copy_neighbours(const G& graph, typename G::vertex_type v, Set& into)
{ detail::copy_neighbours_by_iteration(graph, v, into); }
template<graph_like G, vertex_set_of<G> Set>
    requires detail::by_words<G, Set>
void copy_neighbours(const G& graph, typename G::vertex_type v, Set& into)
{
    into.clear();
    detail::for_each_row(graph,
                         v,
                         0,
                         detail::row_parts(graph),
                         into,
                         [](auto& set, auto words, std::size_t first, std::size_t bits) { detail::dense_words::unite(set, words, first, bits); });
}

/// Keeps in `into` only the neighbours of v in part p, or only v's neighbours.
template<partitioned_graph G, vertex_set_of<G> Set>
void intersect_neighbours(const G& graph, typename G::vertex_type v, std::size_t p, Set& into)
{ detail::intersect_neighbours_by_iteration(graph, v, p, into); }
template<partitioned_graph G, vertex_set_of<G> Set>
    requires detail::by_words<G, Set>
void intersect_neighbours(const G& graph, typename G::vertex_type v, std::size_t p, Set& into)
{
    detail::for_each_row(graph,
                         v,
                         p,
                         p + 1,
                         into,
                         [](auto& set, auto words, std::size_t first, std::size_t bits) { detail::dense_words::intersect(set, words, first, bits); });
}
template<graph_like G, vertex_set_of<G> Set>
void intersect_neighbours(const G& graph, typename G::vertex_type v, Set& into)
{ detail::intersect_neighbours_by_iteration(graph, v, into); }
template<graph_like G, vertex_set_of<G> Set>
    requires detail::by_words<G, Set>
void intersect_neighbours(const G& graph, typename G::vertex_type v, Set& into)
{
    // The parts are disjoint windows of keys that cover every vertex: each keeps those of its keys its row has.
    detail::for_each_row(graph,
                         v,
                         0,
                         detail::row_parts(graph),
                         into,
                         [](auto& set, auto words, std::size_t first, std::size_t bits) { detail::dense_words::intersect_within(set, words, first, bits); });
}

/// Sets `into` to the vertices of `from` that are neighbours of v in part p, or neighbours of v at all: the step of a
/// clique search, candidates narrowed by the vertex chosen, without copying them first. `from` may be `into`.
template<partitioned_graph G, vertex_set_of<G> Set>
void intersect_neighbours(const G& graph, typename G::vertex_type v, std::size_t p, const Set& from, Set& into)
{ detail::intersect_neighbours_by_iteration(graph, v, p, from, into); }
template<partitioned_graph G, vertex_set_of<G> Set>
    requires detail::by_words<G, Set>
void intersect_neighbours(const G& graph, typename G::vertex_type v, std::size_t p, const Set& from, Set& into)
{
    const auto& rows = repr::graphs::rows_of(graph);
    detail::dense_words::assign_common(detail::store_access::representation(into),
                                       detail::store_access::representation(from),
                                       detail::graph_rows::row(rows, v, p).blocks(),
                                       detail::graph_rows::first_of(rows, p),
                                       detail::graph_rows::size_of(rows, p));
}
template<graph_like G, vertex_set_of<G> Set>
void intersect_neighbours(const G& graph, typename G::vertex_type v, const Set& from, Set& into)
{ detail::intersect_neighbours_by_iteration(graph, v, from, into); }
template<graph_like G, vertex_set_of<G> Set>
    requires detail::by_words<G, Set>
void intersect_neighbours(const G& graph, typename G::vertex_type v, const Set& from, Set& into)
{
    if (&from != &into)
        into = from;
    intersect_neighbours(graph, v, into);
}

/// Removes from `into` the neighbours of v in part p, or all of v's neighbours.
template<partitioned_graph G, vertex_set_of<G> Set>
void subtract_neighbours(const G& graph, typename G::vertex_type v, std::size_t p, Set& into)
{ detail::subtract_neighbours_by_iteration(graph, v, p, into); }
template<partitioned_graph G, vertex_set_of<G> Set>
    requires detail::by_words<G, Set>
void subtract_neighbours(const G& graph, typename G::vertex_type v, std::size_t p, Set& into)
{
    detail::for_each_row(graph,
                         v,
                         p,
                         p + 1,
                         into,
                         [](auto& set, auto words, std::size_t first, std::size_t bits) { detail::dense_words::subtract(set, words, first, bits); });
}
template<graph_like G, vertex_set_of<G> Set>
void subtract_neighbours(const G& graph, typename G::vertex_type v, Set& into)
{ detail::subtract_neighbours_by_iteration(graph, v, into); }
template<graph_like G, vertex_set_of<G> Set>
    requires detail::by_words<G, Set>
void subtract_neighbours(const G& graph, typename G::vertex_type v, Set& into)
{
    detail::for_each_row(graph,
                         v,
                         0,
                         detail::row_parts(graph),
                         into,
                         [](auto& set, auto words, std::size_t first, std::size_t bits) { detail::dense_words::subtract(set, words, first, bits); });
}

/// How many neighbours v has in part p.
template<partitioned_graph G>
std::size_t degree_in(const G& graph, typename G::vertex_type v, std::size_t p)
{ return detail::degree_in_by_iteration(graph, v, p); }
template<partitioned_graph G>
    requires has<G, constant_adjacency>
std::size_t degree_in(const G& graph, typename G::vertex_type v, std::size_t p)
{ return detail::graph_rows::row(repr::graphs::rows_of(graph), v, p).count(); }

/// Calls f on each neighbour of v in part p, or of all of v's neighbours, that `among` holds (the vertices still live,
/// say): in increasing order where they combine a word at a time.
template<partitioned_graph G, vertex_set_of<G> Set, typename F>
void for_each_neighbour_in(const G& graph, typename G::vertex_type v, std::size_t p, const Set& among, F f)
{ detail::for_each_neighbour_in_by_iteration(graph, v, p, among, std::move(f)); }
template<partitioned_graph G, vertex_set_of<G> Set, typename F>
    requires detail::by_words<G, Set>
void for_each_neighbour_in(const G& graph, typename G::vertex_type v, std::size_t p, const Set& among, F f)
{
    detail::for_each_row(graph,
                         v,
                         p,
                         p + 1,
                         among,
                         [&f](const auto& set, auto words, std::size_t first, std::size_t bits)
                         { detail::dense_words::for_each_common(set, words, first, bits, f); });
}
template<graph_like G, vertex_set_of<G> Set, typename F>
void for_each_neighbour_in(const G& graph, typename G::vertex_type v, const Set& among, F f)
{ detail::for_each_neighbour_in_by_iteration(graph, v, among, std::move(f)); }
template<graph_like G, vertex_set_of<G> Set, typename F>
    requires detail::by_words<G, Set>
void for_each_neighbour_in(const G& graph, typename G::vertex_type v, const Set& among, F f)
{
    detail::for_each_row(graph,
                         v,
                         0,
                         detail::row_parts(graph),
                         among,
                         [&f](const auto& set, auto words, std::size_t first, std::size_t bits)
                         { detail::dense_words::for_each_common(set, words, first, bits, f); });
}

/// Makes every vertex of part p adjacent to every vertex of part q (another part): an edge from each to each. Where
/// the graph keeps rows of bits, each one's row into the other part is filled, a word at a time.
template<partitioned_graph G>
void connect_parts(G& graph, std::size_t p, std::size_t q)
{
    for (const auto u : graph.part(p))
        for (const auto v : graph.part(q))
            graph.add_edge(u, v);
}
template<partitioned_graph G>
    requires(has<G, constant_adjacency> && requires(G& graph, typename G::vertex_type v) { graph.add_edge(v, v); })
void connect_parts(G& graph, std::size_t p, std::size_t q)
{ detail::graph_rows::connect_parts(repr::graphs::rows_of(graph), p, q); }

/// The graph To (a Store of urd::graph<V, E, D>, frozen say) with the vertices and edges of `graph`, their data and its
/// parts where both have them: from its edges (an edge list graph), in the order of their numbers, so that they keep
/// them where none was removed, or from its rows where both keep rows of bits of one shape, equal rows stored once.
template<typename To, edge_list_graph G>
To freeze(const G& graph)
{
    const auto same = [](auto v) { return std::optional(index_of<typename G::vertex_type>::to_index(v)); };
    const auto count = detail::vertex_bound(graph);
    const auto edges = detail::edges_of<To>(graph, same);
    const auto values = detail::vertex_values_of<To>(graph, same);
    const auto sizes = detail::part_sizes(graph);
    return To(typename container_traits<To>::graph_data_type { count, edges, sizes, values });
}
template<typename To, graph_like G>
    requires detail::same_rows<To, G>
To freeze(const G& graph)
{ return To(graph); }

/// The graph To on the vertices `keep` holds, with the edges between them and their data: the vertices numbered in
/// increasing order, so part after part, each part keeping its place (empty where none of it is kept), the edges in the
/// order of their numbers. From its edges (an edge list graph), or where both keep rows of bits of one shape and `keep`
/// is dense, each kept vertex's rows masked by `keep` a word at a time, or where `keep` holds every vertex, the rows
/// copied whole.
template<typename To, edge_list_graph G, vertex_set_of<G> Set>
To induced(const G& graph, const Set& keep)
{
    using V = typename G::vertex_type;
    auto number = std::vector<std::optional<std::size_t>>(detail::vertex_bound(graph));
    auto sizes = std::vector<std::size_t>(detail::part_sizes(graph).size(), 0);
    std::size_t next = 0;
    for (const auto v : graph.vertices())
        if (keep.contains(v))
        {
            number[index_of<V>::to_index(v)] = next++;
            detail::count_in_part(graph, v, sizes);
        }
    const auto renumber = [&number](V v) { return number[index_of<V>::to_index(v)]; };
    const auto edges = detail::edges_of<To>(graph, renumber);
    const auto values = detail::vertex_values_of<To>(graph, renumber);
    return To(typename container_traits<To>::graph_data_type { next, edges, sizes, values });
}
template<typename To, graph_like G, vertex_set_of<G> Set>
    requires(detail::same_rows<To, G> && detail::by_words<G, Set>)
To induced(const G& graph, const Set& keep)
{
    using V = typename G::vertex_type;
    const auto& rows = repr::graphs::rows_of(graph);
    if (keep.size() == rows.num_vertices())  // all of it: its rows as they are
        return freeze<To>(graph);
    auto number = std::vector<std::size_t>(rows.num_vertices(), 0);
    auto sizes = std::vector<std::size_t>(rows.num_parts(), 0);
    std::size_t next = 0;
    for (const auto v : keep)
    {
        number[index_of<V>::to_index(v)] = next++;
        ++sizes[rows.part_of(v)];
    }
    using Kept = repr::BitMatrix<V, has<G, partitioned>, has<G, undirected>, has<To, bidirectional> && !has<G, undirected>>;
    auto kept = Kept(graph_data<std::pair<V, V>> { next, {}, has<G, partitioned> ? sizes : std::vector<std::size_t>() });
    const auto renumbered = [&number](V v) { return index_of<V>::from_index(number[index_of<V>::to_index(v)]); };
    for (const auto u : keep)
        detail::for_each_row(graph,
                             u,
                             0,
                             rows.num_parts(),
                             keep,
                             [&](const auto& set, auto words, std::size_t first, std::size_t bits)
                             { detail::dense_words::for_each_common(set, words, first, bits, [&](V w) { kept.add_edge(renumbered(u), renumbered(w)); }); });
    return To(std::as_const(kept));
}

}  // namespace urd
