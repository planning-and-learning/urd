#pragma once

#include "urd/elements.hpp"
#include "urd/repr/grouped_fwd.hpp"
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
    {
        m_groups.assign(first, last);
    }

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
            return { lower_bound(key), upper_bound(key) };
        if (!contains(key))
            return { end(), end() };
        const auto found = m_groups.find(key);
        auto after = found;
        return { iterator(&m_groups, found, 0), iterator(&m_groups, ++after, 0).normalized() };
    }
    [[nodiscard]] iterator lower_bound(const K& key) const
        requires Groups::ordered
    {
        return iterator(&m_groups, m_groups.lower_bound(key), 0).normalized();
    }
    [[nodiscard]] iterator upper_bound(const K& key) const
        requires Groups::ordered
    {
        return iterator(&m_groups, m_groups.upper_bound(key), 0).normalized();
    }

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
    {
        return insert(pair);
    }
    template<std::input_iterator InputIt>
        requires Groups::changes
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->insert(pair); });
    }
    template<typename... Args>
        requires Groups::changes
    iterator emplace(Args&&... args)
    {
        return insert(std::pair<K, V>(std::forward<Args>(args)...));
    }

    size_type erase(const K& key)
        requires Groups::changes
    {
        return m_groups.erase(key);
    }
    /// The values after `pos` move up within its key; where it was the key's last, the key leaves.
    iterator erase(const_iterator pos)
        requires Groups::changes
    {
        const auto [at, index] = m_groups.erase(pos.m_group, pos.m_index);
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
    {
        m_groups.clear();
    }

    /// Where Groups compares; else a Store compares by iterating.
    [[nodiscard]] friend bool operator==(const GroupedMultiMap& lhs, const GroupedMultiMap& rhs)
        requires std::equality_comparable<Groups>
    {
        return lhs.m_groups == rhs.m_groups;
    }

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

        reference operator*() const { return { m_groups->key(m_group), m_groups->row(m_group)[m_index] }; }
        arrow operator->() const { return { **this }; }
        iterator& operator++()
        {
            ++m_index;
            return normalized();
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
        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.m_group == rhs.m_group && lhs.m_index == rhs.m_index; }

    private:
        friend class GroupedMultiMap;
        iterator(const Groups* groups, group at, std::size_t index) : m_groups(groups), m_group(at), m_index(index) {}

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
};

/// Groups by key index, as compressed sparse rows: the values of all keys in one array, in key order, with an offset
/// per key where its values start; a group is a key's index. Built at once from pairs in any order, by a counting
/// sort that keeps the order of equal keys; never changed.
template<typename K, typename V>
class IndexGroups
{
    static_assert(indexable<K>, "urd::repr::IndexGroups: keys must be indices (urd::index_of)");

public:
    using key_type = K;
    using mapped_type = V;
    using group = std::size_t;
    static constexpr bool ordered = true;
    static constexpr bool changes = false;

    IndexGroups() = default;

    template<std::input_iterator InputIt>
    IndexGroups(InputIt first, InputIt last)
    {
        assign(first, last);
    }

    /// Rebuilds from pairs. From a range read twice, with values that can be default-built, in the storage already
    /// held: count each key's values, sum the counts to offsets, place each value at its key's next offset, then
    /// shift the offsets back. Otherwise through a copy of the pairs.
    template<std::input_iterator InputIt>
    void assign(InputIt first, InputIt last)
    {
        const auto index = [](const auto& pair) { return index_of<K>::to_index(K(pair.first)); };
        if constexpr (std::forward_iterator<InputIt> && std::default_initializable<V>)
        {
            m_offsets.assign(1, 0);
            std::for_each(first,
                          last,
                          [this, &index](const auto& pair)
                          {
                              const auto at = index(pair);
                              if (at + 2 > m_offsets.size())
                                  m_offsets.resize(at + 2, 0);
                              ++m_offsets[at + 1];
                          });
            std::partial_sum(m_offsets.begin(), m_offsets.end(), m_offsets.begin());
            m_values.resize(m_offsets.back());
            std::for_each(first, last, [this, &index](const auto& pair) { m_values[m_offsets[index(pair)]++] = V(pair.second); });
            std::shift_right(m_offsets.begin(), m_offsets.end(), 1);
            m_offsets.front() = 0;
        }
        else
        {
            auto pairs = std::vector<std::pair<std::size_t, V>>();
            std::for_each(first, last, [&pairs, &index](const auto& pair) { pairs.emplace_back(index(pair), V(pair.second)); });
            auto keys = std::size_t { 0 };
            for (const auto& pair : pairs)
                keys = std::max(keys, pair.first + 1);
            m_offsets.assign(keys + 1, 0);
            for (const auto& pair : pairs)
                ++m_offsets[pair.first + 1];
            std::partial_sum(m_offsets.begin(), m_offsets.end(), m_offsets.begin());
            // Where each pair goes; then the values in that order.
            auto next = std::vector<std::size_t>(m_offsets.begin(), m_offsets.end() - 1);
            auto order = std::vector<std::size_t>(pairs.size());
            for (std::size_t each = 0; each < pairs.size(); ++each)
                order[next[pairs[each].first]++] = each;
            m_values.clear();
            m_values.reserve(pairs.size());
            for (const auto each : order)
                m_values.push_back(std::move(pairs[each].second));
        }
    }

    [[nodiscard]] group begin() const { return 0; }
    [[nodiscard]] group end() const { return m_offsets.size() - 1; }
    [[nodiscard]] std::size_t count() const { return m_values.size(); }
    [[nodiscard]] K key(group at) const { return index_of<K>::from_index(at); }
    [[nodiscard]] std::span<const V> row(group at) const { return std::span<const V>(m_values).subspan(m_offsets[at], m_offsets[at + 1] - m_offsets[at]); }
    [[nodiscard]] group find(const K& key) const { return index_of<K>::is_index(key) ? std::min(index_of<K>::to_index(key), end()) : end(); }
    /// Keys that are no index (a negative integer) come before all.
    [[nodiscard]] group lower_bound(const K& key) const { return index_of<K>::is_index(key) ? find(key) : begin(); }
    [[nodiscard]] group upper_bound(const K& key) const { return index_of<K>::is_index(key) ? std::min(index_of<K>::to_index(key) + 1, end()) : begin(); }

    /// Equal keys with equal values in the same order, by urd::EqualTo.
    [[nodiscard]] friend bool operator==(const IndexGroups& lhs, const IndexGroups& rhs)
    {
        return lhs.m_offsets == rhs.m_offsets && std::ranges::equal(lhs.m_values, rhs.m_values, EqualTo<V> {});
    }

private:
    std::vector<std::size_t> m_offsets { 0 };
    std::vector<V> m_values;
};

/// Groups by key index that grow: a vector of values per index, the rows growing as keys arrive; a group is a key's
/// index. Append grows a key's row; erasing a key clears its row and keeps it (the storage is retained); erasing a
/// value moves the ones after it up within its key. Iterated in key order, the empty rows skipped.
template<typename K, typename V>
class DenseGroups
{
    static_assert(indexable<K>, "urd::repr::DenseGroups: keys must be indices (urd::index_of)");

public:
    using key_type = K;
    using mapped_type = V;
    using group = std::size_t;
    static constexpr bool ordered = true;
    static constexpr bool changes = true;

    DenseGroups() = default;

    template<std::input_iterator InputIt>
    DenseGroups(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->append(K(pair.first), V(pair.second)); });
    }

    [[nodiscard]] group begin() const { return 0; }
    [[nodiscard]] group end() const { return m_rows.size(); }
    [[nodiscard]] std::size_t count() const { return m_count; }
    [[nodiscard]] K key(group at) const { return index_of<K>::from_index(at); }
    [[nodiscard]] std::span<const V> row(group at) const { return m_rows[at]; }
    [[nodiscard]] group find(const K& key) const { return index_of<K>::is_index(key) ? std::min(index_of<K>::to_index(key), end()) : end(); }
    /// Keys that are no index (a negative integer) come before all.
    [[nodiscard]] group lower_bound(const K& key) const { return index_of<K>::is_index(key) ? find(key) : begin(); }
    [[nodiscard]] group upper_bound(const K& key) const { return index_of<K>::is_index(key) ? std::min(index_of<K>::to_index(key) + 1, end()) : begin(); }

    /// The group of `key` and the position of the value appended to it; the rows grow to the key.
    std::pair<group, std::size_t> append(const K& key, V value)
    {
        const auto at = index_of<K>::to_index(key);
        if (at >= m_rows.size())
            m_rows.resize(at + 1);
        m_rows[at].push_back(std::move(value));
        ++m_count;
        return { at, m_rows[at].size() - 1 };
    }
    /// Clears the key's row, keeping its storage.
    std::size_t erase(const K& key)
    {
        const auto at = find(key);
        if (at == end())
            return 0;
        const auto erased = m_rows[at].size();
        m_rows[at].clear();
        m_count -= erased;
        return erased;
    }
    /// Where the value after the erased one is: the same place, which the caller normalizes past an emptied row.
    std::pair<group, std::size_t> erase(group at, std::size_t index)
    {
        auto& values = m_rows[at];
        values.erase(values.begin() + static_cast<std::ptrdiff_t>(index));
        --m_count;
        return { at, index };
    }
    /// Clears every row, keeping the rows and their storage.
    void clear()
    {
        for (auto& values : m_rows)
            values.clear();
        m_count = 0;
    }

    /// Equal keys with equal values in the same order, by urd::EqualTo; rows beyond the other's are empty.
    [[nodiscard]] friend bool operator==(const DenseGroups& lhs, const DenseGroups& rhs)
    {
        if (lhs.m_count != rhs.m_count)
            return false;
        for (group at = 0; at < std::max(lhs.end(), rhs.end()); ++at)
        {
            const auto left = at < lhs.end() ? lhs.row(at) : std::span<const V>();
            const auto right = at < rhs.end() ? rhs.row(at) : std::span<const V>();
            if (!std::ranges::equal(left, right, EqualTo<V> {}))
                return false;
        }
        return true;
    }

private:
    std::vector<std::vector<V>> m_rows;
    std::size_t m_count = 0;
};

/// A CsrMultiMap that also keeps the inverse, values to keys, as compressed sparse rows: a frozen graph with its
/// in-edges. Its values must be indices too.
template<typename K, typename V>
class BidirectionalCsr : public CsrMultiMap<K, V>
{
public:
    BidirectionalCsr() = default;

    template<std::input_iterator InputIt>
    BidirectionalCsr(InputIt first, InputIt last) : BidirectionalCsr(std::vector<std::pair<K, V>>(first, last))
    {
    }

    /// The keys that map to `value`, in the order their pairs were given.
    [[nodiscard]] std::span<const K> keys(const V& value) const { return m_inverse.values(value); }

private:
    explicit BidirectionalCsr(const std::vector<std::pair<K, V>>& pairs) : CsrMultiMap<K, V>(pairs.begin(), pairs.end()), m_inverse(inverted(pairs)) {}
    [[nodiscard]] static CsrMultiMap<V, K> inverted(const std::vector<std::pair<K, V>>& pairs)
    {
        auto swapped = std::vector<std::pair<V, K>>();
        swapped.reserve(pairs.size());
        for (const auto& [key, value] : pairs)
            swapped.emplace_back(value, key);
        return CsrMultiMap<V, K>(swapped.begin(), swapped.end());
    }

    CsrMultiMap<V, K> m_inverse;
};

}  // namespace urd::repr

namespace urd
{

}  // namespace urd
