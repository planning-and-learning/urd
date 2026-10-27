#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

// Containers for keys that are indices 0..n-1 (dense keys): stored by position instead of hashed. They grow to fit
// any key inserted. The registry names them for any element type, so they are unconstrained templates; their
// container_traits declare properties only for indexable keys.

namespace urd::repr
{

/// A set of indices as a bitset: a bit per key below the largest, iterated in key order. Compact and fast to look
/// up; clear and iteration take time in proportion to the largest key, not to the keys present.
template<typename K>
class DenseSet
{
    static_assert(indexable<K>, "urd::repr::DenseSet: keys must be indices (urd::index_of)");

    using word = std::uint64_t;
    static constexpr std::size_t word_bits = std::numeric_limits<word>::digits;
    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

public:
    class iterator;
    using const_iterator = iterator;
    using value_type = K;
    using key_type = K;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    DenseSet() = default;

    template<std::input_iterator InputIt>
    DenseSet(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& key) { this->insert(K(key)); });
    }

    [[nodiscard]] iterator begin() const { return iterator(this, next_from(0)); }
    [[nodiscard]] iterator end() const { return iterator(this, npos); }
    [[nodiscard]] size_type size() const { return m_count; }
    [[nodiscard]] bool empty() const { return m_count == 0; }

    /// Room for keys below `count`.
    void reserve(size_type count) { m_words.reserve((count + word_bits - 1) / word_bits); }

    [[nodiscard]] iterator find(const K& key) const { return contains(key) ? iterator(this, index_of<K>::to_index(key)) : end(); }
    [[nodiscard]] size_type count(const K& key) const { return contains(key) ? 1 : 0; }
    [[nodiscard]] bool contains(const K& key) const { return index_of<K>::is_index(key) && test(index_of<K>::to_index(key)); }
    [[nodiscard]] iterator lower_bound(const K& key) const
    { return iterator(this, index_of<K>::is_index(key) ? next_from(index_of<K>::to_index(key)) : next_from(0)); }
    [[nodiscard]] iterator upper_bound(const K& key) const
    { return iterator(this, index_of<K>::is_index(key) ? next_from(index_of<K>::to_index(key) + 1) : next_from(0)); }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) const { return { lower_bound(key), upper_bound(key) }; }

    std::pair<iterator, bool> insert(const K& key)
    {
        const auto index = index_of<K>::to_index(key);
        if (index / word_bits >= m_words.size())
            m_words.resize(index / word_bits + 1);
        auto& block = m_words[index / word_bits];
        const auto bit = word { 1 } << (index % word_bits);
        const bool inserted = (block & bit) == 0;
        block |= bit;
        m_count += inserted ? 1 : 0;
        return { iterator(this, index), inserted };
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
        reset(index_of<K>::to_index(key));
        return 1;
    }
    iterator erase(const_iterator pos)
    {
        const auto next = next_from(pos.m_index + 1);
        reset(pos.m_index);
        return iterator(this, next);
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        while (first != last)
            first = erase(first);
        return last;
    }
    void clear()
    {
        std::ranges::fill(m_words, word { 0 });
        m_count = 0;
    }

    /// Equal keys; how many words each holds does not matter.
    [[nodiscard]] friend bool operator==(const DenseSet& lhs, const DenseSet& rhs)
    {
        const auto common = std::min(lhs.m_words.size(), rhs.m_words.size());
        const auto zero = [](word block) { return block == 0; };
        return lhs.m_count == rhs.m_count && std::equal(lhs.m_words.begin(), lhs.m_words.begin() + static_cast<difference_type>(common), rhs.m_words.begin())
               && std::all_of(lhs.m_words.begin() + static_cast<difference_type>(common), lhs.m_words.end(), zero)
               && std::all_of(rhs.m_words.begin() + static_cast<difference_type>(common), rhs.m_words.end(), zero);
    }

    /// A key at a time, in key order. Inserting or erasing other keys leaves it valid.
    class iterator
    {
    public:
        using value_type = K;
        using reference = K;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::bidirectional_iterator_tag;
        using iterator_category = std::input_iterator_tag;

        iterator() = default;

        K operator*() const { return index_of<K>::from_index(m_index); }
        iterator& operator++()
        {
            m_index = m_set->next_from(m_index + 1);
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
            m_index = m_set->previous_from(m_index == npos ? m_set->m_words.size() * word_bits : m_index);
            return *this;
        }
        iterator operator--(int)
        {
            auto copy = *this;
            --*this;
            return copy;
        }
        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.m_index == rhs.m_index; }

    private:
        friend class DenseSet;
        iterator(const DenseSet* set, std::size_t index) : m_set(set), m_index(index) {}

        const DenseSet* m_set = nullptr;
        std::size_t m_index = npos;
    };

private:
    [[nodiscard]] bool test(std::size_t index) const
    { return index / word_bits < m_words.size() && (m_words[index / word_bits] >> (index % word_bits) & word { 1 }) != 0; }
    void reset(std::size_t index)
    {
        m_words[index / word_bits] &= ~(word { 1 } << (index % word_bits));
        --m_count;
    }
    /// The first key at or after `index`, else npos.
    [[nodiscard]] std::size_t next_from(std::size_t index) const
    {
        for (auto block = index / word_bits; block < m_words.size(); ++block)
        {
            const auto bits = m_words[block] & (block == index / word_bits ? ~word { 0 } << (index % word_bits) : ~word { 0 });
            if (bits != 0)
                return block * word_bits + static_cast<std::size_t>(std::countr_zero(bits));
        }
        return npos;
    }
    /// The last key before `index`, else npos.
    [[nodiscard]] std::size_t previous_from(std::size_t index) const
    {
        for (auto block = index / word_bits + 1; block-- > 0;)
        {
            if (block >= m_words.size())
                continue;
            const auto keep = index % word_bits;
            const auto bits = m_words[block] & (block == index / word_bits ? (keep == 0 ? word { 0 } : ~word { 0 } >> (word_bits - keep)) : ~word { 0 });
            if (bits != 0)
                return block * word_bits + word_bits - 1 - static_cast<std::size_t>(std::countl_zero(bits));
        }
        return npos;
    }

    std::vector<word> m_words;
    std::size_t m_count = 0;
};

/// A set of indices as a sparse set (Briggs and Torczon): the members in one array, and for each index its place
/// in it. Lookup, insert and erase are O(1); clear is O(1) for members that need no destructor; iteration visits
/// only the members, in no particular order (erase moves the last member into the gap).
template<typename K>
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
        m_members[place] = m_members.back();
        m_places[index_of<K>::to_index(m_members[place])] = place;
        m_members.pop_back();
    }

    std::vector<K> m_members;
    std::vector<std::size_t> m_places;
};

}  // namespace urd::repr

namespace urd
{

/// A bit per key: sorted by index, which urd::Less orders as it does indices; clear zeroes every word.
template<indexable K>
struct container_traits<repr::DenseSet<K>> : properties_of<sorted, unique, dense, canonical, shrinkable, retains>
{
};

/// Members in one array; clear only forgets them, which is O(1) where they need no destructor.
template<indexable K>
struct container_traits<repr::SparseSet<K>> :
    properties_of<unique, dense, contiguous, shrinkable, retains, std::conditional_t<std::is_trivially_destructible_v<K>, constant_clear, proportional_clear>>
{
};

}  // namespace urd
