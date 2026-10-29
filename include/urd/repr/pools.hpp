#pragma once

#include "urd/elements.hpp"
#include "urd/index.hpp"
#include "urd/interface.hpp"
#include "urd/repr/containers/bitset_span.hpp"
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

/// A pool of rows (Row: a Store, or any range constructible from its elements), each stored once and in full, in an
/// indexed set. A row goes in as any range of its elements and is first made a Row, so that the Row's own rules
/// (a set's order and uniqueness) decide which rows are equal. A row of dense indices (a dense set) is stored as its
/// bitset's words, the trailing zero words dropped, so that equal sets are equal words.
///
/// view(h) hands out the stored row without copying: a span of its elements, or a BitsetSpan over its words. Each row
/// keeps its storage while others are inserted (the table moves the rows' vectors, never their elements), so a view
/// stays valid until the pool is cleared.
template<typename Row, bool ThreadSafe = false>
class RowInterner
{
    using element = std::ranges::range_value_t<Row>;
    using word = std::uint64_t;
    static constexpr bool words = by_value<traits_t<Row>> && has<traits_t<Row>, dense>;
    using stored_row = std::vector<std::conditional_t<words, word, element>>;
    using Bits = BitsetSpan<const word>;

public:
    using value_type = Row;
    /// A row's handle is its index: its position, in the order rows were first inserted.
    using handle_type = urd::index<Row>;
    using write_type = Write<element>;
    using size_type = std::size_t;

    RowInterner() = default;

    template<std::ranges::input_range Elements>
    handle_type insert(const Elements& elements)
    {
        return intern(canonical(elements));
    }
    /// The handle of the row with these elements, or none.
    template<std::ranges::input_range Elements>
    [[nodiscard]] handle_type find(const Elements& elements) const
    {
        const auto found = m_rows.find(canonical(elements));
        return found == m_rows.end() ? handle_type() : handle_type(static_cast<std::uint32_t>(found - m_rows.begin()));
    }
    /// How many distinct rows are stored.
    [[nodiscard]] size_type size() const { return m_rows.size(); }

    /// The row `base` with `writes` applied (sorted, unique positions within it), stored in full.
    handle_type insert(handle_type base, std::span<const write_type> writes)
        requires(!words)
    {
        auto row = stored(base);
        for (const auto& write : writes)
            row.at(write.position) = write.value;
        return insert(row);
    }

    /// The row `base` with `added` put in and then `removed` taken out (rows that are sets), stored in full.
    handle_type insert(handle_type base, std::span<const element> added, std::span<const element> removed)
    {
        if constexpr (words)
        {
            auto bits = stored(base);
            for (const auto& each : added)
                set(bits, index_of<element>::to_index(each));
            for (const auto& each : removed)
                if (const auto at = index_of<element>::to_index(each); at / Bits::Digits < bits.size())
                    bits[at / Bits::Digits] &= ~(word { 1 } << (at % Bits::Digits));
            while (!bits.empty() && bits.back() == 0)
                bits.pop_back();
            return intern(std::move(bits));
        }
        else
        {
            auto elements = stored(base);
            elements.insert(elements.end(), added.begin(), added.end());
            std::erase_if(elements, [&removed](const element& each) { return std::ranges::find(removed, each) != removed.end(); });
            return insert(elements);
        }
    }
    /// Whether the row of `handle` holds `element` (rows that are sets).
    [[nodiscard]] bool contains(handle_type handle, const element& value) const
    {
        if constexpr (words)
        {
            const auto bits = view(handle);
            const auto at = index_of<element>::to_index(value);
            return at < bits.size() && bits.test(at);
        }
        else
            return std::ranges::find(stored(handle), value) != stored(handle).end();
    }

    /// The stored row, without copying: valid until the pool is cleared.
    [[nodiscard]] auto view(handle_type handle) const
    {
        const auto& row = stored(handle);
        if constexpr (words)
            return Bits(row.data(), row.size() * Bits::Digits);
        else
            return std::span<const element>(row);
    }

    [[nodiscard]] Row get(handle_type handle) const
    {
        if constexpr (words)
        {
            auto elements = std::vector<element>(row_size(handle));
            read(handle, elements);
            return Row(elements.begin(), elements.end());
        }
        else
        {
            const auto& row = stored(handle);
            return Row(row.begin(), row.end());
        }
    }
    [[nodiscard]] size_type row_size(handle_type handle) const
    {
        if constexpr (words)
            return view(handle).count();
        else
            return stored(handle).size();
    }
    /// Copies the row's elements into `out`, which must hold row_size(handle) of them.
    void read(handle_type handle, std::span<element> out) const
    {
        if (out.size() != row_size(handle))
            throw std::invalid_argument("urd::repr::RowInterner::read: the output must hold the row");
        if constexpr (words)
        {
            const auto bits = view(handle);
            auto next = out.begin();
            for (auto at = bits.find_first(); at != Bits::npos; at = bits.find_next(at))
                *next++ = index_of<element>::from_index(at);
        }
        else
            std::ranges::copy(stored(handle), out.begin());
    }

    /// The bytes its storage takes: the rows' elements, their vectors, and the table.
    [[nodiscard]] size_type memory_usage() const
    {
        return std::accumulate(m_rows.begin(),
                               m_rows.end(),
                               m_rows.memory_usage(),
                               [](size_type total, const auto& row) { return total + row.capacity() * sizeof(typename stored_row::value_type); });
    }
    void reserve(size_type count) { m_rows.reserve(count); }
    void clear() { m_rows.clear(); }

private:
    /// The elements as a stored row: as the Row keeps them (a set's in order, once), or as bitset words.
    template<std::ranges::input_range Elements>
    [[nodiscard]] static stored_row canonical(const Elements& elements)
    {
        const auto row = Row(std::ranges::begin(elements), std::ranges::end(elements));
        if constexpr (words)
        {
            auto bits = stored_row();
            for (const auto& each : row)
                set(bits, index_of<element>::to_index(each));
            return bits;
        }
        else
            return stored_row(row.begin(), row.end());
    }
    [[nodiscard]] const stored_row& stored(handle_type handle) const
    {
        assert(handle.value < m_rows.size());
        return m_rows[handle.value];
    }
    handle_type intern(stored_row row)
    {
        const auto [found, inserted] = m_rows.insert(std::move(row));
        return handle_type(static_cast<std::uint32_t>(found - m_rows.begin()));
    }
    /// Sets bit `at`, growing the words to hold it.
    static void set(stored_row& bits, std::size_t at)
    {
        if (at / Bits::Digits >= bits.size())
            bits.resize((at / Bits::Digits) + 1, 0);
        bits[at / Bits::Digits] |= word { 1 } << (at % Bits::Digits);
    }

    InterningTable<stored_row, ThreadSafe> m_rows;
};

}  // namespace urd::repr

namespace urd
{

/// Rows kept in full, each once; clear keeps the storage.
template<typename Row, bool ThreadSafe>
    requires hashable<std::vector<std::ranges::range_value_t<Row>>>
struct container_traits<repr::RowInterner<Row, ThreadSafe>> :
    properties_of<handles, unique, shrinkable, retains, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

/// Its rows have the properties Row has: they come back as Rows.
template<typename Row, bool ThreadSafe>
struct element_traits<repr::RowInterner<Row, ThreadSafe>> : container_traits<Row>
{
};

}  // namespace urd
