#pragma once

#include "urd/cost.hpp"
#include "urd/elements/prehashed.hpp"
#include "urd/elements/index.hpp"
#include "urd/store/types.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

namespace urd::detail
{
/// What a pool's representation names a block and a block flip of its rows (urd::repr::TreeBitsetSet), else void.
template<typename R>
struct row_blocks_of
{
    using block_type = void;
    using flip_type = void;
};
template<typename R>
    requires requires {
        typename R::block_type;
        typename R::flip_type;
    }
struct row_blocks_of<R>
{
    using block_type = typename R::block_type;
    using flip_type = typename R::flip_type;
};
/// What a pool's representation hands out as a stored row (view(h)), else void.
template<typename R>
struct row_of
{
    using type = void;
};
template<typename R>
    requires requires(const R& rows, typename R::handle_type handle) { rows.view(handle); }
struct row_of<R>
{
    using type = decltype(std::declval<const R&>().view(std::declval<typename R::handle_type>()));
};
/// What a pool's representation builds a row as before it goes in (by_column_widths), else void.
template<typename R>
struct row_builder_of
{
    using type = void;
};
template<typename R>
    requires requires { typename R::row_builder_type; }
struct row_builder_of<R>
{
    using type = typename R::row_builder_type;
};
}  // namespace urd::detail

namespace urd::detail::facet
{

/// A pool: rows go in, handles come out; equal rows get equal handles, so a handle stands for its row (a state, a
/// fact set) in comparisons and as a key. The rows come back by handle.
template<typename Derived, typename R, typename S>
class pool
{
    using row = typename S::element_type;
    using row_element = std::ranges::range_value_t<row>;
    /// Rows that are sequences take writes by position.
    static constexpr bool sequence_rows = appends<typename held_as<typename S::element_type>::kind>;

    /// Erasing moves the last row into the hole: the handles must be positions (no structural_sharing) that no other
    /// thread reads as they move (no concurrent) and no layer over this one continues (no layered).
    static constexpr bool erasable = shrinks<S> && !has<S, structural_sharing> && !has<S, concurrent> && !has<S, layered>;

public:
    using handle_type = typename R::handle_type;
    /// A change to a sequence row: its value at a position.
    using write_type = Write<row_element>;
    /// Rows that are sets of indices kept as bits: a block of a row, and a change to one (void elsewhere).
    using block_type = typename row_blocks_of<R>::block_type;
    using flip_type = typename row_blocks_of<R>::flip_type;
    /// A stored row as view(h) hands it out, and what a row is built as before it goes in (by_column_widths:
    /// urd::repr::EncodedRow; void elsewhere).
    using row_type = typename row_of<R>::type;
    using row_builder_type = typename row_builder_of<R>::type;

    /// The handle of the row with these elements, and whether the insert stored anything: false where find would have
    /// given the handle, as an equal row is held (or, with structural_sharing, every node of it is).
    template<std::ranges::input_range Elements>
    std::pair<handle_type, bool> insert(const Elements& elements)
    { return representation().insert(elements); }
    std::pair<handle_type, bool> insert(std::initializer_list<row_element> elements) { return representation().insert(elements); }
    /// The row `base` with `writes` applied: sorted by position, each position once and within the row. A pool that
    /// shares structure stores only what changed.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const Write<row_element>> writes)
        requires sequence_rows
    {
        assert(std::ranges::is_sorted(writes, {}, &Write<row_element>::position));
        assert(std::ranges::adjacent_find(writes, {}, &Write<row_element>::position) == writes.end());
        assert(writes.empty() || writes.back().position < row_size(base));
        return representation().insert(base, writes);
    }

    /// The row `base` with `added` put in and then `removed` taken out (rows that are sets). A pool that shares
    /// structure stores only what changed.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const row_element> added, std::span<const row_element> removed)
        requires(!sequence_rows)
    { return representation().insert(base, added, removed); }
    /// Whether the row of `handle` holds `element` (rows that are sets); a tree pool reads one path, not the row.
    [[nodiscard]] bool contains(handle_type handle, const row_element& element) const
        requires(!sequence_rows)
    { return constant().contains(handle, element); }

    /// The handle insert(elements) would give, where inserting would store nothing; else none (handle.is_none()).
    /// Without structural_sharing that is an equal row inserted before. A pool with structural_sharing also finds a row
    /// it can build from the nodes it holds (a part of a longer row), which inserting would then not store either.
    /// By a row with its hash (urd/elements/prehashed.hpp; the hash urd::hash_range gives its elements), not hashed
    /// again: where the representation takes one (a pool of plain sequences).
    template<typename Elements>
        requires requires(R& rows, const prehashed<Elements>& key) { rows.insert(key); }
    std::pair<handle_type, bool> insert(const prehashed<Elements>& key)
    { return representation().insert(key); }
    template<typename Elements>
        requires requires(const R& rows, const prehashed<Elements>& key) { rows.find(key); }
    [[nodiscard]] handle_type find(const prehashed<Elements>& key) const
    { return constant().find(key); }
    template<typename Elements>
        requires requires(const R& rows, const prehashed<Elements>& key) { rows.find(key); }
    [[nodiscard]] bool contains(const prehashed<Elements>& key) const
    { return !find(key).is_none(); }
    /// The bit width it was built with (urd::bit), where its representation stores elements in it.
    [[nodiscard]] urd::bit_width width() const noexcept
        requires requires(const R& rows) { rows.width(); }
    { return constant().width(); }
    /// The column widths it was built with (urd::by_column_widths): its rows' columns.
    [[nodiscard]] const urd::column_widths& column_widths() const noexcept
        requires requires(const R& rows) { rows.column_widths(); }
    { return constant().column_widths(); }
    /// Whether a row with these elements is stored, as a set's contains.
    template<std::ranges::input_range Elements>
    [[nodiscard]] bool contains(const Elements& elements) const
    { return !find(elements).is_none(); }
    template<std::ranges::input_range Elements>
    [[nodiscard]] handle_type find(const Elements& elements) const
    { return constant().find(elements); }
    [[nodiscard]] handle_type find(std::initializer_list<row_element> elements) const
    { return find(std::span<const row_element>(elements.begin(), elements.size())); }

    [[nodiscard]] row get(handle_type handle) const { return constant().get(handle); }
    /// The stored row without copying, where the pool keeps rows in full: a span of the elements, or for a dense set
    /// its bits. Valid until the pool is cleared.
    [[nodiscard]] auto view(handle_type handle) const
        requires requires(const R& rows) { rows.view(handle); }
    { return constant().view(handle); }
    [[nodiscard]] std::size_t row_size(handle_type handle) const { return constant().row_size(handle); }
    /// The row's elements into `out`, which must hold row_size(handle) of them; allocates nothing.
    void read(handle_type handle, std::span<row_element> out) const { constant().read(handle, out); }
    /// The row into `out`, whose storage is reused, where the pool writes a row whole (a tree bitset into a dense set,
    /// word by word): allocates nothing where `out` holds the row's words already.
    void read(handle_type handle, row& out) const
        requires requires(const R& rows) { rows.read(handle, out); }
    { constant().read(handle, out); }

    // Rows that are sets of indices kept as bits, by their blocks, where the representation keeps them so: a row
    // changes by the blocks that differ, without listing its elements.

    /// The handle of the row with these blocks, whose last must not be zero.
    handle_type insert_blocks(std::span<const block_type> blocks)
        requires(!std::is_void_v<block_type>)
    { return representation().insert_blocks(blocks); }
    /// The row `base` with `flips` applied (sorted by unique position).
    handle_type insert_flips(handle_type base, std::span<const flip_type> flips)
        requires(!std::is_void_v<flip_type>)
    { return representation().insert_flips(base, flips); }
    /// The row's blocks into `blocks`, which must hold as many as the handle counts.
    void read_blocks(handle_type handle, std::span<block_type> blocks) const
        requires(!std::is_void_v<block_type>)
    { constant().read_blocks(handle, blocks); }
    /// Erases the row of `handle`: the last row moves into its place and takes `handle`. Gives the handle the moved
    /// row had, for whatever keeps handles to follow it, or none where `handle` was the last. O(1) expected; the moved
    /// row's views stay valid, the erased row's do not.
    handle_type erase(handle_type handle)
        requires(erasable)
    { return representation().erase(handle); }
    /// Forgets every row; the handles given so far mean nothing after. A layered pool forgets its own layer's rows.
    void clear()
        requires clears<S>
    { representation().clear(); }

private:
    [[nodiscard]] const R& constant() const { return store_access::of<Derived>(*this); }
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: its values are rows (ranges of elements).
template<>
struct facet_condition<facet::pool> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return held_as<typename S::element_type>::row; }
};
template<>
struct facet_operations<facet::pool>
{
    using offered = operation_list<operation::insert_row, operation::find_row, operation::access>;
};

}  // namespace urd::detail
