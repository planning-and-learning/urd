#pragma once

#include "urd/bits.hpp"
#include "urd/config.hpp"
#include "urd/graph_types.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/graphs/bit_matrix.hpp"
#include "urd/repr/graphs/graphs_fwd.hpp"
#include "urd/repr/graphs/part_rows.hpp"
#include "urd/repr/pools/pools.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/store.hpp"
#include "urd/traits.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace urd::repr
{

/// Whether graph G holds rows of bits as a graph of the given shape does: partitioned, undirected and with in-rows
/// alike (in-rows where undirected or bidirectional), so that its rows are copied as they are.
template<typename G, bool Partitioned, bool Undirected, bool InRows>
concept rows_shaped =
    has<G, constant_adjacency> && (has<G, partitioned> == Partitioned) && (has<G, undirected> == Undirected) && (!InRows || has<G, bidirectional>);

/// A frozen graph of rows of bits (port of tyr's DeduplicatedAdjacencyMatrix): per vertex and part a handle into a
/// pool of rows, equal rows stored once, interned a word at a time. Built from another graph of rows of the same shape
/// (a BitMatrix that was built up, say), or from edges through one.
template<typename V, bool Partitioned, bool Undirected, bool InRows>
class BitRows : public graphs::PartRows<BitRows<V, Partitioned, Undirected, InRows>, V, Partitioned, Undirected, InRows>
{
    using base = graphs::PartRows<BitRows<V, Partitioned, Undirected, InRows>, V, Partitioned, Undirected, InRows>;
    using Row = Store<set<std::size_t>, dense, frozen>;
    using Pool = Store<pool<Row>>;

public:
    BitRows() = default;

    /// From its vertex count, its edges and, Partitioned, its parts' sizes, which add up to the vertex count.
    template<input_edge_of<V, std::monostate> Edge>
    explicit BitRows(const graph_data<Edge>& data) : BitRows(BitMatrix<V, Partitioned, Undirected, InRows>(data))
    {
    }
    /// The rows of `graph`, each distinct one stored once.
    template<typename G>
        requires rows_shaped<G, Partitioned, Undirected, InRows>
    explicit BitRows(const G& graph)
    {
        const auto& rows = graphs::rows_of(graph);
        this->adopt(rows.num_vertices(), urd::detail::graph_rows::part_sizes(rows));
        intern(rows, m_handles, [](const auto& graph, V v, std::size_t p) { return urd::detail::graph_rows::row(graph, v, p); });
        intern_in(rows);
    }

    friend bool operator==(const BitRows& lhs, const BitRows& rhs)
    {
        if (!(static_cast<const base&>(lhs) == static_cast<const base&>(rhs)))
            return false;
        for (std::size_t v = 0; v < lhs.num_vertices(); ++v)
            for (std::size_t p = 0; p < lhs.num_parts(); ++p)
                if (!(lhs.row(index_of<V>::from_index(v), p) == rhs.row(index_of<V>::from_index(v), p)))
                    return false;
        return true;
    }

private:
    friend struct urd::detail::graph_rows;

    [[nodiscard]] bitset_span<const word_t> row(V v, std::size_t p) const { return m_pool[m_handles[(base::index(v) * this->num_parts()) + p]]; }
    [[nodiscard]] bitset_span<const word_t> in_row(V v, std::size_t p) const
        requires InRows
    { return m_pool[m_in_handles[(base::index(v) * this->num_parts()) + p]]; }

    using Handle = typename Pool::handle_type;

    /// A handle per vertex and part to its row of `rows` (read by `read`), interned.
    template<typename Rows, typename Read>
    void intern(const Rows& rows, std::vector<Handle>& into, Read read)
    {
        auto row = Row();
        into.reserve(this->num_vertices() * this->num_parts());
        for (std::size_t v = 0; v < this->num_vertices(); ++v)
            for (std::size_t p = 0; p < this->num_parts(); ++p)
            {
                const auto bits = read(rows, index_of<V>::from_index(v), p);
                row.assign_words(bits.blocks());
                into.push_back(m_pool.insert(row).first);
            }
    }
    template<typename Rows>
    void intern_in(const Rows& rows)
        requires InRows
    {
        intern(rows, m_in_handles, [](const auto& graph, V v, std::size_t p) { return urd::detail::graph_rows::in_row(graph, v, p); });
    }
    template<typename Rows>
    void intern_in(const Rows& /*rows*/)
        requires(!InRows)
    {
    }

    Pool m_pool;
    std::vector<Handle> m_handles;     ///< per vertex and part, its row
    std::vector<Handle> m_in_handles;  ///< InRows only: per vertex and part, its in-row

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept { return Members<&BitRows::m_parts, &BitRows::m_pool, &BitRows::m_handles, &BitRows::m_in_handles> {}; }
};

}  // namespace urd::repr

template<>
struct urd::include<"urd/repr/graphs/bit_rows.hpp">
{
};

namespace urd::detail
{

template<bool Partitioned, bool Undirected, bool InRows, typename S>
struct representation_of<bit_rows_decl<Partitioned, Undirected, InRows>, S>
{
    static_assert(std::same_as<typename S::edge_value_type, std::monostate> && std::same_as<typename S::vertex_value_type, std::monostate>,
                  "urd::graph: rows of bits hold no data: graph<V> without E and D");
    using type = repr::BitRows<typename S::vertex_type, Partitioned, Undirected, InRows>;
};

}  // namespace urd::detail
