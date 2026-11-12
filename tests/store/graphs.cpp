// Graphs: what a request resolves to, the interface its properties give, and each container against a model (a list of
// edges with their numbers), including the derived row operations against their defaults.

#include "urd/graphs.hpp"

#include "urd/repr/graphs/adjacency_lists.hpp"
#include "urd/repr/graphs/bit_matrix.hpp"
#include "urd/repr/graphs/bit_rows.hpp"
#include "urd/repr/graphs/csr_graph.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <random>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

using urd::bidirectional;
using urd::constant_adjacency;
using urd::constant_endpoints;
using urd::frozen;
using urd::graph;
using urd::monotone;
using urd::multi;
using urd::partitioned;
using urd::positional;
using urd::Store;
using urd::undirected;
using V = std::uint32_t;

// What a request resolves to.
static_assert(std::same_as<Store<graph<V>>::representation, urd::repr::AdjacencyLists<V, void>>);
static_assert(std::same_as<Store<graph<V, int>, monotone>::representation, urd::repr::AdjacencyLists<V, int>>);
static_assert(std::same_as<Store<graph<V>, frozen>::representation, urd::repr::CsrGraph<V, void, false, false>>);
static_assert(std::same_as<Store<graph<V>, frozen, bidirectional>::representation, urd::repr::CsrGraph<V, void, true, false>>);
static_assert(std::same_as<Store<graph<V>, frozen, constant_endpoints>::representation, urd::repr::CsrGraph<V, void, false, true>>);
// Rows of bits: frozen, stored once; growing, a matrix; partitioned or not, directed, with in-rows, or undirected.
static_assert(std::same_as<Store<graph<V>, frozen, partitioned, undirected, constant_adjacency>::representation, urd::repr::BitRows<V, true, true, false>>);
static_assert(std::same_as<Store<graph<V>, monotone, partitioned, undirected, constant_adjacency>::representation, urd::repr::BitMatrix<V, true, true, false>>);
static_assert(std::same_as<Store<graph<V>, frozen, constant_adjacency>::representation, urd::repr::BitRows<V, false, false, false>>);
static_assert(std::same_as<Store<graph<V>, frozen, constant_adjacency, bidirectional>::representation, urd::repr::BitRows<V, false, false, true>>);
static_assert(std::same_as<Store<graph<V>, monotone, constant_adjacency, undirected>::representation, urd::repr::BitMatrix<V, false, true, false>>);

// The interface follows the request: a frozen graph adds nothing, a monotone one removes nothing, edges have numbers
// only where asked for (positional) or where they carry data, in-neighbours only where bidirectional.
template<typename G>
concept adds = requires(G g) { g.add_edge(V { 0 }, V { 1 }); };
template<typename G>
concept removes = requires(G g) { g.remove_edge(std::size_t { 0 }); };
template<typename G>
concept numbers_edges = requires(const G g) { g.source(std::size_t { 0 }); };
template<typename G>
concept reads_in = requires(const G g) { g.in_neighbours(V { 0 }); };
template<typename G>
concept reads_bits = requires(const G g) { g.neighbours_in(V { 0 }, std::size_t { 0 }); } || requires(const G g) { g.layout(); };
static_assert(adds<Store<graph<V>>> && removes<Store<graph<V>>>);
static_assert(adds<Store<graph<V>, monotone>> && !removes<Store<graph<V>, monotone>>);
static_assert(!adds<Store<graph<V>, frozen>> && !removes<Store<graph<V>, frozen>>);
static_assert(!numbers_edges<Store<graph<V>, frozen>> && numbers_edges<Store<graph<V>, frozen, positional>> && numbers_edges<Store<graph<V, int>, frozen>>);
static_assert(!reads_in<Store<graph<V>>> && reads_in<Store<graph<V>, bidirectional>> && reads_in<Store<graph<V>, frozen, bidirectional>>);
static_assert(!reads_bits<Store<graph<V>, frozen, partitioned, undirected, constant_adjacency>> && !reads_bits<Store<graph<V>, frozen, constant_adjacency>>);
static_assert(reads_in<Store<graph<V>, frozen, constant_adjacency, undirected>>);
// The operations go a word at a time for rows of bits with dense sets, and by the defaults otherwise.
static_assert(urd::detail::by_words<Store<graph<V>, frozen, constant_adjacency>, Store<urd::set<V>, urd::dense>>);
static_assert(!urd::detail::by_words<Store<graph<V>, frozen, constant_adjacency>, Store<urd::set<V>>>
              && !urd::detail::by_words<Store<graph<V>, frozen>, Store<urd::set<V>, urd::dense>>);
static_assert(reads_in<Store<graph<V>, frozen, constant_adjacency, bidirectional>>);
template<typename G>
concept adds_vertices = requires(G g) { g.add_vertex(); };
static_assert(adds_vertices<Store<graph<V>, monotone>> && !adds_vertices<Store<graph<V>, monotone, partitioned, undirected, constant_adjacency>>
              && !adds_vertices<Store<graph<V>, monotone, constant_adjacency>>);
static_assert(urd::graph_like<Store<graph<V>>> && urd::partitioned_graph<Store<graph<V>, frozen, partitioned, undirected, constant_adjacency>>);

/// An edge of the model: its endpoints, its data, whether it is still there.
struct model_edge
{
    V source;
    V target;
    int value;
    bool alive = true;
};

template<typename Range>
std::vector<V> sorted(Range&& range)
{
    auto result = std::vector<V>();
    for (const auto each : std::forward<Range>(range))
        result.push_back(each);
    std::ranges::sort(result);
    return result;
}

/// A growing and shrinking graph agrees with a model under random changes: neighbours both ways, adjacency, numbers
/// as added, endpoints and data.
TEST(Graph, ListsAgreeWithModel)
{
    auto random = std::mt19937(5);
    auto g = Store<graph<V, int>, bidirectional>();
    auto present = std::vector<bool>();
    auto edges = std::vector<model_edge>();
    for (int step = 0; step < 2000; ++step)
    {
        const auto choice = random() % 10;
        if (choice < 2 || present.size() < 2)
        {
            EXPECT_EQ(g.add_vertex(), present.size());
            present.push_back(true);
        }
        else if (choice < 7)
        {
            const auto u = static_cast<V>(random() % present.size());
            const auto v = static_cast<V>(random() % present.size());
            if (!present[u] || !present[v])
                continue;
            const auto value = static_cast<int>(random() % 100);
            EXPECT_EQ(g.add_edge(u, v, value), edges.size());
            edges.push_back({ u, v, value });
        }
        else if (choice < 9)
        {
            const auto e = random() % (edges.size() + 1);
            if (e == edges.size() || !edges[e].alive)
                continue;
            g.remove_edge(e);
            edges[e].alive = false;
        }
        else
        {
            const auto v = static_cast<V>(random() % present.size());
            if (!present[v])
                continue;
            g.remove_vertex(v);
            present[v] = false;
            for (auto& each : edges)
                each.alive = each.alive && each.source != v && each.target != v;
        }
    }
    ASSERT_EQ(g.num_vertices(), static_cast<std::size_t>(std::ranges::count(present, true)));
    ASSERT_EQ(g.num_edges(), static_cast<std::size_t>(std::ranges::count_if(edges, &model_edge::alive)));
    for (V u = 0; u < present.size(); ++u)
    {
        if (!present[u])
            continue;
        auto out = std::vector<V>();
        auto in = std::vector<V>();
        for (const auto& each : edges)
        {
            if (each.alive && each.source == u)
                out.push_back(each.target);
            if (each.alive && each.target == u)
                in.push_back(each.source);
        }
        std::ranges::sort(out);
        std::ranges::sort(in);
        EXPECT_EQ(sorted(g.neighbours(u)), out);
        EXPECT_EQ(sorted(g.in_neighbours(u)), in);
        EXPECT_EQ(g.out_degree(u), out.size());
        for (V v = 0; v < present.size(); ++v)
            EXPECT_EQ(g.adjacent(u, v), present[v] && std::ranges::binary_search(out, v));
    }
    for (const auto& edge : g.edges())
    {
        const auto& expected = edges[edge.id];
        EXPECT_TRUE(expected.alive);
        EXPECT_EQ(edge.source, expected.source);
        EXPECT_EQ(edge.target, expected.target);
        EXPECT_EQ(edge.value.get(), expected.value);
        EXPECT_EQ(g[edge.id], expected.value);
        EXPECT_EQ(g.source(edge.id), expected.source);
    }
}

/// A frozen graph numbers its edges by (source, target), parallel edges in the order given, with or without a source
/// per edge; its in-rows and edges between two vertices agree.
template<typename G>
void check_frozen(std::size_t seed)
{
    auto random = std::mt19937(seed);
    constexpr std::size_t vertices = 30;
    auto given = std::vector<std::tuple<V, V, int>>();
    for (int each = 0; each < 200; ++each)
        given.emplace_back(static_cast<V>(random() % vertices), static_cast<V>(random() % vertices), each);
    const auto g = G(urd::graph_data<std::tuple<V, V, int>> { vertices, given, {} });

    auto expected = given;
    std::ranges::stable_sort(expected, {}, [](const auto& edge) { return std::pair(std::get<0>(edge), std::get<1>(edge)); });
    ASSERT_EQ(g.num_vertices(), vertices);
    ASSERT_EQ(g.num_edges(), expected.size());
    std::size_t number = 0;
    for (const auto& edge : g.edges())
    {
        ASSERT_EQ(edge.id, number);
        EXPECT_EQ(std::tuple(edge.source, edge.target, edge.value.get()), expected[number]);
        EXPECT_EQ(g.source(number), std::get<0>(expected[number]));
        EXPECT_EQ(g.target(number), std::get<1>(expected[number]));
        ++number;
    }
    for (V u = 0; u < vertices; ++u)
        for (V v = 0; v < vertices; ++v)
        {
            auto between = std::vector<std::size_t>();
            for (std::size_t e = 0; e < expected.size(); ++e)
                if (std::get<0>(expected[e]) == u && std::get<1>(expected[e]) == v)
                    between.push_back(e);
            auto found = std::vector<std::size_t>();
            for (const auto e : g.edges_between(u, v))
                found.push_back(e);
            EXPECT_EQ(found, between);
            EXPECT_EQ(g.edge(u, v), between.empty() ? std::nullopt : std::optional<std::size_t>(between.front()));
            EXPECT_EQ(g.adjacent(u, v), !between.empty());
        }
    for (V v = 0; v < vertices; ++v)
    {
        auto in = std::vector<V>();
        for (const auto& edge : expected)
            if (std::get<1>(edge) == v)
                in.push_back(std::get<0>(edge));
        EXPECT_EQ(sorted(g.in_neighbours(v)), sorted(in));
        for (const auto& edge : g.in_edges(v))
            EXPECT_EQ(edge.target, v);
    }
}
TEST(Graph, FrozenNumbersBySourceAndTarget)
{
    check_frozen<Store<graph<V, int>, frozen, multi, bidirectional>>(7);
    check_frozen<Store<graph<V, int>, frozen, multi, bidirectional, constant_endpoints>>(8);
}

/// A frozen graph built from edges alone has a vertex past every endpoint; a vertex count below that throws.
TEST(Graph, FrozenFromEdges)
{
    const auto given = std::vector<std::pair<V, V>> { { 0, 3 }, { 3, 1 } };
    const auto g = Store<graph<V>, frozen>(given.begin(), given.end());
    EXPECT_EQ(g.num_vertices(), 4U);
    EXPECT_EQ(sorted(g.neighbours(3)), std::vector<V> { 1 });
    EXPECT_THROW((Store<graph<V>, frozen>(urd::graph_data<std::pair<V, V>> { 3, given, {} })), std::out_of_range);
}

/// Whether two graphs on the same vertices have the same edges.
template<typename A, typename B>
bool same_edges(const A& a, const B& b)
{
    if (a.num_vertices() != b.num_vertices())
        return false;
    for (V u = 0; u < a.num_vertices(); ++u)
        for (V v = 0; v < a.num_vertices(); ++v)
            if (a.adjacent(u, v) != b.adjacent(u, v))
                return false;
    return true;
}

using Dense = Store<urd::set<V>, urd::dense>;  // combines with rows of bits a word at a time
using Hashed = Store<urd::set<V>>;             // the defaults

/// A dense graph of one shape, against a model: a matrix built up edge by edge (whether each edge is new), frozen into
/// rows from its rows and from its edges (Plain: compressed sparse rows, built from edges, read by the defaults),
/// each operation on vertex sets with rows and dense sets (over a part's window, over all vertices) against the default
/// with hashed sets, and the subgraph on random vertices both ways and against the model renumbered.
template<typename Matrix, typename Rows, typename Plain, bool Partitioned, bool Undirected, bool InRows>
void check_dense(unsigned seed)
{
    auto random = std::mt19937(seed);
    for (int round = 0; round < 12; ++round)
    {
        auto parts = std::vector<std::size_t>();
        for (std::size_t p = 0; p < (Partitioned ? 2 + (random() % 3) : 1); ++p)
            parts.push_back(random() % 90);
        auto part_of = std::vector<std::size_t>();
        auto first_of = std::vector<std::size_t>(1, 0);
        for (std::size_t p = 0; p < parts.size(); ++p)
        {
            part_of.insert(part_of.end(), parts[p], p);
            first_of.push_back(first_of.back() + parts[p]);
        }
        const auto vertices = part_of.size();
        auto adjacent = std::vector<std::vector<bool>>(vertices, std::vector<bool>(vertices, false));
        auto m = Matrix(urd::graph_data<std::pair<V, V>> { vertices, {}, Partitioned ? parts : std::vector<std::size_t>() });
        for (std::size_t each = 0; vertices > 0 && each < vertices * 4; ++each)
        {
            const auto u = static_cast<V>(random() % vertices);
            const auto v = static_cast<V>(random() % vertices);
            if (Partitioned && part_of[u] == part_of[v])
            {
                EXPECT_THROW(m.add_edge(u, v), std::invalid_argument);
                continue;
            }
            EXPECT_EQ(m.add_edge(u, v), !adjacent[u][v]);
            adjacent[u][v] = true;
            if (Undirected)
                adjacent[v][u] = true;
        }
        auto out = std::vector<std::vector<V>>(vertices);
        auto in = std::vector<std::vector<V>>(vertices);
        for (V u = 0; u < vertices; ++u)
            for (V v = 0; v < vertices; ++v)
            {
                ASSERT_EQ(m.adjacent(u, v), adjacent[u][v]);
                if (adjacent[u][v])
                {
                    out[u].push_back(v);
                    in[v].push_back(u);
                }
            }
        for (V u = 0; u < vertices; ++u)
        {
            EXPECT_EQ(sorted(m.neighbours(u)), out[u]);
            EXPECT_EQ(m.out_degree(u), out[u].size());
            if constexpr (Undirected || InRows)
                EXPECT_EQ(sorted(m.in_neighbours(u)), in[u]);
        }

        const auto fast = urd::freeze<Rows>(m);
        const auto slow = urd::freeze<Plain>(m);
        EXPECT_TRUE(same_edges(fast, m));
        EXPECT_TRUE(same_edges(slow, m));
        if constexpr (Undirected || InRows)
            for (V u = 0; u < vertices; ++u)
                EXPECT_EQ(sorted(fast.in_neighbours(u)), in[u]);

        // Each operation, with rows and dense sets against the default with hashed sets.
        const auto random_set = [&](auto& set, std::size_t first, std::size_t last)
        {
            for (auto key = first; key < last; ++key)
                if (random() % 2 == 0)
                    set.insert(static_cast<V>(key));
        };
        for (V v = 0; v < vertices; ++v)
        {
            if constexpr (Partitioned)
                for (std::size_t p = 0; p < parts.size(); ++p)
                {
                    const auto window = urd::key_range<V> { static_cast<V>(first_of[p]), static_cast<V>(first_of[p + 1]) };
                    auto dense = Dense(window);
                    auto hashed = Hashed();
                    urd::copy_neighbours(fast, v, p, dense);
                    urd::copy_neighbours(fast, v, p, hashed);
                    EXPECT_EQ(sorted(dense), sorted(hashed));
                    EXPECT_EQ(urd::degree_in(fast, v, p), urd::detail::degree_in_by_iteration(fast, v, p));
                    auto among = Dense(window);
                    random_set(among, first_of[p], first_of[p + 1]);
                    auto among_hashed = Hashed(among.begin(), among.end());
                    auto visited = std::vector<V>();
                    auto visited_hashed = std::vector<V>();
                    urd::for_each_neighbour_in(fast, v, p, among, [&](V w) { visited.push_back(w); });
                    urd::for_each_neighbour_in(fast, v, p, among_hashed, [&](V w) { visited_hashed.push_back(w); });
                    EXPECT_EQ(visited, sorted(visited_hashed));
                    auto narrowed = among;
                    auto narrowed_hashed = among_hashed;
                    urd::intersect_neighbours(fast, v, p, narrowed);
                    urd::intersect_neighbours(fast, v, p, narrowed_hashed);
                    EXPECT_EQ(sorted(narrowed), sorted(narrowed_hashed));
                    auto minus = among;
                    auto minus_hashed = among_hashed;
                    urd::subtract_neighbours(fast, v, p, minus);
                    urd::subtract_neighbours(fast, v, p, minus_hashed);
                    EXPECT_EQ(sorted(minus), sorted(minus_hashed));
                }
            // Over all vertices, with a set over a window shifted off the words (from 1).
            auto all = Dense(urd::key_range<V> { 0, static_cast<V>(vertices) });
            auto all_hashed = Hashed();
            urd::copy_neighbours(fast, v, all);
            urd::copy_neighbours(fast, v, all_hashed);
            EXPECT_EQ(sorted(all), out[v]);
            EXPECT_EQ(sorted(all_hashed), out[v]);
            auto among = Dense(urd::key_range<V> { 1, static_cast<V>(vertices) });
            random_set(among, 1, vertices);
            auto among_hashed = Hashed(among.begin(), among.end());
            auto visited = std::vector<V>();
            auto visited_hashed = std::vector<V>();
            urd::for_each_neighbour_in(fast, v, among, [&](V w) { visited.push_back(w); });
            urd::for_each_neighbour_in(fast, v, among_hashed, [&](V w) { visited_hashed.push_back(w); });
            EXPECT_EQ(visited, sorted(visited_hashed));
            auto narrowed = among;
            auto narrowed_hashed = among_hashed;
            urd::intersect_neighbours(fast, v, narrowed);
            urd::intersect_neighbours(fast, v, narrowed_hashed);
            EXPECT_EQ(sorted(narrowed), sorted(narrowed_hashed));
            auto minus = among;
            auto minus_hashed = among_hashed;
            urd::subtract_neighbours(fast, v, minus);
            urd::subtract_neighbours(fast, v, minus_hashed);
            EXPECT_EQ(sorted(minus), sorted(minus_hashed));
        }

        // The subgraph on random vertices, from rows and from edges, against the model renumbered.
        auto keep = Dense();
        random_set(keep, 0, vertices);
        const auto kept = std::vector<V>(keep.begin(), keep.end());
        const auto sub_fast = urd::induced<Rows>(fast, keep);
        const auto sub_slow = urd::induced<Plain>(slow, Hashed(keep.begin(), keep.end()));
        EXPECT_TRUE(same_edges(sub_fast, sub_slow));
        ASSERT_EQ(sub_fast.num_vertices(), kept.size());
        for (V u = 0; u < kept.size(); ++u)
            for (V v = 0; v < kept.size(); ++v)
                EXPECT_EQ(sub_fast.adjacent(u, v), adjacent[kept[u]][kept[v]]);
        if constexpr (Partitioned)
            for (V u = 0; u < kept.size(); ++u)
                EXPECT_EQ(sub_fast.part_of(u), part_of[kept[u]]);
    }
}

TEST(Graph, DenseAgreesWithModel)
{
    check_dense<Store<graph<V>, monotone, constant_adjacency>, Store<graph<V>, frozen, constant_adjacency>, Store<graph<V>, frozen>, false, false, false>(3);
    check_dense<Store<graph<V>, monotone, constant_adjacency, bidirectional>,
                Store<graph<V>, frozen, constant_adjacency, bidirectional>,
                Store<graph<V>, frozen, bidirectional>,
                false,
                false,
                true>(4);
    check_dense<Store<graph<V>, monotone, constant_adjacency, undirected>,
                Store<graph<V>, frozen, constant_adjacency, undirected>,
                Store<graph<V>, frozen>,
                false,
                true,
                false>(5);
    check_dense<Store<graph<V>, monotone, constant_adjacency, partitioned>,
                Store<graph<V>, frozen, constant_adjacency, partitioned>,
                Store<graph<V>, frozen>,
                true,
                false,
                false>(6);
    check_dense<Store<graph<V>, monotone, constant_adjacency, partitioned, bidirectional>,
                Store<graph<V>, frozen, constant_adjacency, partitioned, bidirectional>,
                Store<graph<V>, frozen, bidirectional>,
                true,
                false,
                true>(7);
    check_dense<Store<graph<V>, monotone, constant_adjacency, partitioned, undirected>,
                Store<graph<V>, frozen, constant_adjacency, partitioned, undirected>,
                Store<graph<V>, frozen>,
                true,
                true,
                false>(8);
}

/// An edge inside a part, or parts that do not add up to the vertices, throw.
TEST(Graph, PartitionedChecks)
{
    using rows = Store<graph<V>, frozen, partitioned, undirected, constant_adjacency>;
    const auto parts = std::vector<std::size_t> { 2, 2 };
    const auto inside = std::vector<std::pair<V, V>> { { 0, 1 } };
    EXPECT_THROW((rows(urd::graph_data<std::pair<V, V>> { 4, inside, parts })), std::invalid_argument);
    EXPECT_THROW((rows(urd::graph_data<std::pair<V, V>> { 5, {}, parts })), std::invalid_argument);
}

}  // namespace
