#pragma once

#include "urd/elements.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A frozen multimap from indices to values, as compressed sparse rows: the values of all keys in one array, in key
/// order, with an offset per key where its values start. Built at once from pairs in any order, by a counting sort
/// that keeps the order of equal keys. The registry names it for any key type, so it is an unconstrained template;
/// its container_traits declare properties only for indexable keys.
template<typename K, typename V>
class CsrMultiMap
{
    static_assert(indexable<K>, "urd::repr::CsrMultiMap: keys must be indices (urd::index_of)");

public:
    class iterator;
    using const_iterator = iterator;
    using key_type = K;
    using mapped_type = V;
    /// What iterating gives: a key and a view of one of its values.
    using value_type = std::pair<const K, const V&>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    CsrMultiMap() = default;

    /// From pairs in any order (std::pair<const K, V>, or what iterating another map gives).
    template<std::input_iterator InputIt>
    CsrMultiMap(InputIt first, InputIt last)
    {
        auto pairs = std::vector<std::pair<std::size_t, V>>();
        std::for_each(first, last, [&pairs](const auto& pair) { pairs.emplace_back(index_of<K>::to_index(K(pair.first)), V(pair.second)); });
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
        m_values.reserve(pairs.size());
        for (const auto each : order)
            m_values.push_back(std::move(pairs[each].second));
    }

    [[nodiscard]] iterator begin() const { return iterator(this, 0, 0).normalized(); }
    [[nodiscard]] iterator end() const { return iterator(this, keys(), m_values.size()); }
    [[nodiscard]] size_type size() const { return m_values.size(); }
    [[nodiscard]] bool empty() const { return m_values.empty(); }

    /// The values of `key`, in the order they were given.
    [[nodiscard]] std::span<const V> values(const K& key) const
    {
        if (!index_of<K>::is_index(key) || index_of<K>::to_index(key) >= keys())
            return {};
        const auto index = index_of<K>::to_index(key);
        return std::span<const V>(m_values).subspan(m_offsets[index], m_offsets[index + 1] - m_offsets[index]);
    }

    [[nodiscard]] iterator find(const K& key) const { return contains(key) ? lower_bound(key) : end(); }
    [[nodiscard]] size_type count(const K& key) const { return values(key).size(); }
    [[nodiscard]] bool contains(const K& key) const { return !values(key).empty(); }
    [[nodiscard]] iterator lower_bound(const K& key) const
    {
        if (!index_of<K>::is_index(key))
            return begin();
        return from_key(index_of<K>::to_index(key));
    }
    [[nodiscard]] iterator upper_bound(const K& key) const
    {
        if (!index_of<K>::is_index(key))
            return begin();
        return from_key(index_of<K>::to_index(key) + 1);
    }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) const { return { lower_bound(key), upper_bound(key) }; }

    /// Equal keys with equal values in the same order, by urd::EqualTo.
    [[nodiscard]] friend bool operator==(const CsrMultiMap& lhs, const CsrMultiMap& rhs)
    { return lhs.m_offsets == rhs.m_offsets && std::ranges::equal(lhs.m_values, rhs.m_values, EqualTo<V> {}); }

    /// A value at a time, in key order, with its key.
    class iterator
    {
    public:
        using value_type = typename CsrMultiMap::value_type;
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

        reference operator*() const { return { index_of<K>::from_index(m_key), m_map->m_values[m_position] }; }
        arrow operator->() const { return { **this }; }
        iterator& operator++()
        {
            ++m_position;
            return normalized();
        }
        iterator operator++(int)
        {
            auto copy = *this;
            ++*this;
            return copy;
        }
        /// The previous value's key is the last whose values start at or before it.
        iterator& operator--()
        {
            --m_position;
            while (m_map->m_offsets[m_key] > m_position)
                --m_key;
            return *this;
        }
        iterator operator--(int)
        {
            auto copy = *this;
            --*this;
            return copy;
        }
        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.m_position == rhs.m_position; }

    private:
        friend class CsrMultiMap;
        iterator(const CsrMultiMap* map, std::size_t key, std::size_t position) : m_map(map), m_key(key), m_position(position) {}

        /// Moves the key past keys whose values end at or before the position: to the key of the value there.
        iterator& normalized()
        {
            while (m_key < m_map->keys() && m_position >= m_map->m_offsets[m_key + 1])
                ++m_key;
            return *this;
        }

        const CsrMultiMap* m_map = nullptr;
        std::size_t m_key = 0;
        std::size_t m_position = 0;
    };

private:
    [[nodiscard]] std::size_t keys() const { return m_offsets.size() - 1; }
    [[nodiscard]] iterator from_key(std::size_t index) const { return index >= keys() ? end() : iterator(this, index, m_offsets[index]).normalized(); }

    std::vector<std::size_t> m_offsets { 0 };
    std::vector<V> m_values;
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

/// Values in key order, each key's in the order given; built once, never changed.
template<indexable K, typename V>
struct container_traits<repr::CsrMultiMap<K, V>> : properties_of<sorted, multi, dense, canonical>
{
};

/// As a CsrMultiMap, answering by value too.
template<indexable K, indexable V>
struct container_traits<repr::BidirectionalCsr<K, V>> : with_t<container_traits<repr::CsrMultiMap<K, V>>, bidirectional>
{
};

}  // namespace urd
