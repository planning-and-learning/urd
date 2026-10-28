#pragma once

#include "urd/elements.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

// Pools: rows identified by the handles insert gives, equal rows by equal handles.

namespace urd
{

/// A change to a row of a pool: the value at `position` becomes `value`. A pool of sequence rows takes a sorted
/// span of them, with unique positions, to insert a row that differs from another in few positions.
template<typename T>
struct Write
{
    std::size_t position = 0;
    T value {};
};

}  // namespace urd

namespace urd::repr
{

/// The handle of a row in a RowInterner: its position, in the order rows were first inserted.
struct RowHandle
{
    std::uint32_t index = 0;

    friend bool operator==(const RowHandle&, const RowHandle&) = default;
    static constexpr auto identifying_members() noexcept { return Members<&RowHandle::index> {}; }
};

/// A pool of rows (Row: a Store, or any range constructible from its elements), each stored once and in full, in an
/// indexed set. A row goes in as any range of its elements and is first made a Row, so that the Row's own rules
/// (a set's order and uniqueness) decide which rows are equal.
template<typename Row>
class RowInterner
{
    using element = std::ranges::range_value_t<Row>;

public:
    using value_type = Row;
    using handle_type = RowHandle;
    using write_type = Write<element>;
    using size_type = std::size_t;

    RowInterner() = default;

    template<std::ranges::input_range Elements>
    handle_type insert(const Elements& elements)
    {
        const auto row = Row(std::ranges::begin(elements), std::ranges::end(elements));
        const auto [found, inserted] = m_rows.insert(std::vector<element>(row.begin(), row.end()));
        return handle_type { static_cast<std::uint32_t>(found - m_rows.begin()) };
    }

    /// The row `base` with `writes` applied (sorted, unique positions within it), stored in full.
    handle_type insert(handle_type base, std::span<const write_type> writes)
    {
        auto row = stored(base);
        for (const auto& write : writes)
            row.at(write.position) = write.value;
        return insert(row);
    }

    [[nodiscard]] Row get(handle_type handle) const
    {
        const auto& row = stored(handle);
        return Row(row.begin(), row.end());
    }
    [[nodiscard]] size_type row_size(handle_type handle) const { return stored(handle).size(); }
    /// Copies the row's elements into `out`, which must hold row_size(handle) of them.
    void read(handle_type handle, std::span<element> out) const
    {
        if (out.size() != row_size(handle))
            throw std::invalid_argument("urd::repr::RowInterner::read: the output must hold the row");
        std::ranges::copy(stored(handle), out.begin());
    }

    /// The bytes its storage takes: the rows' elements, their vectors, and the table.
    [[nodiscard]] size_type memory_usage() const
    {
        return std::accumulate(m_rows.begin(),
                               m_rows.end(),
                               m_rows.memory_usage(),
                               [](size_type total, const auto& row) { return total + row.capacity() * sizeof(element); });
    }
    void reserve(size_type count) { m_rows.reserve(count); }
    void clear() { m_rows.clear(); }

private:
    [[nodiscard]] const std::vector<element>& stored(handle_type handle) const
    {
        assert(handle.index < m_rows.size());
        return m_rows[handle.index];
    }

    IndexedSet<std::vector<element>> m_rows;
};

}  // namespace urd::repr

namespace urd
{

/// Rows kept in full, each once; clear keeps the storage.
template<typename Row>
    requires hashable<std::vector<std::ranges::range_value_t<Row>>>
struct container_traits<repr::RowInterner<Row>> : properties_of<handles, shrinkable, retains>
{
};

/// Its rows have the properties Row has: they come back as Rows.
template<typename Row>
struct element_traits<repr::RowInterner<Row>> : container_traits<Row>
{
};

}  // namespace urd
