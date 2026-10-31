#pragma once

// The grouping of a frozen multimap by dense key: compressed sparse rows.

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
    { assign(first, last); }

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
    { return lhs.m_offsets == rhs.m_offsets && std::ranges::equal(lhs.m_values, rhs.m_values, EqualTo<V> {}); }

private:
    std::vector<std::size_t> m_offsets { 0 };
    std::vector<V> m_values;
};

}  // namespace urd::repr
