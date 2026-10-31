#pragma once

// A set of indices as a sparse set (Briggs and Torczon), for dense keys: stored by position, not hashed.

#include "urd/elements.hpp"
#include "urd/repr/core/config.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A set of indices as a sparse set (Briggs and Torczon): the members in one array, and for each index its place
/// in it. Lookup, insert and erase are O(1); clear is O(1) for members that need no destructor; iteration visits
/// only the members, in insertion order until an erase moves the last member into the gap. With KeepOrder, erase
/// shifts the later members instead (O(n)), so the members stay in insertion order.
template<typename K, bool KeepOrder>
class SparseSet
{
    static_assert(indexable<K>, "urd::repr::SparseSet: keys must be indices (urd::index_of)");

public:
    using iterator = typename std::vector<K>::const_iterator;
    using const_iterator = iterator;
    using value_type = K;
    using key_type = K;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    SparseSet() = default;

    template<std::input_iterator InputIt>
    SparseSet(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& key) { this->insert(K(key)); });
    }

    [[nodiscard]] iterator begin() const { return m_members.begin(); }
    [[nodiscard]] iterator end() const { return m_members.end(); }
    [[nodiscard]] size_type size() const { return m_members.size(); }
    [[nodiscard]] bool empty() const { return m_members.empty(); }
    [[nodiscard]] const K* data() const { return m_members.data(); }

    /// Room for keys below `count`.
    void reserve(size_type count)
    {
        m_places.reserve(count);
        m_members.reserve(count);
    }

    [[nodiscard]] iterator find(const K& key) const
    { return contains(key) ? begin() + static_cast<difference_type>(m_places[index_of<K>::to_index(key)]) : end(); }
    [[nodiscard]] size_type count(const K& key) const { return contains(key) ? 1 : 0; }
    [[nodiscard]] bool contains(const K& key) const
    {
        if (!index_of<K>::is_index(key))
            return false;
        const auto index = index_of<K>::to_index(key);
        return index < m_places.size() && m_places[index] < m_members.size() && index_of<K>::to_index(m_members[m_places[index]]) == index;
    }

    std::pair<iterator, bool> insert(const K& key)
    {
        if (contains(key))
            return { find(key), false };
        const auto index = index_of<K>::to_index(key);
        if (index >= m_places.size())
            m_places.resize(index + 1);
        m_places[index] = m_members.size();
        m_members.push_back(key);
        return { end() - 1, true };
    }
    iterator insert(const_iterator /*hint*/, const K& key) { return insert(key).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& key) { this->insert(K(key)); });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(K(std::forward<Args>(args)...)); }

    size_type erase(const K& key)
    {
        if (!contains(key))
            return 0;
        erase_at(m_places[index_of<K>::to_index(key)]);
        return 1;
    }
    /// The member after `pos` takes its place, so the returned iterator is `pos` again.
    iterator erase(const_iterator pos)
    {
        const auto place = static_cast<std::size_t>(pos - begin());
        erase_at(place);
        return begin() + static_cast<difference_type>(place);
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        const auto from = static_cast<std::size_t>(first - begin());
        for (auto place = static_cast<std::size_t>(last - begin()); place > from; --place)
            erase_at(place - 1);
        return begin() + static_cast<difference_type>(from);
    }
    void clear() { m_members.clear(); }

    [[nodiscard]] friend bool operator==(const SparseSet& lhs, const SparseSet& rhs)
    {
        return lhs.size() == rhs.size() && std::ranges::all_of(lhs.m_members, [&](const K& key) { return rhs.contains(key); });
    }

private:
    void erase_at(std::size_t place)
    {
        if constexpr (KeepOrder)
        {
            m_members.erase(m_members.begin() + static_cast<difference_type>(place));
            for (; place < m_members.size(); ++place)
                m_places[index_of<K>::to_index(m_members[place])] = place;
        }
        else
        {
            m_members[place] = m_members.back();
            m_places[index_of<K>::to_index(m_members[place])] = place;
            m_members.pop_back();
        }
    }

    std::vector<K> m_members;
    std::vector<std::size_t> m_places;
};

}  // namespace urd::repr
