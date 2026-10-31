#pragma once

// The grouping of a growing multimap by dense key: a vector of values per index.

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

}  // namespace urd::repr
