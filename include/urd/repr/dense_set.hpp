#pragma once

// A set of indices as a bitset, for dense keys: stored by position, not hashed.

#include "urd/elements.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/core/config.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/repr/sparse_set.hpp"
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

/// A set of indices as a bitset: a bit per key below the largest, iterated in key order. Compact and fast to look
/// up; iteration takes time in proportion to the largest key, not to the keys present, and so does clear, unless
/// TrackWords: then the words that are not zero are kept in a SparseSet of their indices, and clear zeroes only
/// those, at most one per element.
template<typename K, bool TrackWords>
class DenseSet
{
    static_assert(indexable<K>, "urd::repr::DenseSet: keys must be indices (urd::index_of)");

    using word = std::uint64_t;
    using Bits = BitsetSpan<word>;
    using ConstBits = BitsetSpan<const word>;
    static constexpr std::size_t npos = Bits::npos;

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
    void reserve(size_type count) { m_words.reserve(Bits::num_blocks(count)); }

    [[nodiscard]] iterator find(const K& key) const { return contains(key) ? iterator(this, index_of<K>::to_index(key)) : end(); }
    [[nodiscard]] size_type count(const K& key) const { return contains(key) ? 1 : 0; }
    [[nodiscard]] bool contains(const K& key) const
    {
        if (!index_of<K>::is_index(key))
            return false;
        const auto index = index_of<K>::to_index(key);
        return index < bits().size() && bits().test(index);
    }
    [[nodiscard]] iterator lower_bound(const K& key) const
    { return iterator(this, index_of<K>::is_index(key) ? next_from(index_of<K>::to_index(key)) : next_from(0)); }
    [[nodiscard]] iterator upper_bound(const K& key) const
    { return iterator(this, index_of<K>::is_index(key) ? next_from(index_of<K>::to_index(key) + 1) : next_from(0)); }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) const { return { lower_bound(key), upper_bound(key) }; }

    std::pair<iterator, bool> insert(const K& key)
    {
        const auto index = index_of<K>::to_index(key);
        if (index >= bits().size())
            m_words.resize(Bits::num_blocks(index + 1));
        const bool inserted = !bits().test(index);
        if constexpr (TrackWords)
            if (m_words[Bits::block_index(index)] == 0)
                m_touched.insert(Bits::block_index(index));
        bits().set(index);
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
        remove(index_of<K>::to_index(key));
        return 1;
    }
    iterator erase(const_iterator pos)
    {
        const auto next = next_from(pos.m_index + 1);
        remove(pos.m_index);
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
        if constexpr (TrackWords)
        {
            for (const auto block : m_touched)
                m_words[block] = 0;
            m_touched.clear();
        }
        else
            bits().reset();
        m_count = 0;
    }

    // Set algebra on whole words, which the Store's set_algebra uses. Sets grow to their largest key, so the
    // operations apply BitsetSpan to the words both have, and treat the rest of the longer one by itself.

    [[nodiscard]] bool includes(const DenseSet& other) const
    {
        const auto common = std::min(m_words.size(), other.m_words.size());
        return other.words_from(common).none() && other.words(common).is_subset_of(words(common));
    }
    [[nodiscard]] bool intersects(const DenseSet& other) const
    {
        const auto common = std::min(m_words.size(), other.m_words.size());
        return words(common).intersects(other.words(common));
    }
    DenseSet& operator|=(const DenseSet& other)
    {
        if (other.m_words.size() > m_words.size())
            m_words.resize(other.m_words.size());
        words(other.m_words.size()) |= other.words(other.m_words.size());
        return recount();
    }
    DenseSet& operator&=(const DenseSet& other)
    {
        const auto common = std::min(m_words.size(), other.m_words.size());
        words(common) &= other.words(common);
        words_from(common).reset();
        return recount();
    }
    DenseSet& operator-=(const DenseSet& other)
    {
        const auto common = std::min(m_words.size(), other.m_words.size());
        words(common) -= other.words(common);
        return recount();
    }

    /// The words up to the last one with a bit set: equal sets have equal ones, however many words each holds.
    [[nodiscard]] std::span<const word> significant_words() const noexcept
    {
        auto count = m_words.size();
        while (count > 0 && m_words[count - 1] == 0)
            --count;
        return std::span<const word>(m_words.data(), count);
    }

    /// Equal keys; how many words each holds does not matter.
    [[nodiscard]] friend bool operator==(const DenseSet& lhs, const DenseSet& rhs)
    {
        const auto common = std::min(lhs.m_words.size(), rhs.m_words.size());
        return lhs.m_count == rhs.m_count && std::ranges::equal(lhs.words(common).blocks(), rhs.words(common).blocks()) && lhs.words_from(common).none()
               && rhs.words_from(common).none();
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
            m_index = m_set->previous_from(m_index == npos ? m_set->bits().size() : m_index);
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
    [[nodiscard]] Bits bits() { return Bits(m_words.data(), m_words.size() * Bits::Digits); }
    [[nodiscard]] ConstBits bits() const { return ConstBits(m_words.data(), m_words.size() * Bits::Digits); }
    /// The first `count` words, as a bitset.
    [[nodiscard]] Bits words(std::size_t count) { return Bits(m_words.data(), count * Bits::Digits); }
    [[nodiscard]] ConstBits words(std::size_t count) const { return ConstBits(m_words.data(), count * Bits::Digits); }
    /// The words from `first` on, as a bitset.
    [[nodiscard]] Bits words_from(std::size_t first) { return Bits(std::span<word>(m_words).subspan(first).data(), (m_words.size() - first) * Bits::Digits); }
    [[nodiscard]] ConstBits words_from(std::size_t first) const
    { return ConstBits(std::span<const word>(m_words).subspan(first).data(), (m_words.size() - first) * Bits::Digits); }

    DenseSet& recount()
    {
        m_count = bits().count();
        if constexpr (TrackWords)
        {
            // Set algebra wrote whole words: the words to clear are those not zero now.
            m_touched.clear();
            for (std::size_t block = 0; block < m_words.size(); ++block)
                if (m_words[block] != 0)
                    m_touched.insert(block);
        }
        return *this;
    }
    void remove(std::size_t index)
    {
        bits().reset(index);
        --m_count;
        if constexpr (TrackWords)
            if (m_words[Bits::block_index(index)] == 0)
                m_touched.erase(Bits::block_index(index));
    }
    /// The first key at or after `index`, else npos.
    [[nodiscard]] std::size_t next_from(std::size_t index) const
    {
        if (index >= bits().size())
            return npos;
        return bits().test(index) ? index : bits().find_next(index);
    }
    /// The last key before `index`, else npos. BitsetSpan searches forward only, so this one goes by words.
    [[nodiscard]] std::size_t previous_from(std::size_t index) const
    {
        for (auto block = std::min(Bits::block_index(index) + 1, m_words.size()); block-- > 0;)
        {
            const auto keep = block == Bits::block_index(index) ? Bits::block_pos(index) : Bits::Digits;
            const auto below = keep == Bits::Digits ? m_words[block] : m_words[block] & ((word { 1 } << keep) - 1);
            if (below != 0)
                return block * Bits::Digits + Bits::Digits - 1 - static_cast<std::size_t>(std::countl_zero(below));
        }
        return npos;
    }

    struct NoWords
    {
    };

    std::vector<word> m_words;
    std::size_t m_count = 0;
    /// The words that are not zero, where TrackWords.
    [[no_unique_address]] std::conditional_t<TrackWords, SparseSet<std::size_t>, NoWords> m_touched;
};

}  // namespace urd::repr
