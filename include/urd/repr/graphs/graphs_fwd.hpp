#pragma once

// The graphs (urd::graph<V, E>), declared with what the registry reads of them: adjacency lists, which grow and shrink and are read both ways;
// compressed sparse rows, frozen; and rows of bits per vertex and part (constant_adjacency): a matrix that grows,
// and rows stored once, frozen, each partitioned or not, directed, with in-rows, or undirected. Defined in
// urd/repr/graphs/<container>.hpp.

#include "urd/cost.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <array>
#include <string_view>

namespace urd::repr
{

/// Adjacency lists: edges numbered as added, each vertex the numbers of the edges that leave it and that enter it.
/// Edges and vertices are removed in place; numbers are not reused.
template<typename V, typename E>
class AdjacencyLists;

/// Compressed sparse rows: each vertex's edges in a row, sorted by target, the position of an edge its number. InEdges
/// adds rows of the edges entering each vertex; Sources a source per edge.
template<typename V, typename E, bool InEdges, bool Sources>
class CsrGraph;

/// Rows of bits that grow: per vertex and part a row, the vertex's neighbours in the part (Partitioned: no edge inside
/// a part; else all vertices are one part), set both ways where Undirected, with InRows a row of in-neighbours too.
template<typename V, bool Partitioned, bool Undirected, bool InRows>
class BitMatrix;

/// Rows of bits, frozen: per vertex and part a handle into a pool of rows, equal rows stored once.
template<typename V, bool Partitioned, bool Undirected, bool InRows>
class BitRows;

}  // namespace urd::repr

namespace urd::detail
{

// A graph's words are per vertex, measured on the benchmarks' graphs of three edges per vertex (benchmarks/lifecycle.hpp,
// graph_workload): the edges a vertex has decide its memory, which an element's words cannot say otherwise.

struct adjacency_lists_decl
{
    static constexpr std::string_view id = "adjacency_lists";
    static constexpr auto headers = header_list<1> { "urd/repr/graphs/adjacency_lists.hpp" };
    using kind = graph_kind;
    static constexpr property_set provides = property_set::of({ position::positional, direction::bidirectional });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>());
    static constexpr std::array<tag, 0> gives_elements {};
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_a>,
                                       cost_entry<operation::erase, costs::lin_row>,
                                       cost_entry<operation::iterate, costs::lin_row>,
                                       cost_entry<operation::adjacency, costs::lin_row>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::access, costs::one>,
                                       cost_entry<operation::endpoints, costs::one>>;
    static constexpr double words = 27;
    static constexpr std::string_view description = "adjacency lists, out and in";
    static constexpr std::string_view holds = "per edge its endpoints, data and two list entries; per vertex two lists";
};

template<bool InEdges, bool Sources>
struct csr_graph_decl
{
    static constexpr std::string_view id =
        InEdges ? (Sources ? "bidirectional_csr_graph_with_sources" : "bidirectional_csr_graph") : (Sources ? "csr_graph_with_sources" : "csr_graph");
    static constexpr auto headers = header_list<1> { "urd/repr/graphs/csr_graph.hpp" };
    using kind = graph_kind;
    static constexpr property_set provides = property_set::of({ position::positional,
                                                                InEdges ? tag(direction::bidirectional) : tag(direction::none),
                                                                Sources ? tag(endpoints::constant_endpoints) : tag(endpoints::none) });
    static constexpr property_set relies_on = relies(stated({ change::frozen }), traits<role::element, trait::an_index>());
    static constexpr std::array<tag, 0> gives_elements {};
    using operation_costs = cost_table<cost_entry<operation::iterate, costs::lin_row>,
                                       cost_entry<operation::adjacency, costs::log_row>,
                                       cost_entry<operation::access, costs::one>,
                                       cost_entry<operation::endpoints, Sources ? costs::one : costs::log_n>>;
    static constexpr double words = 2.5 + (InEdges ? 5.5 : 0) + (Sources ? 1.5 : 0);
    static constexpr std::string_view description = "compressed sparse rows, by source and target";
    static constexpr std::string_view holds = "an offset per vertex; a target and data per edge";
};

/// What a graph of rows of bits provides and relies on: rows (constant_adjacency); partition where Partitioned, which
/// a request must state (else its edges inside a part would be refused); undirected likewise; in-neighbours where
/// undirected or with InRows.
template<bool Partitioned, bool Undirected, bool InRows>
struct bit_graph_facts
{
    static constexpr property_set provides = property_set::of({ adjacency::constant_adjacency,
                                                                Partitioned ? tag(grouping::partition) : tag(grouping::none),
                                                                Undirected ? tag(symmetry::undirected) : tag(symmetry::none),
                                                                Undirected || InRows ? tag(direction::bidirectional) : tag(direction::none) });
    /// What a request must state: rows (constant_adjacency, else it would get rows unasked), and its shape.
    static constexpr property_set shape = property_set::of({ adjacency::constant_adjacency,
                                                             Partitioned ? tag(grouping::partition) : tag(grouping::none),
                                                             Undirected ? tag(symmetry::undirected) : tag(symmetry::none) });
};

template<bool Partitioned, bool Undirected, bool InRows>
struct bit_matrix_decl
{
    static_assert(!(Undirected && InRows), "an undirected graph's in-rows are its rows");
    static constexpr std::string_view id = Partitioned ? (Undirected ? "partitioned_undirected_bit_matrix" :
                                                          InRows     ? "partitioned_bidirectional_bit_matrix" :
                                                                       "partitioned_bit_matrix") :
                                                         (Undirected ? "undirected_bit_matrix" :
                                                          InRows     ? "bidirectional_bit_matrix" :
                                                                       "bit_matrix");
    static constexpr auto headers = header_list<1> { "urd/repr/graphs/bit_matrix.hpp" };
    using kind = graph_kind;
    static constexpr property_set provides = bit_graph_facts<Partitioned, Undirected, InRows>::provides;
    static constexpr property_set relies_on =
        relies(stated({ change::monotone }), bit_graph_facts<Partitioned, Undirected, InRows>::shape, traits<role::element, trait::an_index>());
    static constexpr std::array<tag, 0> gives_elements {};
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one>,
                                       cost_entry<operation::iterate, costs::lin_n>,
                                       cost_entry<operation::adjacency, costs::one>,
                                       cost_entry<operation::clear, costs::lin_n>>;
    /// Up to 10,000 vertices: a row of n bits per vertex (each part's in whole words), twice with InRows.
    static constexpr double words = (Partitioned ? 176 : 168) * (InRows ? 2 : 1);
    static constexpr std::string_view description = "a row of bits per vertex and part (fixed-shape rows of dense sets)";
    static constexpr std::string_view holds = "per vertex its part and a row of n bits";
};

template<bool Partitioned, bool Undirected, bool InRows>
struct bit_rows_decl
{
    static_assert(!(Undirected && InRows), "an undirected graph's in-rows are its rows");
    static constexpr std::string_view id = Partitioned ? (Undirected ? "partitioned_undirected_bit_rows" :
                                                          InRows     ? "partitioned_bidirectional_bit_rows" :
                                                                       "partitioned_bit_rows") :
                                                         (Undirected ? "undirected_bit_rows" :
                                                          InRows     ? "bidirectional_bit_rows" :
                                                                       "bit_rows");
    static constexpr auto headers = header_list<1> { "urd/repr/graphs/bit_rows.hpp" };
    using kind = graph_kind;
    static constexpr property_set provides = bit_graph_facts<Partitioned, Undirected, InRows>::provides;
    static constexpr property_set relies_on =
        relies(stated({ change::frozen }), bit_graph_facts<Partitioned, Undirected, InRows>::shape, traits<role::element, trait::an_index>());
    static constexpr std::array<tag, 0> gives_elements {};
    using operation_costs = cost_table<cost_entry<operation::iterate, costs::lin_n>, cost_entry<operation::adjacency, costs::one>>;
    /// Up to 10,000 vertices: a handle per vertex and part, and the distinct rows (as many as vertices at worst).
    static constexpr double words = InRows ? 336 : 168;
    static constexpr std::string_view description = "rows of bits per vertex and part, each distinct row once (a pool of dense sets)";
    static constexpr std::string_view holds = "per vertex its part and a handle per part; the distinct rows";
};

template<typename V, typename E>
struct declaration_of<repr::AdjacencyLists<V, E>>
{
    using declaration = adjacency_lists_decl;
    static constexpr property_set roles = roles_of<urd::graph<V, E>>();
};
template<typename V, typename E, bool InEdges, bool Sources>
struct declaration_of<repr::CsrGraph<V, E, InEdges, Sources>>
{
    using declaration = csr_graph_decl<InEdges, Sources>;
    static constexpr property_set roles = roles_of<urd::graph<V, E>>();
};
template<typename V, bool Partitioned, bool Undirected, bool InRows>
struct declaration_of<repr::BitMatrix<V, Partitioned, Undirected, InRows>>
{
    using declaration = bit_matrix_decl<Partitioned, Undirected, InRows>;
    static constexpr property_set roles = roles_of<urd::graph<V>>();
};
template<typename V, bool Partitioned, bool Undirected, bool InRows>
struct declaration_of<repr::BitRows<V, Partitioned, Undirected, InRows>>
{
    using declaration = bit_rows_decl<Partitioned, Undirected, InRows>;
    static constexpr property_set roles = roles_of<urd::graph<V>>();
};

}  // namespace urd::detail
