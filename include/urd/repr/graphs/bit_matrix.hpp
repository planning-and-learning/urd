#pragma once

#include "urd/bits.hpp"
#include "urd/config.hpp"
#include "urd/graph_types.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/graphs/graphs_fwd.hpp"
#include "urd/repr/graphs/part_rows.hpp"
#include "urd/repr/sequences/jagged.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/store.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace urd::repr
{

/// A graph of rows of bits that grows (monotone), its vertices fixed: per vertex and part a row of bits, the vertex's
/// neighbours in the part, held as fixed-shape rows of dense sets (vertex × part × bits). add_edge sets a bit in the
/// source's row, in the target's too where Undirected, and with InRows in the target's in-row. Not Partitioned, all
/// vertices are one part. What a graph of rows stored once is built from (urd::freeze).
template<typename V, bool Partitioned, bool Undirected, bool InRows>
class BitMatrix : public graphs::PartRows<BitMatrix<V, Partitioned, Undirected, InRows>, V, Partitioned, Undirected, InRows>
{
    using base = graphs::PartRows<BitMatrix<V, Partitioned, Undirected, InRows>, V, Partitioned, Undirected, InRows>;
    using Rows = Store<sequence<Store<sequence<Store<set<std::size_t>, dense>>, fixed_shape>>, fixed_shape>;

public:
    BitMatrix() = default;

    /// From its vertex count, its edges and, Partitioned, its parts' sizes, which add up to the vertex count.
    template<input_edge_of<V, std::monostate> Edge>
    explicit BitMatrix(const graph_data<Edge>& data)
    {
        this->adopt(data.vertices, data.parts);
        const auto shape = std::vector<std::vector<std::size_t>>(this->num_vertices(), this->part_sizes());
        m_rows = Rows(shape);
        m_in_rows = Rows(InRows ? shape : std::vector<std::vector<std::size_t>>());
        for (const auto& each : data.edges)
            add_edge(std::get<0>(each), std::get<1>(each));
    }

    /// Adds the edge from `u` to `v`; whether it is new. Endpoints past the vertices, or Partitioned in one part, throw
    /// std::invalid_argument.
    bool add_edge(V u, V v)
    {
        this->check_edge(u, v);
        const bool added = !this->adjacent(u, v);
        set_bit(m_rows, u, v);
        set_back(u, v);
        return added;
    }
    void clear()
    {
        m_rows.reset();
        m_in_rows.reset();
    }

    friend bool operator==(const BitMatrix&, const BitMatrix&) = default;

private:
    friend struct urd::detail::graph_rows;

    /// The neighbours of `v` in part `p` (bit i for the part's i-th vertex), and with InRows the vertices of part p that
    /// `v` is a neighbour of.
    [[nodiscard]] bitset_span<const word_t> row(V v, std::size_t p) const { return std::as_const(m_rows)[base::index(v)][static_cast<std::ptrdiff_t>(p)]; }
    [[nodiscard]] bitset_span<const word_t> in_row(V v, std::size_t p) const
        requires InRows
    { return std::as_const(m_in_rows)[base::index(v)][static_cast<std::ptrdiff_t>(p)]; }

    /// Makes every vertex of part p adjacent to every vertex of part q: each one's row into the other part filled, a
    /// word at a time. p and q the same part throws std::invalid_argument.
    void connect_parts(std::size_t p, std::size_t q)
    {
        if (p == q)
            throw std::invalid_argument("urd::graph: a partitioned graph has no edge inside a part");
        for (auto u = this->first_of(p); u < this->first_of(p) + this->size_of(p); ++u)
            m_rows[u][static_cast<std::ptrdiff_t>(q)].set();
        for (auto v = this->first_of(q); v < this->first_of(q) + this->size_of(q); ++v)
        {
            if constexpr (Undirected)
                m_rows[v][static_cast<std::ptrdiff_t>(p)].set();
            if constexpr (InRows)
                m_in_rows[v][static_cast<std::ptrdiff_t>(p)].set();
        }
    }

    /// Sets `v` in the row of `u` in `rows`.
    void set_bit(Rows& rows, V u, V v)
    {
        const auto p = this->part_of(v);
        rows[base::index(u)][static_cast<std::ptrdiff_t>(p)].set(base::index(v) - this->first_of(p));
    }
    /// The other side of an edge: `u` in the row of `v` where undirected, in its in-row with InRows.
    void set_back(V u, V v)
        requires Undirected
    { set_bit(m_rows, v, u); }
    void set_back(V u, V v)
        requires InRows
    { set_bit(m_in_rows, v, u); }
    void set_back(V /*u*/, V /*v*/)
        requires(!Undirected && !InRows)
    {
    }

    Rows m_rows;
    Rows m_in_rows;  ///< InRows only

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept { return Members<&BitMatrix::m_parts, &BitMatrix::m_rows, &BitMatrix::m_in_rows> {}; }
};

}  // namespace urd::repr

template<>
struct urd::include<"urd/repr/graphs/bit_matrix.hpp">
{
};

namespace urd::detail
{

template<bool Partitioned, bool Undirected, bool InRows, typename S>
struct representation_of<bit_matrix_decl<Partitioned, Undirected, InRows>, S>
{
    static_assert(std::same_as<typename S::edge_value_type, std::monostate> && std::same_as<typename S::vertex_value_type, std::monostate>,
                  "urd::graph: rows of bits hold no data: graph<V> without E and D");
    using type = repr::BitMatrix<typename S::vertex_type, Partitioned, Undirected, InRows>;
};

}  // namespace urd::detail
