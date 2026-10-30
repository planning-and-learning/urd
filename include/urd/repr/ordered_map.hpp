#pragma once

#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A map in insertion order: its keys in Keys, an insertion-ordered set whose positions index the values, and the
/// values in a vector beside them. Keys and values are spans in that order. Keys chooses the key lookup: an
/// IndexedSet hashes them, a SparseSet<K, true> places dense ones by index (with O(1) clear).
template<typename Keys, typename V>
class OrderedMap
{
    using K = typename Keys::value_type;

public:
    class iterator;
    using const_iterator = iterator;
    using key_type = K;
    using mapped_type = V;
    /// What iterating gives: a key and a view of its value.
    using value_type = std::pair<const K, const V&>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    OrderedMap() = default;

    template<std::input_iterator InputIt>
    OrderedMap(InputIt first, InputIt last)
    {
        insert(first, last);
    }

    [[nodiscard]] iterator begin() const { return iterator(this, 0); }
    [[nodiscard]] iterator end() const { return iterator(this, size()); }
    [[nodiscard]] size_type size() const { return m_values.size(); }
    [[nodiscard]] bool empty() const { return m_values.empty(); }
    void reserve(size_type count)
    {
        m_keys.reserve(count);
        m_values.reserve(count);
    }

    /// The keys and the values, in insertion order.
    [[nodiscard]] std::span<const K> keys() const { return { m_keys.data(), m_keys.size() }; }
    [[nodiscard]] std::span<const V> values() const { return m_values; }

    [[nodiscard]] iterator find(const K& key) const
    {
        const auto found = m_keys.find(key);
        return found == m_keys.end() ? end() : iterator(this, static_cast<size_type>(found - m_keys.begin()));
    }
    [[nodiscard]] size_type count(const K& key) const { return m_keys.count(key); }
    [[nodiscard]] bool contains(const K& key) const { return m_keys.contains(key); }
    [[nodiscard]] const V& at(const K& key) const { return m_values[position_of(key)]; }
    [[nodiscard]] V& at(const K& key) { return m_values[position_of(key)]; }

    /// Inserts a value built from `args` unless `key` has one.
    template<typename... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args)
    {
        const auto [found, inserted] = m_keys.insert(key);
        const auto position = static_cast<size_type>(found - m_keys.begin());
        if (inserted)
            m_values.emplace_back(std::forward<Args>(args)...);
        return { iterator(this, position), inserted };
    }
    /// Sets the value for `key`, inserting it last where there is none.
    template<typename M>
    std::pair<iterator, bool> insert_or_assign(const K& key, M&& mapped)
    {
        if (const auto found = find(key); found != end())
        {
            m_values[found.m_position] = std::forward<M>(mapped);
            return { found, false };
        }
        return try_emplace(key, std::forward<M>(mapped));
    }
    /// A key and its value, as any pair (std::pair<const K, V>, or what iterating another map gives).
    template<typename Pair>
        requires requires(const Pair& pair) {
            K(pair.first);
            V(pair.second);
        }
    std::pair<iterator, bool> insert(const Pair& pair)
    {
        return try_emplace(K(pair.first), pair.second);
    }
    template<typename Pair>
    iterator insert(const_iterator /*hint*/, const Pair& pair)
    {
        return insert(pair).first;
    }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->insert(pair); });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        return insert(std::pair<K, V>(std::forward<Args>(args)...));
    }

    size_type erase(const K& key)
    {
        const auto found = find(key);
        if (found == end())
            return 0;
        erase(found);
        return 1;
    }
    /// Later entries move up to keep the order (O(n)).
    iterator erase(const_iterator pos)
    {
        const auto position = pos.m_position;
        m_keys.erase(m_keys.begin() + static_cast<difference_type>(position));
        m_values.erase(m_values.begin() + static_cast<difference_type>(position));
        return iterator(this, position);
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        for (auto remaining = last.m_position - first.m_position; remaining > 0; --remaining)
            first = erase(first);
        return first;
    }
    void clear()
    {
        m_keys.clear();
        m_values.clear();
    }

    /// An entry at a time, in insertion order, with a view of its value.
    class iterator
    {
    public:
        using value_type = typename OrderedMap::value_type;
        using reference = value_type;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::bidirectional_iterator_tag;
        using iterator_category = std::input_iterator_tag;

        /// What -> points at: the pair, kept for the expression's lifetime.
        struct arrow
        {
            value_type pair;
            const value_type* operator->() const { return &pair; }
        };

        iterator() = default;

        reference operator*() const { return { m_map->keys()[m_position], m_map->m_values[m_position] }; }
        arrow operator->() const { return { **this }; }
        iterator& operator++()
        {
            ++m_position;
            return *this;
        }
        iterator operator++(int)
        {
            auto copy = *this;
            ++*this;
            return copy;
        }
        iterator& operator--()
        {
            --m_position;
            return *this;
        }
        iterator operator--(int)
        {
            auto copy = *this;
            --*this;
            return copy;
        }
        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.m_position == rhs.m_position; }
        /// How many entries lie between: positions, so that an entry's position is `it - begin()`.
        friend difference_type operator-(const iterator& lhs, const iterator& rhs)
        {
            return static_cast<difference_type>(lhs.m_position) - static_cast<difference_type>(rhs.m_position);
        }

    private:
        friend class OrderedMap;
        iterator(const OrderedMap* map, size_type position) : m_map(map), m_position(position) {}

        const OrderedMap* m_map = nullptr;
        size_type m_position = 0;
    };

private:
    [[nodiscard]] size_type position_of(const K& key) const
    {
        const auto found = m_keys.find(key);
        if (found == m_keys.end())
            throw std::out_of_range("urd::repr::OrderedMap::at: no value for the key");
        return static_cast<size_type>(found - m_keys.begin());
    }

    Keys m_keys;
    std::vector<V> m_values;
};

}  // namespace urd::repr

namespace urd
{

/// In the order of its keys, which are as Keys keeps them (dense or not). Its entries are no contiguous pairs. Clear
/// costs what clearing Keys does where values need no destructor, else at most time in proportion to the entries.
template<typename Keys, typename V>
    requires has<container_traits<Keys>, insertion_ordered, unique>
struct container_traits<repr::OrderedMap<Keys, V>> :
    with_t<container_traits<Keys>,
           functional,
           by_key,  // the keys are positional, the entries are not: a map is reached by key
           any_layout,
           std::conditional_t<detail::trivially_destructible<V> && has<container_traits<Keys>, constant_clear>,
                              constant_clear,
                              std::conditional_t<has<container_traits<Keys>, proportional_clear>, proportional_clear, any_clear>>>
{
};

}  // namespace urd
