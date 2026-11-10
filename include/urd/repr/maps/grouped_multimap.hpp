#pragma once

// A multimap whose values are grouped by key, each key's values one span, over a grouping policy.

#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/repr/maps/grouped_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A multimap whose values lie together per key, in groups: the multimap interface, written once over how Groups
/// keeps them. Iterating gives each value with its key, a key's values together; values(k) is a span. Groups names
/// its groups by a cursor (`group`), and says what it offers: `ordered` groups (by key index, iterated both ways,
/// with bounds), and `changes` (append and erase).
template<typename Groups>
class GroupedMultiMap
{
    using K = typename Groups::key_type;
    using V = typename Groups::mapped_type;
    using group = typename Groups::group;

public:
    class iterator;
    using const_iterator = iterator;
    using key_type = K;
    using mapped_type = V;
    /// What iterating gives: a key and a view of one of its values.
    using value_type = std::pair<const K, const V&>;
    using reference = value_type;  ///< an entry is handed out by value: its key and a reference to its value
    using const_reference = value_type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    GroupedMultiMap() = default;

    /// From pairs in any order (std::pair<const K, V>, or what iterating another map gives).
    template<std::input_iterator InputIt>
    GroupedMultiMap(InputIt first, InputIt last) : m_groups(first, last)
    {
    }

    /// Rebuilds from pairs, in the storage held, where the groups can.
    template<std::input_iterator InputIt>
        requires requires(Groups& groups, InputIt first) { groups.assign(first, first); }
    void assign(InputIt first, InputIt last)
    { m_groups.assign(first, last); }

    [[nodiscard]] iterator begin() const { return iterator(&m_groups, m_groups.begin(), 0).normalized(); }
    [[nodiscard]] iterator end() const { return iterator(&m_groups, m_groups.end(), 0); }
    [[nodiscard]] size_type size() const { return m_groups.count(); }
    [[nodiscard]] bool empty() const { return size() == 0; }

    /// The values of `key`, in the order they were given.
    [[nodiscard]] std::span<const V> values(const K& key) const
    {
        const auto found = m_groups.find(key);
        return found == m_groups.end() ? std::span<const V>() : m_groups.row(found);
    }

    [[nodiscard]] iterator find(const K& key) const { return contains(key) ? iterator(&m_groups, m_groups.find(key), 0) : end(); }
    [[nodiscard]] size_type count(const K& key) const { return values(key).size(); }
    [[nodiscard]] bool contains(const K& key) const { return !values(key).empty(); }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) const
    {
        if constexpr (Groups::ordered)
        {
            // The key's values and no further: the end is the next key's place, whether or not that key has values (an
            // iterator settles on a value when it is read), so keys without values after this one are not walked over.
            const auto first = m_groups.lower_bound(key), last = m_groups.upper_bound(key);
            const auto none = first == last || m_groups.row(first).empty();
            return { iterator(&m_groups, none ? last : first, 0), iterator(&m_groups, last, 0) };
        }
        if (!contains(key))
            return { end(), end() };
        const auto found = m_groups.find(key);
        auto after = found;
        return { iterator(&m_groups, found, 0), iterator(&m_groups, ++after, 0).normalized() };
    }
    [[nodiscard]] iterator lower_bound(const K& key) const
        requires Groups::ordered
    { return iterator(&m_groups, m_groups.lower_bound(key), 0).normalized(); }
    [[nodiscard]] iterator upper_bound(const K& key) const
        requires Groups::ordered
    { return iterator(&m_groups, m_groups.upper_bound(key), 0).normalized(); }

    /// Appends a value to its key's, from any pair (std::pair<const K, V>, or what iterating another map gives).
    template<typename Pair>
        requires Groups::changes && requires(const Pair& pair) {
            K(pair.first);
            V(pair.second);
        }
    iterator insert(const Pair& pair)
    {
        const auto [at, index] = m_groups.append(K(pair.first), V(pair.second));
        return iterator(&m_groups, at, index);
    }
    template<typename Pair>
        requires Groups::changes
    iterator insert(const_iterator /*hint*/, const Pair& pair)
    { return insert(pair); }
    template<std::input_iterator InputIt>
        requires Groups::changes
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->insert(pair); });
    }
    template<typename... Args>
        requires Groups::changes
    iterator emplace(Args&&... args)
    { return insert(std::pair<K, V>(std::forward<Args>(args)...)); }

    size_type erase(const K& key)
        requires Groups::changes
    { return m_groups.erase(key); }
    /// The values after `pos` move up within its key; where it was the key's last, the key leaves.
    iterator erase(const_iterator pos)
        requires Groups::changes
    {
        const auto [at, index] = m_groups.erase(pos.settled(), pos.m_index);
        return iterator(&m_groups, at, index).normalized();
    }
    iterator erase(const_iterator first, const_iterator last)
        requires Groups::changes
    {
        for (auto remaining = std::distance(first, last); remaining > 0; --remaining)
            first = erase(first);
        return first;
    }
    void clear()
        requires Groups::changes
    { m_groups.clear(); }

    /// Where Groups compares; else a Store compares by iterating.
    [[nodiscard]] friend bool operator==(const GroupedMultiMap& lhs, const GroupedMultiMap& rhs)
        requires std::equality_comparable<Groups>
    { return lhs.m_groups == rhs.m_groups; }

    /// A value at a time, a key's together, with its key.
    class iterator
    {
    public:
        using value_type = typename GroupedMultiMap::value_type;
        using reference = value_type;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::conditional_t<Groups::ordered, std::bidirectional_iterator_tag, std::forward_iterator_tag>;
        using iterator_category = std::input_iterator_tag;

        /// What -> points at: the pair, kept for the expression's lifetime.
        struct arrow
        {
            value_type pair;
            const value_type* operator->() const { return &pair; }
        };

        iterator() = default;

        reference operator*() const
        {
            const auto at = settled();
            return { m_groups->key(at), m_groups->row(at)[m_index] };
        }
        arrow operator->() const { return { **this }; }
        /// To the next value of its key, else to the next key's place: in constant time, whatever follows.
        iterator& operator++()
        {
            m_group = settled();
            if (++m_index == m_groups->row(m_group).size())
            {
                ++m_group;
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
        /// The previous value: in this group, else the last of the nearest group before with any.
        iterator& operator--()
            requires Groups::ordered
        {
            if (m_index > 0)
                --m_index;
            else
            {
                --m_group;
                while (m_groups->row(m_group).empty())
                    --m_group;
                m_index = m_groups->row(m_group).size() - 1;
            }
            return *this;
        }
        iterator operator--(int)
            requires Groups::ordered
        {
            auto copy = *this;
            --*this;
            return copy;
        }
        /// At the same value, or both at the end: a key's place without values is the next value's.
        friend bool operator==(const iterator& lhs, const iterator& rhs)
        { return lhs.m_index == rhs.m_index && (lhs.m_group == rhs.m_group || (lhs.m_index == 0 && lhs.settled() == rhs.settled())); }

    private:
        friend class GroupedMultiMap;
        iterator(const Groups* groups, group at, std::size_t index) : m_groups(groups), m_group(at), m_index(index) {}

        /// The group of the value it is at: its own, or from a key's place without values, the next with any (the
        /// end, where none has). An iterator is at a value of its group, or at a group's first place.
        [[nodiscard]] group settled() const
        {
            auto at = m_group;
            while (at != m_groups->end() && m_groups->row(at).empty())
                ++at;
            return at;
        }
        /// Moves past groups whose values end at or before the index: to the value there, or to the end.
        iterator& normalized()
        {
            while (m_group != m_groups->end() && m_index >= m_groups->row(m_group).size())
            {
                ++m_group;
                m_index = 0;
            }
            return *this;
        }

        const Groups* m_groups = nullptr;
        group m_group {};
        std::size_t m_index = 0;
    };

private:

    Groups m_groups;

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept { return Members<&GroupedMultiMap::m_groups> {}; }
};

}  // namespace urd::repr

