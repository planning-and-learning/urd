#pragma once

// What a graph of rows of bits reads, whatever holds its rows: per vertex and part, the vertex's neighbours in the part
// (Derived::row), and with InRows the vertices it is a neighbour of (Derived::in_row). A graph that is not partitioned
// is one part of all its vertices. The full matrix and the rows stored once differ only in how they hold rows.

#include "urd/bits.hpp"
#include "urd/config.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/store/types.hpp"
#include "urd/traits.hpp"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

namespace urd::detail
{

/// What urd's own operations on graphs (urd/graphs.hpp) and its graphs of rows reach of a graph of rows of bits, and
/// nothing else does: the row of a vertex in a part (bit i for the part's i-th vertex), and its in-row. The rows are
/// the containers' own, private; this accessor is the one way to them, so that no public interface hands out bits.
struct graph_rows
{
    template<typename Rows>
    [[nodiscard]] static auto row(const Rows& rows, typename Rows::vertex_type v, std::size_t p)
    { return rows.row(v, p); }
    template<typename Rows>
    [[nodiscard]] static auto in_row(const Rows& rows, typename Rows::vertex_type v, std::size_t p)
    { return rows.in_row(v, p); }
    /// Where part p's vertices begin, how many it has, and the sizes of all parts.
    template<typename Rows>
    [[nodiscard]] static std::size_t first_of(const Rows& rows, std::size_t p)
    { return rows.first_of(p); }
    template<typename Rows>
    [[nodiscard]] static std::size_t size_of(const Rows& rows, std::size_t p)
    { return rows.size_of(p); }
    template<typename Rows>
    [[nodiscard]] static std::vector<std::size_t> part_sizes(const Rows& rows)
    { return rows.part_sizes(); }
};

}  // namespace urd::detail

namespace urd::repr::graphs
{

/// Its vertices' parts: per part its first vertex (then the vertex count), per vertex its part.
struct Parts
{
    std::vector<std::size_t> first = std::vector<std::size_t>(1, 0);
    std::vector<std::uint32_t> of;

    friend bool operator==(const Parts&, const Parts&) = default;
    static constexpr auto memory_members() noexcept { return Members<&Parts::first, &Parts::of> {}; }
};

template<typename Derived, typename V, bool Partitioned, bool Undirected, bool InRows>
class PartRows
{
public:
    using vertex_type = V;
    using size_type = std::size_t;

    [[nodiscard]] size_type num_vertices() const noexcept { return m_parts.of.size(); }
    [[nodiscard]] auto vertices() const
    {
        return std::views::iota(std::size_t { 0 }, num_vertices()) | std::views::transform([](std::size_t at) { return index_of<V>::from_index(at); });
    }
    /// The neighbours of `u`, in increasing order.
    [[nodiscard]] auto neighbours(V u) const { return keys_of(u, rows_out()); }
    [[nodiscard]] size_type out_degree(V u) const { return count_of(u, rows_out()); }
    [[nodiscard]] bool adjacent(V u, V v) const
    {
        const auto p = m_parts.of[index(v)];
        const auto bits = urd::detail::graph_rows::row(self(), u, p);
        const auto at = index(v) - m_parts.first[p];
        return at < bits.size() && bits.test(at);
    }
    /// The vertices whose neighbour `v` is: its neighbours where undirected.
    [[nodiscard]] auto in_neighbours(V v) const
        requires Undirected
    { return neighbours(v); }
    [[nodiscard]] auto in_neighbours(V v) const
        requires InRows
    { return keys_of(v, rows_in()); }
    [[nodiscard]] size_type in_degree(V v) const
        requires Undirected
    { return out_degree(v); }
    [[nodiscard]] size_type in_degree(V v) const
        requires InRows
    { return count_of(v, rows_in()); }

    [[nodiscard]] size_type num_parts() const noexcept { return m_parts.first.size() - 1; }
    [[nodiscard]] size_type part_of(V v) const { return m_parts.of[index(v)]; }
    [[nodiscard]] auto part(std::size_t p) const
    {
        return std::views::iota(m_parts.first[p], m_parts.first[p + 1]) | std::views::transform([](std::size_t at) { return index_of<V>::from_index(at); });
    }

    friend bool operator==(const PartRows&, const PartRows&) = default;

protected:
    friend struct urd::detail::graph_rows;

    PartRows() = default;

    /// The index of part p's first vertex, and how many it has: whose keys a row of the part holds, from its bit 0.
    [[nodiscard]] std::size_t first_of(std::size_t p) const { return m_parts.first[p]; }
    [[nodiscard]] std::size_t size_of(std::size_t p) const { return m_parts.first[p + 1] - m_parts.first[p]; }
    /// The sizes of its parts.
    [[nodiscard]] std::vector<std::size_t> part_sizes() const
    {
        auto result = std::vector<std::size_t>();
        for (std::size_t p = 0; p < num_parts(); ++p)
            result.push_back(size_of(p));
        return result;
    }

    [[nodiscard]] static std::size_t index(V v) { return index_of<V>::to_index(v); }
    /// Takes the parts' sizes, which add up to `vertices`; a graph that is not partitioned is one part of all.
    void adopt(std::size_t vertices, std::span<const std::size_t> sizes)
    {
        const auto all = std::vector<std::size_t>(1, vertices);
        const auto parts = Partitioned ? sizes : std::span<const std::size_t>(all);
        if (std::accumulate(parts.begin(), parts.end(), std::size_t { 0 }) != vertices)
            throw std::invalid_argument("urd::graph: the parts' sizes do not add up to the vertex count");
        m_parts.first.assign(1, 0);
        m_parts.of.clear();
        for (std::size_t p = 0; p < parts.size(); ++p)
        {
            m_parts.first.push_back(m_parts.first.back() + parts[p]);
            m_parts.of.insert(m_parts.of.end(), parts[p], static_cast<std::uint32_t>(p));
        }
    }
    /// An edge's endpoints are vertices, and partitioned, of different parts; else std::invalid_argument.
    void check_edge(V u, V v) const
    {
        if (index(u) >= num_vertices() || index(v) >= num_vertices())
            throw std::invalid_argument("urd::graph: an edge's endpoint is not below the vertex count");
        if (Partitioned && m_parts.of[index(u)] == m_parts.of[index(v)])
            throw std::invalid_argument("urd::graph: a partitioned graph has no edge inside a part");
    }
    // The containers built on it list it among the members they own memory through (rule D3).
    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
    Parts m_parts;
    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

private:
    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
    /// How a vertex's rows, or in-rows, are read.
    static constexpr auto rows_out()
    {
        return [](const Derived& graph, V v, std::size_t p) { return urd::detail::graph_rows::row(graph, v, p); };
    }
    static constexpr auto rows_in()
    {
        return [](const Derived& graph, V v, std::size_t p) { return urd::detail::graph_rows::in_row(graph, v, p); };
    }
    /// The vertices whose bits the rows of `u` (as `rows` reads them) set, part after part.
    template<typename Rows>
    [[nodiscard]] auto keys_of(V u, Rows rows) const
    {
        return std::views::iota(std::size_t { 0 }, num_parts())
               | std::views::transform(
                   [this, u, rows](std::size_t p)
                   {
                       return std::ranges::owning_view(set_bit_indices(rows(self(), u, p)))
                              | std::views::transform([first = m_parts.first[p]](std::size_t bit) { return index_of<V>::from_index(first + bit); });
                   })
               | std::views::join;
    }
    template<typename Rows>
    [[nodiscard]] std::size_t count_of(V u, Rows rows) const
    {
        auto result = std::size_t { 0 };
        for (std::size_t p = 0; p < num_parts(); ++p)
            result += rows(self(), u, p).count();
        return result;
    }
};
}  // namespace urd::repr::graphs

namespace urd::repr::graphs
{

/// The container that holds the rows of graph `graph`: a Store's, or the graph itself where it is a container.
template<typename G>
    requires is_store<G>
const auto& rows_of(const G& graph)
{ return urd::detail::store_access::representation(graph); }
template<typename G>
    requires(!is_store<G>)
const G& rows_of(const G& graph)
{ return graph; }  // NOLINT(bugprone-return-const-ref-from-parameter): called with the graph an operation reads, never a temporary

}  // namespace urd::repr::graphs
