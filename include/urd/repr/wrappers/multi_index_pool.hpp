#pragma once

// A pool of rows with hashed non-unique indexes on sets of columns, added at run time.

#include "urd/elements/hash.hpp"
#include "urd/elements/prehashed.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/core/gtl_memory.hpp"
#include "urd/repr/pools/pools_fwd.hpp"
#include "urd/repr/wrappers/multi_index_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace urd::repr
{

namespace detail
{

template<typename R>
struct multi_index_write_of
{
    using type = void;
};
template<typename R>
    requires requires { typename R::write_type; }
struct multi_index_write_of<R>
{
    using type = typename R::write_type;
};
template<typename R>
struct multi_index_row_builder_of
{
    using type = void;
};
template<typename R>
    requires requires { typename R::row_builder_type; }
struct multi_index_row_builder_of<R>
{
    using type = typename R::row_builder_type;
};

}  // namespace detail

/// A multi-index container of rows (Boost.MultiIndex): the pool R is its unique index on whole rows; each index added
/// by add_index is a hashed non-unique index on a set of columns (hashed_non_unique), whose key is those columns'
/// values in the order given. Indexes are added at run time, not fixed by the type. Rows with equal keys form a group,
/// linked through the rows, so inserting and erasing a row is O(1) expected per index, however many rows share its
/// key; count(key) is exact. Erasing moves the pool's last row into the erased row's handle; the indexes follow.
/// Iterating gives the handles 0..size()-1; an index's equal_range gives the handles of the rows with a key.
template<typename R>
class MultiIndexPool
{
    using T = typename R::value_type;
    using element = std::ranges::range_value_t<T>;
    using handle_value = std::uint32_t;
    static constexpr handle_value none = std::numeric_limits<handle_value>::max();

    /// The rows with one key: the first of them (each links to the next), their number, the key's hash, and the next
    /// group whose key hashes alike.
    struct Group
    {
        handle_value first = none;
        std::size_t count = 0;
        hash_t hash = 0;
        handle_value next_alike = none;
    };

    /// An index on `columns`: its groups (some free, in a free list through next_alike) and, by key hash, the first of
    /// the groups whose key hashes so; per row its group and its neighbours in the group.
    struct Index
    {
        std::vector<std::size_t> columns;
        std::vector<Group> groups;
        handle_value free = none;
        gtl::flat_hash_map<hash_t, handle_value> by_hash;
        std::vector<handle_value> group_of;
        std::vector<handle_value> next;
        std::vector<handle_value> previous;

        /// The members it owns memory through (urd::memory_of).
        static constexpr auto memory_members() noexcept
        { return Members<&Index::columns, &Index::groups, &Index::by_hash, &Index::group_of, &Index::next, &Index::previous> {}; }
    };

public:
    using value_type = T;
    using handle_type = typename R::handle_type;
    using write_type = typename detail::multi_index_write_of<R>::type;
    using row_builder_type = typename detail::multi_index_row_builder_of<R>::type;
    using size_type = std::size_t;

    /// The handles of rows in turn: of the whole pool (0..size()-1), or of one group of an index.
    class handle_iterator
    {
    public:
        using value_type = handle_type;
        using difference_type = std::ptrdiff_t;
        using reference = handle_type;
        using iterator_category = std::forward_iterator_tag;

        handle_iterator() = default;
        handle_iterator(const std::vector<handle_value>* next, handle_value at) noexcept : m_next(next), m_at(at) {}

        [[nodiscard]] handle_type operator*() const noexcept { return handle_type(m_at); }
        handle_iterator& operator++() noexcept
        {
            m_at = m_next == nullptr ? m_at + 1 : (*m_next)[m_at];
            return *this;
        }
        handle_iterator operator++(int) noexcept
        {
            auto result = *this;
            ++*this;
            return result;
        }
        [[nodiscard]] bool operator==(const handle_iterator& other) const noexcept { return m_at == other.m_at; }

    private:
        const std::vector<handle_value>* m_next = nullptr;  ///< Null: every handle in turn.
        handle_value m_at = none;
    };
    using iterator = handle_iterator;
    using const_iterator = handle_iterator;

    /// An index on a set of columns: a key is those columns' values, in the order add_index named them.
    class index_view
    {
    public:
        using key_type = std::span<const element>;
        using iterator = handle_iterator;
        using const_iterator = handle_iterator;
        using size_type = std::size_t;

        index_view(const MultiIndexPool& pool, const Index& index) noexcept : m_pool(&pool), m_index(&index) {}

        /// Its columns, in key order.
        [[nodiscard]] std::span<const std::size_t> columns() const noexcept { return m_index->columns; }
        /// The number of rows whose key is `key`.
        template<std::ranges::input_range Key>
        [[nodiscard]] size_type count(const Key& key) const
        {
            const auto group = m_pool->find_group(*m_index, key);
            return group == none ? 0 : m_index->groups[group].count;
        }
        template<std::ranges::input_range Key>
        [[nodiscard]] bool contains(const Key& key) const
        { return m_pool->find_group(*m_index, key) != none; }
        /// The first row whose key is `key`, else end().
        template<std::ranges::input_range Key>
        [[nodiscard]] iterator find(const Key& key) const
        { return equal_range(key).first; }
        /// The rows whose key is `key`.
        template<std::ranges::input_range Key>
        [[nodiscard]] std::pair<iterator, iterator> equal_range(const Key& key) const
        {
            const auto group = m_pool->find_group(*m_index, key);
            return { iterator(&m_index->next, group == none ? none : m_index->groups[group].first), end() };
        }
        [[nodiscard]] iterator end() const noexcept { return iterator(&m_index->next, none); }

    private:
        const MultiIndexPool* m_pool;
        const Index* m_index;
    };

    MultiIndexPool() = default;
    /// Its pool built from `args` (a bit row interner from its urd::bit_width, a schema row interner from its
    /// urd::column_widths).
    template<typename... Args>
        requires(sizeof...(Args) > 0 && std::constructible_from<R, Args...>)
    explicit MultiIndexPool(Args&&... args) : m_rows(std::forward<Args>(args)...)
    {
    }

    // NOLINTNEXTLINE(bugprone-exception-escape): the pool counts its rows; an empty pool's shared storage is made without allocating
    [[nodiscard]] size_type size() const noexcept { return m_rows.size(); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    [[nodiscard]] urd::bit_width width() const noexcept
        requires requires(const R& rows) { rows.width(); }
    { return m_rows.width(); }
    [[nodiscard]] const urd::column_widths& column_widths() const noexcept
        requires requires(const R& rows) { rows.column_widths(); }
    { return m_rows.column_widths(); }

    [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(nullptr, 0); }
    [[nodiscard]] const_iterator end() const noexcept { return const_iterator(nullptr, static_cast<handle_value>(size())); }

    /// The number of the index on `columns` (the key's columns, in order), added where it is new: the rows held are
    /// indexed now, the rows inserted later as they come.
    template<std::ranges::input_range Columns>
    std::size_t add_index(const Columns& columns)
    {
        auto wanted = std::vector<std::size_t>(std::ranges::begin(columns), std::ranges::end(columns));
        for (std::size_t n = 0; n < m_indexes.size(); ++n)
            if (m_indexes[n].columns == wanted)
                return n;
        auto& index = m_indexes.emplace_back();
        index.columns = std::move(wanted);
        for (handle_value at = 0; at < size(); ++at)
            link(index, at);
        return m_indexes.size() - 1;
    }
    /// The number of indexes added.
    [[nodiscard]] std::size_t index_count() const noexcept { return m_indexes.size(); }
    /// The index of number `n`.
    [[nodiscard]] index_view index(std::size_t n) const
    {
        assert(n < m_indexes.size() && "urd::repr::MultiIndexPool: no index of this number");
        return index_view(*this, m_indexes[n]);
    }

    template<std::ranges::input_range Elements>
    std::pair<handle_type, bool> insert(const Elements& elements)
    { return indexed(m_rows.insert(elements)); }
    template<typename Elements>
        requires requires(R& rows, const prehashed<Elements>& key) { rows.insert(key); }
    std::pair<handle_type, bool> insert(const prehashed<Elements>& key)
    { return indexed(m_rows.insert(key)); }
    /// The row `base` with `writes` applied.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const write_type> writes)
        requires requires(R& rows, handle_type at, std::span<const write_type> some) { rows.insert(at, some); }
    { return indexed(m_rows.insert(base, writes)); }
    template<std::ranges::input_range Elements>
    [[nodiscard]] handle_type find(const Elements& elements) const
    { return m_rows.find(elements); }
    template<typename Elements>
        requires requires(const R& rows, const prehashed<Elements>& key) { rows.find(key); }
    [[nodiscard]] handle_type find(const prehashed<Elements>& key) const
    { return m_rows.find(key); }
    [[nodiscard]] auto at(handle_type handle) const { return m_rows.at(handle); }
    [[nodiscard]] T get(handle_type handle) const { return m_rows.get(handle); }
    [[nodiscard]] size_type row_size(handle_type handle) const { return m_rows.row_size(handle); }
    void copy(handle_type handle, std::span<element> out) const { m_rows.copy(handle, out); }

    /// Erases the row of `handle`: the last row moves into its place, in the pool and in every index. Gives the handle
    /// the moved row had, or none where `handle` was the last.
    handle_type erase(handle_type handle)
    {
        const auto at = static_cast<handle_value>(handle.value);
        for (auto& index : m_indexes)
            unlink(index, at);
        const auto moved = m_rows.erase(handle);
        for (auto& index : m_indexes)
        {
            if (!moved.is_none())
                move(index, static_cast<handle_value>(moved.value), at);
            index.group_of.pop_back();
            index.next.pop_back();
            index.previous.pop_back();
        }
        return moved;
    }
    /// Forgets every row; the indexes stay, empty.
    void clear()
    {
        m_rows.clear();
        for (auto& index : m_indexes)
        {
            index.groups.clear();
            index.free = none;
            index.by_hash.clear();
            index.group_of.clear();
            index.next.clear();
            index.previous.clear();
        }
    }

private:
    /// Indexes a row the pool stored now.
    std::pair<handle_type, bool> indexed(std::pair<handle_type, bool> inserted)
    {
        if (inserted.second)
            for (auto& index : m_indexes)
                link(index, static_cast<handle_value>(inserted.first.value));
        return inserted;
    }
    /// The row's elements (into m_row).
    std::span<const element> elements_of(handle_value at) const
    {
        m_row.resize(m_rows.row_size(handle_type(at)));
        m_rows.copy(handle_type(at), m_row);
        return m_row;
    }
    /// The group of index whose key is `key`, else none.
    template<typename Key>
    handle_value find_group(const Index& index, const Key& key) const
    {
        const auto hash = hash_range(key);
        const auto found = index.by_hash.find(hash);
        if (found == index.by_hash.end())
            return none;
        for (auto group = found->second; group != none; group = index.groups[group].next_alike)
        {
            const auto row = elements_of(index.groups[group].first);
            if (std::ranges::equal(index.columns | std::views::transform([&](std::size_t column) { return row[column]; }), key))
                return group;
        }
        return none;
    }
    /// Links the row `at`, the last one, into its key's group (made where new).
    void link(Index& index, handle_value at)
    {
        const auto row = elements_of(at);
        m_key.clear();
        for (const auto column : index.columns)
            m_key.push_back(row[column]);
        auto group = find_group(index, m_key);
        if (group == none)
        {
            const auto hash = hash_range(m_key);
            if (index.free == none)
            {
                group = static_cast<handle_value>(index.groups.size());
                index.groups.emplace_back();
            }
            else
            {
                group = index.free;
                index.free = index.groups[group].next_alike;
            }
            const auto [found, inserted] = index.by_hash.try_emplace(hash, group);
            index.groups[group] = Group { none, 0, hash, inserted ? none : found->second };
            found->second = group;
        }
        auto& entry = index.groups[group];
        index.group_of.push_back(group);
        index.next.push_back(entry.first);
        index.previous.push_back(none);
        if (entry.first != none)
            index.previous[entry.first] = at;
        entry.first = at;
        ++entry.count;
    }
    /// Unlinks the row `at` from its group, which is freed where it held no other row.
    void unlink(Index& index, handle_value at)
    {
        const auto group = index.group_of[at];
        auto& entry = index.groups[group];
        const auto previous = index.previous[at], next = index.next[at];
        if (previous == none)
            entry.first = next;
        else
            index.next[previous] = next;
        if (next != none)
            index.previous[next] = previous;
        if (--entry.count != 0)
            return;
        // The group leaves the groups whose key hashes alike, and goes to the free list.
        const auto found = index.by_hash.find(entry.hash);
        if (found->second == group)
        {
            if (entry.next_alike == none)
                index.by_hash.erase(found);
            else
                found->second = entry.next_alike;
        }
        else
        {
            auto before = found->second;
            while (index.groups[before].next_alike != group)
                before = index.groups[before].next_alike;
            index.groups[before].next_alike = entry.next_alike;
        }
        entry.next_alike = index.free;
        index.free = group;
    }
    /// The row `from` moved to `to`: its links, and the links to it, follow.
    static void move(Index& index, handle_value from, handle_value to)
    {
        const auto group = index.group_of[from], previous = index.previous[from], next = index.next[from];
        index.group_of[to] = group;
        index.previous[to] = previous;
        index.next[to] = next;
        if (previous == none)
            index.groups[group].first = to;
        else
            index.next[previous] = to;
        if (next != none)
            index.previous[next] = to;
    }

    R m_rows;
    std::vector<Index> m_indexes;
    mutable std::vector<element> m_row;  ///< A row's elements, read to compare keys.
    std::vector<element> m_key;          ///< The key of a row being linked.

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept { return Members<&MultiIndexPool::m_rows, &MultiIndexPool::m_indexes> {}; }
};

}  // namespace urd::repr

namespace urd
{

template<>
struct include<"urd/repr/wrappers/multi_index_pool.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

/// Over its part's container for the request.
template<typename Part, typename S>
struct representation_of<multi_indexed_decl<Part>, S>
{
    using type = repr::MultiIndexPool<typename representation_of<Part, S>::type>;
};

}  // namespace urd::detail
