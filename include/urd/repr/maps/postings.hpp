#pragma once

// Postings: multimaps whose values are found per key in a flat hash table. The grouped multimap over hashed groups,
// and chained postings, whose values share one pool of slots; the containers here pull in Boost.Unordered.

#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/core/boost_memory.hpp"
#include "urd/repr/maps/grouped_fwd.hpp"
#include "urd/repr/maps/grouped_multimap.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace urd::repr
{

/// Groups as postings: a flat hash table from each key to the vector of its values, in the order inserted, so looking
/// a key's values up is one probe; a group is an entry of the table. A key whose last value is erased leaves it.
template<typename K, typename V>
class HashGroups
{
    using Table = boost::unordered_flat_map<K, std::vector<V>, Hash<K>, EqualTo<K>>;

public:
    using key_type = K;
    using mapped_type = V;
    using group = typename Table::const_iterator;
    static constexpr bool ordered = false;
    static constexpr bool changes = true;

    HashGroups() = default;

    template<std::input_iterator InputIt>
    HashGroups(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->append(K(pair.first), V(pair.second)); });
    }

    [[nodiscard]] group begin() const { return m_table.begin(); }
    [[nodiscard]] group end() const { return m_table.end(); }
    [[nodiscard]] std::size_t count() const { return m_count; }
    [[nodiscard]] const K& key(group at) const { return at->first; }
    [[nodiscard]] std::span<const V> row(group at) const { return at->second; }
    [[nodiscard]] group find(const K& key) const { return m_table.find(key); }

    /// The group of `key` and the position of the value appended to it.
    std::pair<group, std::size_t> append(const K& key, V value)
    {
        const auto [found, inserted] = m_table.try_emplace(key);
        found->second.push_back(std::move(value));
        ++m_count;
        return { found, found->second.size() - 1 };
    }
    std::size_t erase(const K& key)
    {
        const auto found = m_table.find(key);
        if (found == m_table.end())
            return 0;
        const auto erased = found->second.size();
        m_table.erase(found);
        m_count -= erased;
        return erased;
    }
    /// Where the value after the erased one is.
    std::pair<group, std::size_t> erase(group at, std::size_t index)
    {
        auto& values = m_table.find(at->first)->second;
        values.erase(values.begin() + static_cast<std::ptrdiff_t>(index));
        --m_count;
        if (!values.empty())
            return { at, index };
        // Erasing from Boost's open-addressing table moves no other entry, so the next one stays where it is.
        const auto next = std::next(at);
        m_table.erase(at);
        return { next, 0 };
    }
    void clear()
    {
        m_table.clear();
        m_count = 0;
    }

private:
    friend struct urd::memory_of<HashGroups>;

    Table m_table;
    std::size_t m_count = 0;
};


/// A multimap as chained postings: a flat hash table from each key to the chain of its values, the values in one pool of
/// slots. Erasing a value links its slot into a free list, which inserting takes from before the pool grows, whatever
/// the key; clear() keeps the table's and the pool's storage. So, once warm, rebuilding and churning keys allocate
/// nothing: the storage is retained. A key's values are iterated newest first.
template<typename K, typename V>
class ChainedPostingMap
{
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    struct Slot
    {
        std::optional<V> value;  ///< Empty only while the slot is free.
        std::size_t next;
    };
    struct Chain
    {
        std::size_t first;
        std::size_t count;
    };
    using Table = boost::unordered_flat_map<K, Chain, Hash<K>, EqualTo<K>>;
    using entry = typename Table::const_iterator;

public:
    class iterator;
    using const_iterator = iterator;
    using key_type = K;
    using mapped_type = V;
    /// What iterating gives: a key and a view of one of its values.
    using value_type = std::pair<const K, const V&>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    /// The values of one key, newest first.
    class value_iterator
    {
    public:
        using value_type = V;
        using difference_type = std::ptrdiff_t;
        using reference = const V&;
        using iterator_category = std::forward_iterator_tag;

        value_iterator() = default;
        reference operator*() const { return *(*m_slots)[m_slot].value; }
        const V* operator->() const { return std::addressof(**this); }
        value_iterator& operator++()
        {
            m_slot = (*m_slots)[m_slot].next;
            return *this;
        }
        value_iterator operator++(int)
        {
            auto copy = *this;
            ++*this;
            return copy;
        }
        friend bool operator==(const value_iterator& lhs, const value_iterator& rhs) { return lhs.m_slot == rhs.m_slot; }

    private:
        friend class ChainedPostingMap;
        value_iterator(const std::vector<Slot>* slots, std::size_t slot) : m_slots(slots), m_slot(slot) {}

        const std::vector<Slot>* m_slots = nullptr;
        std::size_t m_slot = npos;
    };

    ChainedPostingMap() = default;

    /// From pairs in any order (std::pair<const K, V>, or what iterating another map gives).
    template<std::input_iterator InputIt>
    ChainedPostingMap(InputIt first, InputIt last)
    {
        insert(first, last);
    }

    [[nodiscard]] iterator begin() const { return iterator(this, m_table.begin()); }
    [[nodiscard]] iterator end() const { return iterator(this, m_table.end()); }
    [[nodiscard]] size_type size() const { return m_size; }
    [[nodiscard]] bool empty() const { return m_size == 0; }

    /// The values of `key`, newest first.
    [[nodiscard]] std::ranges::subrange<value_iterator> values(const K& key) const
    {
        const auto found = m_table.find(key);
        return { value_iterator(&m_slots, found == m_table.end() ? npos : found->second.first), value_iterator(&m_slots, npos) };
    }

    [[nodiscard]] iterator find(const K& key) const { return iterator(this, m_table.find(key)); }
    [[nodiscard]] size_type count(const K& key) const
    {
        const auto found = m_table.find(key);
        return found == m_table.end() ? 0 : found->second.count;
    }
    [[nodiscard]] bool contains(const K& key) const { return m_table.contains(key); }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) const
    {
        const auto found = m_table.find(key);
        if (found == m_table.end())
            return { end(), end() };
        return { iterator(this, found), iterator(this, std::next(found)) };
    }

    /// Prepends a value to its key's, from any pair (std::pair<const K, V>, or what iterating another map gives); takes
    /// a free slot before the pool grows.
    template<typename Pair>
        requires requires(const Pair& pair) {
            K(pair.first);
            V(pair.second);
        }
    iterator insert(const Pair& pair)
    {
        auto value = V(pair.second);
        const auto [found, inserted] = m_table.try_emplace(K(pair.first), Chain { npos, 0 });
        try
        {
            auto slot = m_free;
            if (slot == npos)
            {
                m_slots.push_back({ std::move(value), found->second.first });
                slot = m_slots.size() - 1;
            }
            else
            {
                m_slots[slot].value.emplace(std::move(value));
                m_free = m_slots[slot].next;
                m_slots[slot].next = found->second.first;
            }
            found->second.first = slot;
            ++found->second.count;
            ++m_size;
            return iterator(this, found, slot, npos);
        }
        catch (...)
        {
            if (inserted)
                m_table.erase(found);
            throw;
        }
    }
    template<typename Pair>
    iterator insert(const_iterator /*hint*/, const Pair& pair)
    { return insert(pair); }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->insert(pair); });
    }
    template<typename... Args>
    iterator emplace(Args&&... args)
    { return insert(std::pair<K, V>(std::forward<Args>(args)...)); }

    size_type erase(const K& key)
    {
        const auto found = m_table.find(key);
        if (found == m_table.end())
            return 0;
        const auto erased = found->second.count;
        for (auto slot = found->second.first; slot != npos;)
            slot = release(slot);
        m_table.erase(found);
        m_size -= erased;
        return erased;
    }
    /// The slot goes to the free list; where it held the key's last value, the key leaves.
    iterator erase(const_iterator pos)
    {
        auto& chain = m_table.find(pos.m_entry->first)->second;
        const auto next = release(pos.m_slot);
        if (pos.m_previous == npos)
            chain.first = next;
        else
            m_slots[pos.m_previous].next = next;
        --m_size;
        if (--chain.count != 0)
            return next == npos ? iterator(this, std::next(pos.m_entry)) : iterator(this, pos.m_entry, next, pos.m_previous);
        // Erasing from Boost's open-addressing table moves no other entry, so the next one stays where it is.
        const auto after = std::next(pos.m_entry);
        m_table.erase(pos.m_entry);
        return iterator(this, after);
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        for (auto remaining = std::distance(first, last); remaining > 0; --remaining)
            first = erase(first);
        return first;
    }
    /// Destroys the values, keeping the table's and the pool's storage.
    void clear()
    {
        for (auto& slot : m_slots)
            slot.value.reset();
        m_table.clear();
        // Every slot free, in order.
        for (std::size_t slot = 0; slot < m_slots.size(); ++slot)
            m_slots[slot].next = slot + 1 < m_slots.size() ? slot + 1 : npos;
        m_free = m_slots.empty() ? npos : 0;
        m_size = 0;
    }
    /// Room for `count` values and as many keys.
    void reserve(std::size_t count)
    {
        // Only growing: Boost's reserve rehashes to fit, and so shrinks a larger table.
        if (count > m_table.max_load())
            m_table.reserve(count);
        m_slots.reserve(count);
    }

    /// A value at a time, a key's together, with its key.
    class iterator
    {
    public:
        using value_type = typename ChainedPostingMap::value_type;
        using reference = value_type;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::input_iterator_tag;

        /// What -> points at: the pair, kept for the expression's lifetime.
        struct arrow
        {
            value_type pair;
            const value_type* operator->() const { return &pair; }
        };

        iterator() = default;

        reference operator*() const { return { m_entry->first, *m_map->m_slots[m_slot].value }; }
        arrow operator->() const { return { **this }; }
        iterator& operator++()
        {
            m_previous = m_slot;
            m_slot = m_map->m_slots[m_slot].next;
            if (m_slot == npos)
                *this = iterator(m_map, std::next(m_entry));
            return *this;
        }
        iterator operator++(int)
        {
            auto copy = *this;
            ++*this;
            return copy;
        }
        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.m_entry == rhs.m_entry && lhs.m_slot == rhs.m_slot; }

    private:
        friend class ChainedPostingMap;
        /// The first value of an entry, or the end.
        iterator(const ChainedPostingMap* map, entry at) :
            m_map(map),
            m_entry(at),
            m_slot(at == map->m_table.end() ? npos : at->second.first)
        {
        }
        iterator(const ChainedPostingMap* map, entry at, std::size_t slot, std::size_t previous) :
            m_map(map),
            m_entry(at),
            m_slot(slot),
            m_previous(previous)
        {
        }

        const ChainedPostingMap* m_map = nullptr;
        entry m_entry {};
        std::size_t m_slot = npos;
        std::size_t m_previous = npos;  ///< The slot before in the chain; npos at its first.
    };

private:
    friend struct urd::memory_of<ChainedPostingMap>;

    /// Frees a slot; the slot after it in its chain.
    std::size_t release(std::size_t slot)
    {
        const auto next = m_slots[slot].next;
        m_slots[slot].value.reset();
        m_slots[slot].next = m_free;
        m_free = slot;
        return next;
    }

    Table m_table;
    std::vector<Slot> m_slots;
    std::size_t m_free = npos;
    std::size_t m_size = 0;
};

}  // namespace urd::repr

/// The table of keys, and each key's row of values.
template<typename K, typename V>
struct urd::memory_of<urd::repr::HashGroups<K, V>>
{
    static std::size_t bytes(const repr::HashGroups<K, V>& held) { return memory_usage(held.m_table); }
};

/// The table of keys and the pool of slots, free ones included.
template<typename K, typename V>
struct urd::memory_of<urd::repr::ChainedPostingMap<K, V>>
{
    static std::size_t bytes(const repr::ChainedPostingMap<K, V>& held) { return memory_usage(held.m_table) + memory_usage(held.m_slots); }
};

namespace urd
{

template<>
struct include<"urd/repr/maps/postings.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<posting_map_decl, S>
{
    using type = repr::PostingMap<typename S::key_type, typename S::mapped_type>;
};
template<typename S>
struct representation_of<chained_posting_map_decl, S>
{
    using type = repr::ChainedPostingMap<typename S::key_type, typename S::mapped_type>;
};

}  // namespace urd::detail
