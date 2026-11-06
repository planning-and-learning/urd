#pragma once

#include "urd/elements/index.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/bit.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/pools/pools_fwd.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

// Pools: rows identified by the handles insert gives, equal rows by equal handles.

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
///
/// Rows of trivially copyable elements (flat) are stored flat, without a vector per row (urd/repr/containers/
/// flat_rows.hpp: while all have one width, without offsets either); a row that is a sequence goes in and is looked up
/// as the span of its elements, no row built.
///
/// With Bits (a request of urd::bit, rows of indices), each element is stored in the bit width the pool is built with
/// (urd/repr/containers/bit_rows.hpp), and view(h) hands out the row as a range that decodes its elements.
template<typename Row, bool ThreadSafe, bool Bits>
class RowInterner
{
    using element = std::ranges::range_value_t<Row>;
    using word = std::uint64_t;
    static constexpr bool words = has<traits_t<Row>, unique> && has<traits_t<Row>, dense>;  // a dense set: its bits
    using stored_row = std::vector<std::conditional_t<words, word, element>>;
    using BitsView = BitsetSpan<const word>;
    static constexpr bool flat = Bits || (!words && std::is_trivially_copyable_v<element> && Hashable<element>);
    /// A sequence keeps its elements as given: the given elements are the row.
    static constexpr bool as_given = []
    {
        if constexpr (flat && is_store<Row>)
            return std::same_as<typename traits_t<Row>::kind, sequence_kind>;
        else
            return false;
    }();
    using Table = std::conditional_t<Bits,
                                     BitInterningTable<stored_row, ThreadSafe>,
                                     std::conditional_t<flat, FlatInterningTable<stored_row, ThreadSafe>, InterningTable<stored_row, ThreadSafe>>>;
    /// Contiguous elements of a row, taken as they are.
    template<typename Elements>
    static constexpr bool contiguous_of = std::ranges::contiguous_range<Elements> && std::ranges::sized_range<Elements>
                                          && std::same_as<std::ranges::range_value_t<Elements>, element>;

public:
    using value_type = Row;
    /// A row's handle is its index: its position, in the order rows were first inserted.
    using handle_type = urd::index<Row>;
    using write_type = Write<element>;
    using size_type = std::size_t;

    RowInterner()
        requires(!Bits)
    = default;
    /// Rows whose elements each fit in `width` (urd::bit).
    explicit RowInterner(urd::bit_width width)
        requires Bits
        : m_rows(width)
    {
    }

    /// The handle of the row with these elements, and whether it was stored now (no equal row was held).
    template<std::ranges::input_range Elements>
    std::pair<handle_type, bool> insert(const Elements& elements)
    {
        if constexpr (as_given && contiguous_of<Elements>)
            return intern(elements);
        else
            return intern(canonical(elements));
    }
    /// The handle of the row with these elements, or none.
    template<std::ranges::input_range Elements>
    [[nodiscard]] handle_type find(const Elements& elements) const
    {
        const auto found = [&]
        {
            if constexpr (as_given && contiguous_of<Elements>)
                return m_rows.position(std::span<const element>(std::ranges::data(elements), std::ranges::size(elements)));
            else
                return m_rows.position(canonical(elements));
        }();
        return found ? handle_type(static_cast<std::uint32_t>(*found)) : handle_type();
    }
    /// By a row with its hash (urd::prehashed: the hash urd::hash_range gives its elements), which is not hashed
    /// again: a row of a sequence, stored flat, as its contiguous elements.
    template<typename Elements>
        requires as_given && contiguous_of<Elements>
    std::pair<handle_type, bool> insert(const prehashed<Elements>& key)
    { return intern(key); }
    template<typename Elements>
        requires as_given && contiguous_of<Elements>
    [[nodiscard]] handle_type find(const prehashed<Elements>& key) const
    {
        const auto found = m_rows.position(key);
        return found ? handle_type(static_cast<std::uint32_t>(*found)) : handle_type();
    }
    /// The bit width of the rows' elements (Bits: urd::bit).
    [[nodiscard]] urd::bit_width width() const noexcept
        requires Bits
    { return m_rows.width(); }
    /// How many distinct rows are stored.
    [[nodiscard]] size_type size() const { return m_rows.size(); }

    /// The row `base` with `writes` applied (sorted, unique positions within it), stored in full.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const write_type> writes)
        requires(!words)
    {
        auto row = copy_of(stored(base));
        for (const auto& write : writes)
            row.at(write.position) = write.value;
        return insert(row);
    }

    /// The row `base` with `added` put in and then `removed` taken out (rows that are sets), stored in full.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const element> added, std::span<const element> removed)
    {
        if constexpr (words)
        {
            auto bits = stored(base);
            for (const auto& each : added)
                set(bits, index_of<element>::to_index(each));
            for (const auto& each : removed)
                if (const auto at = index_of<element>::to_index(each); at / BitsView::Digits < bits.size())
                    bit::bit_reference<word>(bits, at) = false;
            while (!bits.empty() && bits.back() == 0)
                bits.pop_back();
            return intern(std::move(bits));
        }
        else
        {
            auto elements = copy_of(stored(base));
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
        {
            const auto row = view(handle);
            return std::ranges::find(row, value) != row.end();
        }
    }

    /// The stored row, without copying: valid until the pool is cleared.
    [[nodiscard]] auto view(handle_type handle) const
    {
        const auto& row = stored(handle);
        if constexpr (words)
            return BitsView(row.data(), row.size() * BitsView::Digits);
        else if constexpr (Bits)
            return row;  // decoding its elements
        else
            return std::span<const element>(std::ranges::data(row), std::ranges::size(row));
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
            const auto row = view(handle);
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
            for (auto at = bits.find_first(); at != BitsView::npos; at = bits.find_next(at))
                *next++ = index_of<element>::from_index(at);
        }
        else
            std::ranges::copy(stored(handle), out.begin());
    }

    void reserve(size_type count)
    {
        if constexpr (requires { m_rows.reserve(count); })
            m_rows.reserve(count);
    }
    void clear() { m_rows.clear(); }
    /// Erases the row of `handle` by moving the last row into its place, which then has `handle`: O(1) expected.
    /// Gives the handle the moved row had, or none where `handle` was the last. Views of the moved row stay valid.
    handle_type erase(handle_type handle)
        requires(!ThreadSafe)
    {
        assert((handle.value) < m_rows.size());
        const auto last = handle_type(static_cast<std::uint32_t>(m_rows.size() - 1));
        m_rows.swap_erase(handle.value);
        return handle == last ? handle_type() : last;
    }

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
    /// The stored row: its vector, or where flat, the span of its elements.
    [[nodiscard]] decltype(auto) stored(handle_type handle) const
    {
        assert((handle.value) < m_rows.size());
        return m_rows[handle.value];
    }
    [[nodiscard]] static stored_row copy_of(const auto& row) { return stored_row(row.begin(), row.end()); }
    template<typename Elements>
    std::pair<handle_type, bool> intern(Elements&& row)
    {
        const auto [found, inserted] = m_rows.insert(std::forward<Elements>(row));
        return { handle_type(static_cast<std::uint32_t>(found - m_rows.begin())), inserted };
    }
    /// Sets bit `at`, growing the words to hold it.
    static void set(stored_row& bits, std::size_t at)
    {
        if (at / BitsView::Digits >= bits.size())
            bits.resize((at / BitsView::Digits) + 1, 0);
        bit::bit_reference<word>(bits, at) = true;
    }

    friend struct urd::memory_of<RowInterner>;

    Table m_rows;
};

}  // namespace urd::repr

namespace urd
{

/// Its table of rows, each row's vector with it.
template<typename Row, bool ThreadSafe, bool Bits>
struct memory_of<repr::RowInterner<Row, ThreadSafe, Bits>>
{
    static std::size_t bytes(const repr::RowInterner<Row, ThreadSafe, Bits>& held) { return memory_usage(held.m_rows); }
};

template<>
struct include<"urd/repr/pools/pools.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<row_interner_decl, S>
{
    using type = repr::RowInterner<typename S::element_type>;
};
template<typename S>
struct representation_of<thread_safe_row_interner_decl, S>
{
    using type = repr::RowInterner<typename S::element_type, true>;
};
template<typename S, bool ThreadSafe>
struct representation_of<bit_row_interner_decl<ThreadSafe>, S>
{
    using type = repr::RowInterner<typename S::element_type, ThreadSafe, true>;
};

}  // namespace urd::detail
