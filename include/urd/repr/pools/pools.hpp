#pragma once

#include "urd/config.hpp"
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
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

// Pools: rows identified by the handles insert gives, equal rows by equal handles.

namespace urd::repr
{

namespace detail
{

/// How a pool keeps a row of Row. As the elements the Row holds, in its order (element_rows), or, for a set of dense
/// indices, as the words of its bits, the trailing zero words dropped, so that equal sets are equal words (word_rows).
/// Each gives the stored form of given elements (canonical), a stored row with elements put in and taken out (changed),
/// what a stored row is handed out as (view), and its elements (size, contains, read).
template<typename Row>
struct element_rows
{
    using element = std::ranges::range_value_t<Row>;
    using stored_row = std::vector<element>;

    /// The elements as the Row keeps them (a set's in order, each once).
    template<std::ranges::input_range Elements>
    [[nodiscard]] static stored_row canonical(const Elements& elements)
    {
        const auto row = Row(std::ranges::begin(elements), std::ranges::end(elements));
        return stored_row(row.begin(), row.end());
    }
    /// A stored row as its elements: a row of column widths read as them, column by column; any other as it is.
    template<bool ThreadSafe>
    [[nodiscard]] static auto elements(const SchemaRow<ThreadSafe>& row)
    { return row.template as<element>(); }
    template<typename Stored>
    [[nodiscard]] static auto elements(const Stored& row)
    { return std::ranges::subrange(std::ranges::begin(row), std::ranges::end(row)); }
    /// What a stored row is handed out as: a vector's elements as a span, any other (a span, decoding rows) as it is.
    [[nodiscard]] static std::span<const element> view(const stored_row& row) noexcept { return row; }
    template<typename Stored>
    [[nodiscard]] static Stored view(const Stored& row)
    { return row; }

    template<typename Stored>
    [[nodiscard]] static stored_row copy(const Stored& row)
    {
        const auto held = elements(row);
        return stored_row(held.begin(), held.end());
    }
    template<typename Stored>
    [[nodiscard]] static stored_row changed(const Stored& row, std::span<const element> added, std::span<const element> removed)
    {
        auto held = copy(row);
        held.insert(held.end(), added.begin(), added.end());
        std::erase_if(held, [&removed](const element& each) { return std::ranges::find(removed, each) != removed.end(); });
        return canonical(held);
    }
    template<typename Stored>
    [[nodiscard]] static std::size_t size(const Stored& row)
    { return std::ranges::size(row); }
    template<typename Stored>
    [[nodiscard]] static bool contains(const Stored& row, const element& value)
    { return std::ranges::find(elements(row), value) != std::ranges::end(elements(row)); }
    template<typename Stored>
    static void read(const Stored& row, std::span<element> out)
    { std::ranges::copy(elements(row), out.begin()); }
    /// A stored row as the Row it was given as, built from its elements where they lie.
    template<typename Stored>
    [[nodiscard]] static Row row(const Stored& stored)
    {
        const auto all = elements(stored);
        return Row(all.begin(), all.end());
    }
};

template<typename Row>
struct word_rows
{
    using element = std::ranges::range_value_t<Row>;
    using stored_row = std::vector<word_t>;
    using bits_view = BitsetSpan<const word_t>;

    template<std::ranges::input_range Elements>
    [[nodiscard]] static stored_row canonical(const Elements& elements)
    {
        auto bits = stored_row();
        for (const auto& each : Row(std::ranges::begin(elements), std::ranges::end(elements)))
            set(bits, index_of<element>::to_index(each));
        return bits;
    }
    /// A row given as a dense set: its words as they are, a word at a time, not a key at a time.
    [[nodiscard]] static stored_row canonical(const Row& row)
        requires requires { row.significant_words(); }
    {
        const auto words = row.significant_words();
        return stored_row(words.begin(), words.end());
    }
    [[nodiscard]] static bits_view view(const stored_row& row) noexcept { return bits_view(row.data(), row.size() * bits_view::Digits); }

    [[nodiscard]] static stored_row changed(stored_row bits, std::span<const element> added, std::span<const element> removed)
    {
        for (const auto& each : added)
            set(bits, index_of<element>::to_index(each));
        for (const auto& each : removed)
            if (const auto at = index_of<element>::to_index(each); at / bits_view::Digits < bits.size())
                bit::bit_reference<word_t>(bits, at) = false;
        while (!bits.empty() && bits.back() == 0)
            bits.pop_back();
        return bits;
    }
    [[nodiscard]] static std::size_t size(const stored_row& row) noexcept { return view(row).count(); }
    [[nodiscard]] static bool contains(const stored_row& row, const element& value)
    {
        const auto at = index_of<element>::to_index(value);
        return at < view(row).size() && view(row).test(at);
    }
    static void read(const stored_row& row, std::span<element> out)
    {
        const auto bits = view(row);
        auto next = out.begin();
        for (auto at = bits.find_first(); at != bits_view::npos; at = bits.find_next(at))
            *next++ = index_of<element>::from_index(at);
    }
    /// A stored row as the Row it was given as: its set bits read out first.
    [[nodiscard]] static Row row(const stored_row& stored)
    {
        auto all = std::vector<element>(size(stored));
        read(stored, all);
        return Row(all.begin(), all.end());
    }

private:
    /// Sets bit `at`, growing the words to hold it.
    static void set(stored_row& bits, std::size_t at)
    {
        if (at / bits_view::Digits >= bits.size())
            bits.resize((at / bits_view::Digits) + 1, 0);
        bit::bit_reference<word_t>(bits, at) = true;
    }
};

/// A Store of a sequence: a row that keeps its elements as they are given.
template<typename Row>
concept sequence_store = is_store<Row> && std::same_as<typename traits_t<Row>::kind, sequence_kind>;

}  // namespace detail

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
/// uniform_rows.hpp: while all have one length, without offsets either); a row that is a sequence goes in and is looked up
/// as the span of its elements, no row built.
///
/// With Width bit (a request of urd::bit, rows of indices), each element is stored in the bit width the pool is built
/// with (urd/repr/containers/bit_rows.hpp), and view(h) hands out the row as a range that decodes its elements.
///
/// With Width by_column_widths (a request of urd::by_column_widths), rows are unsigned columns stored by the column
/// widths the pool is built with (urd/repr/containers/schema_rows.hpp): a row goes in as any range of values, one per
/// column (unsigned integers or indices), or as a row of equal widths; view(h) hands out a SchemaRow (get(i),
/// get<T>(i)), and EncodedRow (row_builder_type) builds one. A row is hashed by its values, so a lookup down a chain of
/// layered pools of different bits finds it in each.
template<typename Row, bool ThreadSafe, urd::width Width, typename H>
class RowInterner
{
    static constexpr bool Bits = Width == urd::width::bit;
    static constexpr bool by_column_widths = Width == urd::width::by_column_widths;
    using element = std::ranges::range_value_t<Row>;
    static constexpr bool words = !by_column_widths && has<traits_t<Row>, unique> && has<traits_t<Row>, dense>;  // a dense set: its bits
    /// How a row is kept: its elements, or a dense set's words.
    using Form = std::conditional_t<words, detail::word_rows<Row>, detail::element_rows<Row>>;
    using stored_row = typename Form::stored_row;
    static constexpr bool flat = Bits || by_column_widths || (!words && std::is_trivially_copyable_v<element> && Hashable<element>);
    /// A sequence stored flat keeps its elements as given: the given elements are the row.
    static constexpr bool as_given = flat && detail::sequence_store<Row>;
    using Table =
        std::conditional_t<by_column_widths,
                           SchemaInterningTable<stored_row, ThreadSafe>,
                           std::conditional_t<Bits,
                                              BitInterningTable<stored_row, ThreadSafe>,
                                              std::conditional_t<flat, FlatInterningTable<stored_row, ThreadSafe>, InterningTable<stored_row, ThreadSafe>>>>;
    /// A row of the widths, as a lookup takes it.
    using SchemaKey = SchemaRow<ThreadSafe>;
    /// Contiguous elements of a row, taken as they are.
    template<typename Elements>
    static constexpr bool contiguous_of =
        std::ranges::contiguous_range<Elements> && std::ranges::sized_range<Elements> && std::same_as<std::ranges::range_value_t<Elements>, element>;
    /// Elements a row is made of as they are given: a sequence's, contiguous.
    template<typename Elements>
    static constexpr bool given = as_given && contiguous_of<Elements>;

public:
    using value_type = Row;
    /// A row's handle is its index (H: urd::pool<V, H>): its position, in the order rows were first inserted.
    using handle_type = H;
    using write_type = Write<element>;
    using size_type = std::size_t;
    /// What a row is built as before it goes in (by_column_widths: an EncodedRow; void elsewhere).
    using row_builder_type = std::conditional_t<by_column_widths, EncodedRow<ThreadSafe>, void>;

    RowInterner()
        requires(!Bits && !by_column_widths)
    = default;
    /// With Bits, elements of any width (a word's bits): what a slot holds before its value is built with its width (a map
    /// of layered pools, Layered::try_emplace).
    RowInterner()
        requires Bits
        : RowInterner(urd::bit_width { static_cast<std::uint8_t>(word_bits) })
    {
    }
    /// Rows whose elements each fit in `width` (urd::bit).
    explicit RowInterner(urd::bit_width width)
        requires Bits
        : m_rows(width)
    {
    }
    /// With by_column_widths, no columns: what a slot holds before its value is built with its widths.
    RowInterner()
        requires(by_column_widths)
        : RowInterner(urd::column_widths())
    {
    }
    /// Rows of these columns (urd::by_column_widths).
    explicit RowInterner(const urd::column_widths& columns)
        requires(by_column_widths)
        : m_rows(columns)
    {
    }

    /// The handle of the row with these values, one per column, and whether it was stored now. A value that does not
    /// fit its column throws std::out_of_range, a value count other than the columns' std::invalid_argument, before
    /// anything is stored.
    template<std::ranges::input_range Values>
        requires by_column_widths
    std::pair<handle_type, bool> insert(const Values& values)
    {
        auto key = EncodedRow<ThreadSafe>(column_widths());
        encode(key, values);
        return intern(key.row());
    }
    /// By values with their hash (urd::hash_range of them as unsigned integers), which is not computed again.
    template<std::ranges::input_range Values>
        requires by_column_widths
    std::pair<handle_type, bool> insert(const prehashed<Values>& values)
    {
        auto key = EncodedRow<ThreadSafe>(column_widths());
        encode(key, values.key);
        return intern(prehashed<SchemaKey> { key.row(), values.hash });
    }
    /// The handle of the row with these values, or none (also where they do not fit the columns); no row is built for it.
    template<std::ranges::input_range Values>
        requires by_column_widths
    [[nodiscard]] handle_type find(const Values& values) const
    {
        const auto key = unsigned_values(values);
        return handle_of(urd::detail::indexed_access::position(m_rows, prehashed<decltype(key)> { key, hash_range(key) }));
    }
    template<std::ranges::input_range Values>
        requires by_column_widths
    [[nodiscard]] handle_type find(const prehashed<Values>& values) const
    {
        const auto key = unsigned_values(values.key);
        return handle_of(urd::detail::indexed_access::position(m_rows, prehashed<decltype(key)> { key, values.hash }));
    }
    /// The column widths its rows are stored by (urd::by_column_widths).
    [[nodiscard]] const urd::column_widths& column_widths() const noexcept
        requires by_column_widths
    { return urd::detail::indexed_access::built_with(m_rows); }

    /// The handle of the row with these elements, and whether it was stored now (no equal row was held): the elements
    /// in the row's stored form (canonical), or as they are where the row keeps them as given (given<Elements>).
    template<std::ranges::input_range Elements>
        requires(!by_column_widths && !given<Elements>)
    std::pair<handle_type, bool> insert(const Elements& elements)
    { return intern(canonical(elements)); }
    template<std::ranges::input_range Elements>
        requires(!by_column_widths && given<Elements>)
    std::pair<handle_type, bool> insert(const Elements& elements)
    { return intern(elements); }
    /// The handle of the row with these elements, or none.
    template<std::ranges::input_range Elements>
        requires(!by_column_widths && !given<Elements>)
    [[nodiscard]] handle_type find(const Elements& elements) const
    { return handle_of(urd::detail::indexed_access::position(m_rows, canonical(elements))); }
    template<std::ranges::input_range Elements>
        requires(!by_column_widths && given<Elements>)
    [[nodiscard]] handle_type find(const Elements& elements) const
    { return handle_of(urd::detail::indexed_access::position(m_rows, elements)); }
    /// By a row with its hash (urd::prehashed: the hash urd::hash_range gives its elements), which is not hashed
    /// again: a row of a sequence, stored flat, as its contiguous elements.
    template<typename Elements>
        requires(!by_column_widths && given<Elements>)
    std::pair<handle_type, bool> insert(const prehashed<Elements>& key)
    { return intern(key); }
    template<typename Elements>
        requires(!by_column_widths && given<Elements>)
    [[nodiscard]] handle_type find(const prehashed<Elements>& key) const
    {
        const auto found = urd::detail::indexed_access::position(m_rows, key);
        return found ? handle_type(static_cast<std::uint32_t>(*found)) : handle_type();
    }
    /// The bit width of the rows' elements (Bits: urd::bit).
    [[nodiscard]] urd::bit_width width() const noexcept
        requires Bits
    { return urd::detail::indexed_access::built_with(m_rows); }
    /// How many distinct rows are stored.
    [[nodiscard]] size_type size() const { return m_rows.size(); }

    /// The row `base` with `writes` applied (sorted, unique positions within it), stored in full.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const write_type> writes)
        requires(!words)
    {
        auto row = Form::copy(stored(base));
        for (const auto& write : writes)
            row.at(write.position) = write.value;
        return insert(row);
    }

    /// The row `base` with `added` put in and then `removed` taken out (rows that are sets), stored in full.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const element> added, std::span<const element> removed)
    { return insert_canonical(Form::changed(stored(base), added, removed)); }
    /// Whether the row of `handle` holds `element` (rows that are sets).
    [[nodiscard]] bool contains(handle_type handle, const element& value) const { return Form::contains(stored(handle), value); }

    /// The stored row, without copying: valid until the pool is cleared.
    [[nodiscard]] auto at(handle_type handle) const { return Form::view(stored(handle)); }

    [[nodiscard]] Row get(handle_type handle) const { return Form::row(stored(handle)); }
    [[nodiscard]] size_type row_size(handle_type handle) const { return Form::size(stored(handle)); }
    /// Copies the row's elements into `out`, which must hold row_size(handle) of them.
    void copy(handle_type handle, std::span<element> out) const
    {
        if (out.size() != row_size(handle))
            throw std::invalid_argument("urd::repr::RowInterner::copy: the output must hold the row");
        Form::read(stored(handle), out);
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
        urd::detail::indexed_access::swap_erase(m_rows, handle.value);
        return handle == last ? handle_type() : last;
    }

private:
    /// The elements as a stored row (Form::canonical).
    template<std::ranges::input_range Elements>
    [[nodiscard]] static stored_row canonical(const Elements& elements)
    { return Form::canonical(elements); }
    /// A row in its stored form, stored where it is new: encoded by the columns' widths, or as it is.
    std::pair<handle_type, bool> insert_canonical(const stored_row& row)
        requires by_column_widths
    { return insert(row); }
    std::pair<handle_type, bool> insert_canonical(stored_row&& row)
        requires(!by_column_widths)
    { return intern(std::move(row)); }
    /// The stored row: its vector, or where flat, the span of its elements.
    [[nodiscard]] decltype(auto) stored(handle_type handle) const
    {
        assert((handle.value) < m_rows.size());
        return urd::detail::indexed_access::element(m_rows, handle.value);
    }
    /// Values (unsigned integers or indices, index_of) as the unsigned values a stored row holds: what a row is looked up by.
    template<typename Values>
    [[nodiscard]] static auto unsigned_values(const Values& values)
    {
        return values
               | std::views::transform([](const auto& value) { return static_cast<word_t>(index_of<std::ranges::range_value_t<Values>>::to_index(value)); });
    }
    [[nodiscard]] static handle_type handle_of(std::optional<std::size_t> found)
    { return found ? handle_type(static_cast<std::uint32_t>(*found)) : handle_type(); }
    /// The values into `key`, by its widths: std::invalid_argument where they are not one per column, std::out_of_range
    /// where one does not fit.
    template<typename Values>
    static void encode(EncodedRow<ThreadSafe>& key, const Values& values)
    {
        if (std::ranges::distance(values) != static_cast<std::ptrdiff_t>(key.row().size()))
            throw std::invalid_argument("urd::by_column_widths: a row has one value per column");
        if (!key.assign(values))
            throw std::out_of_range("urd::by_column_widths: a value does not fit in the bits of its column");
    }
    template<typename Elements>
    std::pair<handle_type, bool> intern(Elements&& row)
    {
        const auto [found, inserted] = m_rows.insert(std::forward<Elements>(row));
        return { handle_type(static_cast<std::uint32_t>(found - m_rows.begin())), inserted };
    }

    Table m_rows;

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept { return Members<&RowInterner::m_rows> {}; }
};

}  // namespace urd::repr

namespace urd
{

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
    using type = repr::RowInterner<typename S::element_type, false, urd::width::word, typename S::handle_type>;
};
template<typename S>
struct representation_of<thread_safe_row_interner_decl, S>
{
    using type = repr::RowInterner<typename S::element_type, true, urd::width::word, typename S::handle_type>;
};
template<typename S, bool ThreadSafe>
struct representation_of<bit_row_interner_decl<ThreadSafe>, S>
{
    using type = repr::RowInterner<typename S::element_type, ThreadSafe, urd::width::bit, typename S::handle_type>;
};
template<typename S, bool ThreadSafe>
struct representation_of<schema_row_interner_decl<ThreadSafe>, S>
{
    using type = repr::RowInterner<typename S::element_type, ThreadSafe, urd::width::by_column_widths, typename S::handle_type>;
};

}  // namespace urd::detail
