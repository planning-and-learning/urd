#pragma once

#include "urd/elements.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstddef>
#include <iterator>
#include <span>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A multimap as postings: a flat hash table from each key to the vector of its values, in the order inserted. A
/// key's values lie together, so looking them up is one probe and reading them a span. A key whose last value is
/// erased leaves the table. The registry names it for any key type, so it is an unconstrained template; its
/// container_traits declare properties only for hashable keys.
template<typename K, typename V>
class PostingMap
{
    using Table = boost::unordered_flat_map<K, std::vector<V>, Hash<K>, EqualTo<K>>;
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

    PostingMap() = default;

    template<std::input_iterator InputIt>
    PostingMap(InputIt first, InputIt last)
    { insert(first, last); }

    [[nodiscard]] iterator begin() const { return iterator(m_table.begin()); }
    [[nodiscard]] iterator end() const { return iterator(m_table.end()); }
    [[nodiscard]] size_type size() const { return m_count; }
    [[nodiscard]] bool empty() const { return m_count == 0; }

    /// The values of `key`, in the order inserted.
    [[nodiscard]] std::span<const V> values(const K& key) const
    {
        const auto found = m_table.find(key);
        return found == m_table.end() ? std::span<const V>() : std::span<const V>(found->second);
    }

    [[nodiscard]] iterator find(const K& key) const { return iterator(m_table.find(key)); }
    [[nodiscard]] size_type count(const K& key) const { return values(key).size(); }
    [[nodiscard]] bool contains(const K& key) const { return m_table.contains(key); }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) const
    {
        const auto found = m_table.find(key);
        return { iterator(found), iterator(found == m_table.end() ? found : std::next(found)) };
    }

    /// Appends a value to its key's, from any pair (std::pair<const K, V>, or what iterating another map gives).
    template<typename Pair>
        requires requires(const Pair& pair) {
            K(pair.first);
            V(pair.second);
        }
    iterator insert(const Pair& pair)
    {
        const auto [found, inserted] = m_table.try_emplace(K(pair.first));
        found->second.push_back(V(pair.second));
        ++m_count;
        return iterator(found, found->second.size() - 1);
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
        const auto erased = found->second.size();
        m_table.erase(found);
        m_count -= erased;
        return erased;
    }
    /// The values after `pos` move up within its key; where it was the key's last, the key leaves the table.
    iterator erase(const_iterator pos)
    {
        auto& row = m_table.find(pos.m_entry->first)->second;
        row.erase(row.begin() + static_cast<difference_type>(pos.m_index));
        --m_count;
        if (row.empty())
        {
            // Erasing from Boost's open-addressing table moves no other entry, so the next one stays where it is.
            const auto next = std::next(pos.m_entry);
            m_table.erase(pos.m_entry);
            return iterator(next);
        }
        return pos.m_index < row.size() ? pos : iterator(std::next(pos.m_entry));
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        for (auto remaining = std::distance(first, last); remaining > 0; --remaining)
            first = erase(first);
        return first;
    }
    void clear()
    {
        m_table.clear();
        m_count = 0;
    }

    /// A value at a time, a key's together, with its key.
    class iterator
    {
    public:
        using value_type = typename PostingMap::value_type;
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

        reference operator*() const { return { m_entry->first, m_entry->second[m_index] }; }
        arrow operator->() const { return { **this }; }
        iterator& operator++()
        {
            if (++m_index == m_entry->second.size())
            {
                ++m_entry;
                m_index = 0;
            }
            return *this;
        }
        iterator operator++(int)
        {
            auto copy = *this;
            ++*this;
            return copy;
        }
        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.m_entry == rhs.m_entry && lhs.m_index == rhs.m_index; }

    private:
        friend class PostingMap;
        explicit iterator(entry at, std::size_t index = 0) : m_entry(at), m_index(index) {}

        entry m_entry;
        std::size_t m_index = 0;
    };

private:
    Table m_table;
    size_type m_count = 0;
};

}  // namespace urd::repr

namespace urd
{

/// Values grouped by key, in no order of keys; erase and clear free what the removed keys' vectors hold.
template<hashable K, typename V>
struct container_traits<repr::PostingMap<K, V>> : properties_of<multi, sparse, shrinkable>
{
};

}  // namespace urd
