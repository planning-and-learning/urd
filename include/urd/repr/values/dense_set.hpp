#pragma once

// A set of indices as a bitset, for dense keys: stored by position, not hashed.

#include "urd/config.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/key_range.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/bit.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/containers/detail/tracking.hpp"
#include "urd/repr/values/dense_set_fwd.hpp"
#include "urd/repr/values/sparse_set.hpp"
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

namespace urd::detail
{
struct dense_words;
}  // namespace urd::detail

namespace urd::repr
{

/// A set of indices as a bitset: a bit per key below the largest, iterated in key order. Compact and fast to look
/// up; iteration takes time in proportion to the largest key, not to the keys present, and so does clear, unless
/// TrackWords: then the words that are not zero are kept in a SparseSet of their indices, and clear zeroes only
/// those, at most one per element. Built over a window of keys (urd::key_range), its bits begin at the window's
/// first key: a key below it does not fit, and sets of keys in a window cost the window's words. Sets over different
/// windows combine word by word, shifted.
template<typename K, bool TrackWords>
class DenseSet
{
    static_assert(indexable<K>, "urd::repr::DenseSet: keys must be indices (urd::index_of)");

    using word = urd::word_t;
    using Bits = BitsetSpan<word>;
    using ConstBits = BitsetSpan<const word>;
    static constexpr std::size_t npos = Bits::npos;

public:
    class iterator;
    using const_iterator = iterator;
    using value_type = K;
    using reference = K;  ///< a key is read from its bit
    using const_reference = K;
    using key_type = K;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    DenseSet() = default;
    /// Keys from `range.first` on, with room for those below `range.last`.
    explicit DenseSet(const key_range<K>& range) : m_first(index_of<K>::to_index(range.first)) { reserve(index_of<K>::to_index(range.last)); }

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
    void reserve(size_type count) { m_words.reserve(Bits::num_blocks(count > m_first ? count - m_first : 0)); }

    [[nodiscard]] iterator find(const K& key) const { return contains(key) ? iterator(this, index_of<K>::to_index(key) - m_first) : end(); }
    [[nodiscard]] size_type count(const K& key) const { return contains(key) ? 1 : 0; }
    [[nodiscard]] bool contains(const K& key) const
    {
        if (!index_of<K>::is_index(key) || index_of<K>::to_index(key) < m_first)
            return false;
        const auto index = index_of<K>::to_index(key) - m_first;
        return index < bits().size() && bits().test(index);
    }
    [[nodiscard]] iterator lower_bound(const K& key) const { return iterator(this, next_from(position_from(key, 0))); }
    [[nodiscard]] iterator upper_bound(const K& key) const { return iterator(this, next_from(position_from(key, 1))); }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) const { return { lower_bound(key), upper_bound(key) }; }

    /// Inserts `key`; a key below the window's first throws std::out_of_range.
    std::pair<iterator, bool> insert(const K& key)
    {
        if (index_of<K>::to_index(key) < m_first)
            throw std::out_of_range("urd: a dense set built over a window of keys holds none below its first");
        const auto index = index_of<K>::to_index(key) - m_first;
        if (index >= bits().size())
            m_words.resize(Bits::num_blocks(index + 1));
        const bool inserted = !bits().test(index);
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
        remove(index_of<K>::to_index(key) - m_first);
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
        m_touched.reset(
            m_words.size(),
            [this](std::size_t first, std::size_t last)
            { std::fill(m_words.begin() + static_cast<difference_type>(first), m_words.begin() + static_cast<difference_type>(last), word { 0 }); });
        m_count = 0;
    }

    // Set algebra on whole words, which the Store's set_algebra uses. Sets grow to their largest key, so the
    // operations combine the words both have, and treat the rest of the longer one by itself; each word through
    // put_word, which counts only the bits that change. Sets over different windows combine a word at a time, the
    // other's words shifted into this one's (window).

    [[nodiscard]] bool includes(const DenseSet& other) const
    {
        if (m_first != other.m_first)
            return count_common(other.m_words, other.m_first, other.bits().size()) == other.m_count;
        const auto common = std::min(m_words.size(), other.m_words.size());
        return other.words_from(common).none() && other.words(common).is_subset_of(words(common));
    }
    [[nodiscard]] bool intersects(const DenseSet& other) const
    {
        if (m_first != other.m_first)
            return count_common(other.m_words, other.m_first, other.bits().size()) != 0;
        const auto common = std::min(m_words.size(), other.m_words.size());
        return words(common).intersects(other.words(common));
    }
    DenseSet& operator|=(const DenseSet& other)
    {
        if (m_first != other.m_first)
            return unite(other.m_words, other.m_first, other.bits().size());
        if (other.m_words.size() > m_words.size())
            m_words.resize(other.m_words.size());
        for (std::size_t at = 0; at < other.m_words.size(); ++at)
            put_word(at, m_words[at] | other.m_words[at]);
        return *this;
    }
    DenseSet& operator&=(const DenseSet& other)
    {
        if (m_first != other.m_first)
            return intersect(other.m_words, other.m_first, other.bits().size());
        for (std::size_t at = 0; at < m_words.size(); ++at)
            put_word(at, at < other.m_words.size() ? m_words[at] & other.m_words[at] : word { 0 });
        return *this;
    }
    DenseSet& operator-=(const DenseSet& other)
    {
        if (m_first != other.m_first)
            return subtract(other.m_words, other.m_first, other.bits().size());
        for (std::size_t at = 0; at < std::min(m_words.size(), other.m_words.size()); ++at)
            put_word(at, m_words[at] & ~other.m_words[at]);
        return *this;
    }

    /// As std::set::merge: moves into this set each element of `source` it lacks; `source` keeps the others.
    void merge(DenseSet& source)
    {
        auto moved = source;
        moved -= *this;
        *this |= moved;
        source -= moved;
    }

    /// The words up to the last one with a bit set: equal sets have equal ones, however many words each holds. Key k
    /// is at bit k % word_bits of word k / word_bits: a set over a window whose first key is not 0 has none such
    /// (std::logic_error).
    [[nodiscard]] std::span<const word> significant_words() const
    {
        check_from_zero();
        auto count = m_words.size();
        while (count > 0 && m_words[count - 1] == 0)
            --count;
        return std::span<const word>(m_words.data(), count);
    }

    /// Makes this the set whose bits `words` set (key k at bit k % word_bits of word k / word_bits), its storage reused.
    void assign_words(std::span<const word> words)
    {
        check_from_zero();
        m_words.assign(words.begin(), words.end());
        recount();
    }
    /// Makes this the set whose bits fill(words) sets in `count` words, zeroed first, written in place: its storage
    /// reused, nothing copied.
    template<typename Fill>
        requires std::invocable<Fill&, std::span<word>>
    void assign_words(std::size_t count, Fill fill)
    {
        check_from_zero();
        m_words.assign(count, word { 0 });
        fill(std::span<word>(m_words));
        recount();
    }

    /// Equal keys; how many words each holds does not matter.
    [[nodiscard]] friend bool operator==(const DenseSet& lhs, const DenseSet& rhs)
    {
        if (lhs.m_first != rhs.m_first)
            return lhs.m_count == rhs.m_count && lhs.includes(rhs);
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

        K operator*() const { return index_of<K>::from_index(m_set->m_first + m_index); }
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
    // The same with a bitset given as words (`bits` bits, bit j for key `first` + j), which a graph of rows of bits
    // combines with sets of vertices (urd/graphs.hpp), through urd::detail::dense_words only: each costs the words of
    // this set it touches, a word at a time.
    friend struct urd::detail::dense_words;

    /// Makes this the set of the given bitset's keys (none may be below the window's first).
    DenseSet& assign(std::span<const word> given, std::size_t first, std::size_t bits)
    {
        clear();
        return unite(given, first, bits);
    }
    /// Adds the given bitset's keys; one below the window's first throws std::out_of_range.
    DenseSet& unite(std::span<const word> given, std::size_t first, std::size_t bits)
    {
        if (first < m_first && has_any(given, std::min(bits, m_first - first)))
            throw std::out_of_range("urd: a dense set built over a window of keys holds none below its first");
        const auto end = first + bits;
        if (end > m_first && Bits::num_blocks(end - m_first) > m_words.size())
            m_words.resize(Bits::num_blocks(end - m_first), word { 0 });
        for (auto at = first_word_of(first); at < last_word_of(first, bits); ++at)
            put_word(at, m_words[at] | window(given, first, bits, at));
        return *this;
    }
    /// Makes this the keys of `from` the given bitset has: in one pass where both sets share a window, else from a copy.
    DenseSet& assign_common(const DenseSet& from, std::span<const word> given, std::size_t first, std::size_t bits)
    {
        if (this == &from)
            return intersect(given, first, bits);
        if (m_first != from.m_first)
        {
            *this = from;
            return intersect(given, first, bits);
        }
        return assign_each(from.m_words.size(), [&](std::size_t at) { return from.m_words[at] & window(given, first, bits, at); });
    }
    /// Makes this the keys `lhs` and `rhs` share: in one pass where all three share a window, else from a copy.
    DenseSet& assign_intersection(const DenseSet& lhs, const DenseSet& rhs)
    {
        if (this == &lhs)
            return *this &= rhs;
        if (this == &rhs)
            return *this &= lhs;
        if (m_first != lhs.m_first || m_first != rhs.m_first)
        {
            *this = lhs;
            return *this &= rhs;
        }
        return assign_each(std::min(lhs.m_words.size(), rhs.m_words.size()), [&](std::size_t at) { return lhs.m_words[at] & rhs.m_words[at]; });
    }
    /// Makes word `at` word(at), for `count` words: each written once and counted once, whatever it held.
    template<typename Word>
    DenseSet& assign_each(std::size_t count, Word word_at)
    {
        if (m_words.size() != count)
            m_words.resize(count);
        m_count = 0;
        m_touched.clear();
        for (std::size_t at = 0; at < count; ++at)
        {
            const auto value = word_at(at);
            m_words[at] = value;
            if (value != 0)
            {
                m_count += static_cast<std::size_t>(std::popcount(value));
                m_touched.insert(at);
            }
        }
        return *this;
    }
    /// Keeps the keys the given bitset has.
    DenseSet& intersect(std::span<const word> given, std::size_t first, std::size_t bits)
    {
        for (std::size_t at = 0; at < m_words.size(); ++at)
            put_word(at, m_words[at] & window(given, first, bits, at));
        return *this;
    }
    /// Keeps, of its keys from `first` to before `first` + `bits`, those the given bitset has; the others stay.
    DenseSet& intersect_within(std::span<const word> given, std::size_t first, std::size_t bits)
    {
        for (auto at = first_word_of(first); at < last_word_of(first, bits); ++at)
            put_word(at, m_words[at] & (window(given, first, bits, at) | ~range_mask(first, bits, at)));
        return *this;
    }
    /// Removes the keys the given bitset has.
    DenseSet& subtract(std::span<const word> given, std::size_t first, std::size_t bits)
    {
        for (auto at = first_word_of(first); at < last_word_of(first, bits); ++at)
            put_word(at, m_words[at] & ~window(given, first, bits, at));
        return *this;
    }
    /// How many keys this set and the given bitset share.
    [[nodiscard]] std::size_t count_common(std::span<const word> given, std::size_t first, std::size_t bits) const
    {
        std::size_t result = 0;
        for (auto at = first_word_of(first); at < last_word_of(first, bits); ++at)
            result += static_cast<std::size_t>(std::popcount(m_words[at] & window(given, first, bits, at)));
        return result;
    }
    /// Calls f on each key this set and the given bitset share, in key order.
    template<typename F>
    void for_each_common(std::span<const word> given, std::size_t first, std::size_t bits, F f) const
    {
        for (auto at = first_word_of(first); at < last_word_of(first, bits); ++at)
            bit::for_each_set_bit(m_words[at] & window(given, first, bits, at),
                                  [&](std::size_t j) { f(index_of<K>::from_index(m_first + (at * Bits::Digits) + j)); });
    }

    [[nodiscard]] Bits bits() { return Bits(m_words.data(), m_words.size() * Bits::Digits); }
    [[nodiscard]] ConstBits bits() const { return ConstBits(m_words.data(), m_words.size() * Bits::Digits); }
    /// The first `count` words, as a bitset.
    [[nodiscard]] Bits words(std::size_t count) { return Bits(m_words.data(), count * Bits::Digits); }
    [[nodiscard]] ConstBits words(std::size_t count) const { return ConstBits(m_words.data(), count * Bits::Digits); }
    /// The words from `first` on, as a bitset.
    [[nodiscard]] Bits words_from(std::size_t first) { return Bits(std::span<word>(m_words).subspan(first).data(), (m_words.size() - first) * Bits::Digits); }
    [[nodiscard]] ConstBits words_from(std::size_t first) const
    { return ConstBits(std::span<const word>(m_words).subspan(first).data(), (m_words.size() - first) * Bits::Digits); }

    /// The position of the first key at or after `key` plus `past`: 0 for a key below the window.
    [[nodiscard]] std::size_t position_from(const K& key, std::size_t past) const
    {
        if (!index_of<K>::is_index(key) || index_of<K>::to_index(key) < m_first)
            return 0;
        return index_of<K>::to_index(key) - m_first + past;
    }
    /// Whether any of the first `bits` bits of `given` is set.
    [[nodiscard]] static bool has_any(std::span<const word> given, std::size_t bits)
    {
        for (std::size_t at = 0; at < Bits::num_blocks(bits) && at < given.size(); ++at)
        {
            const auto room = bits - (at * Bits::Digits);
            if ((room < Bits::Digits ? given[at] & bit::lo_set<word>[room] : given[at]) != 0)
                return true;
        }
        return false;
    }
    void check_from_zero() const
    {
        if (m_first != 0)
            throw std::logic_error("urd: a dense set built over a window of keys keeps no words from key 0");
    }
    /// The word of this set that holds key `key`, or 0 where it is below the window; and one past the last word that
    /// holds a key below `first` + `bits`, at most its words.
    [[nodiscard]] std::size_t first_word_of(std::size_t key) const { return key > m_first ? Bits::block_index(key - m_first) : 0; }
    [[nodiscard]] std::size_t last_word_of(std::size_t first, std::size_t bits) const
    {
        const auto end = first + bits;
        return end > m_first ? std::min(m_words.size(), Bits::num_blocks(end - m_first)) : 0;
    }
    /// The bits of `given` (`bits` of them, bit j for key `first` + j) for the keys of this set's word `at`: bit j for key
    /// m_first + at * Digits + j, 0 where `given` has none.
    [[nodiscard]] word window(std::span<const word> given, std::size_t first, std::size_t bits, std::size_t at) const
    {
        if (first == m_first)  // the same window: word for word
        {
            if (at >= given.size() || at * Bits::Digits >= bits)
                return 0;
            const auto room = bits - (at * Bits::Digits);
            return room < Bits::Digits ? given[at] & bit::lo_set<word>[room] : given[at];
        }
        constexpr auto digits = static_cast<std::ptrdiff_t>(Bits::Digits);
        const auto from = static_cast<std::ptrdiff_t>(m_first + (at * Bits::Digits)) - static_cast<std::ptrdiff_t>(first);
        const auto source = [&given](std::ptrdiff_t i)
        { return i >= 0 && static_cast<std::size_t>(i) < given.size() ? given[static_cast<std::size_t>(i)] : word { 0 }; };
        auto result = word { 0 };
        if (from >= 0)
        {
            const auto shift = from % digits;
            result = source(from / digits) >> shift;
            if (shift != 0)
                result |= source((from / digits) + 1) << (digits - shift);
        }
        else if (-from < digits)
            result = source(0) << -from;
        const auto room = static_cast<std::ptrdiff_t>(bits) - from;
        if (room <= 0)
            return 0;
        return room < digits ? result & bit::lo_set<word>[static_cast<std::size_t>(room)] : result;
    }
    /// The bits of word `at` whose keys lie from `first` to before `first` + `bits`.
    [[nodiscard]] word range_mask(std::size_t first, std::size_t bits, std::size_t at) const
    {
        const auto low = m_first + (at * Bits::Digits);
        const auto begin = std::max(low, first);
        const auto end = std::min(low + Bits::Digits, first + bits);
        if (begin >= end)
            return 0;
        const auto upto = end - low == Bits::Digits ? ~word { 0 } : bit::lo_set<word>[end - low];
        return upto & ~bit::lo_set<word>[begin - low];
    }
    /// Sets word `at`, keeping the count and the words not zero.
    void put_word(std::size_t at, word value)
    {
        const auto old = m_words[at];
        if (old == value)
            return;
        // Only the bits that change are counted: an intersection loses some, a union gains some, most words keep all.
        if (const auto gained = value & ~old; gained != 0)
            m_count += static_cast<std::size_t>(std::popcount(gained));
        if (const auto lost = old & ~value; lost != 0)
            m_count -= static_cast<std::size_t>(std::popcount(lost));
        if (old == 0 && value != 0)
            m_touched.insert(at);
        else if (old != 0 && value == 0)
            m_touched.erase(at);
        m_words[at] = value;
    }

    DenseSet& recount()
    {
        m_count = bits().count();
        // Set algebra wrote whole words: the words to clear are those not zero now.
        m_touched.clear();
        for (std::size_t block = 0; block < m_words.size(); ++block)
            if (m_words[block] != 0)
                m_touched.insert(block);
        return *this;
    }
    void remove(std::size_t index)
    {
        bits().reset(index);
        --m_count;
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
            const auto below = m_words[block] & bit::lo_set<word>[keep];
            if (below != 0)
                return block * Bits::Digits + Bits::Digits - 1 - static_cast<std::size_t>(std::countl_zero(below));
        }
        return npos;
    }

    std::size_t m_first = 0;  ///< the first key of its window: key k is bit k - m_first
    std::vector<word> m_words;
    std::size_t m_count = 0;
    /// The words that are not zero, where TrackWords: what clear zeroes.
    [[no_unique_address]] detail::Tracking<TrackWords> m_touched;

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept { return Members<&DenseSet::m_words, &DenseSet::m_touched> {}; }
};

}  // namespace urd::repr

namespace urd::detail
{

/// What urd's own operations on sets of vertices (urd/graphs.hpp) reach of a dense set, and nothing else does: its
/// keys combined with a bitset given as words (`bits` bits, bit j for key `first` + j), a word at a time. The members
/// are the set's own, private; this accessor is the one way to them, so that no public interface hands out bits.
struct dense_words
{
    template<typename K, bool T>
    static void assign(repr::DenseSet<K, T>& set, std::span<const word_t> given, std::size_t first, std::size_t bits)
    { set.assign(given, first, bits); }
    template<typename K, bool T>
    static void unite(repr::DenseSet<K, T>& set, std::span<const word_t> given, std::size_t first, std::size_t bits)
    { set.unite(given, first, bits); }
    template<typename K, bool T>
    static void intersect(repr::DenseSet<K, T>& set, std::span<const word_t> given, std::size_t first, std::size_t bits)
    { set.intersect(given, first, bits); }
    template<typename K, bool T>
    static void assign_common(repr::DenseSet<K, T>& set, const repr::DenseSet<K, T>& from, std::span<const word_t> given, std::size_t first, std::size_t bits)
    { set.assign_common(from, given, first, bits); }
    template<typename K, bool T>
    static void assign_intersection(repr::DenseSet<K, T>& set, const repr::DenseSet<K, T>& lhs, const repr::DenseSet<K, T>& rhs)
    { set.assign_intersection(lhs, rhs); }
    template<typename K, bool T>
    static void intersect_within(repr::DenseSet<K, T>& set, std::span<const word_t> given, std::size_t first, std::size_t bits)
    { set.intersect_within(given, first, bits); }
    template<typename K, bool T>
    static void subtract(repr::DenseSet<K, T>& set, std::span<const word_t> given, std::size_t first, std::size_t bits)
    { set.subtract(given, first, bits); }
    template<typename K, bool T>
    [[nodiscard]] static std::size_t count_common(const repr::DenseSet<K, T>& set, std::span<const word_t> given, std::size_t first, std::size_t bits)
    { return set.count_common(given, first, bits); }
    template<typename K, bool T, typename F>
    static void for_each_common(const repr::DenseSet<K, T>& set, std::span<const word_t> given, std::size_t first, std::size_t bits, F f)
    { set.for_each_common(given, first, bits, std::move(f)); }
};

}  // namespace urd::detail

namespace urd
{

template<>
struct include<"urd/repr/values/dense_set.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<dense_set_decl, S>
{
    using type = repr::DenseSet<typename S::element_type>;
};
template<typename S>
struct representation_of<tracked_dense_set_decl, S>
{
    using type = repr::DenseSet<typename S::element_type, true>;
};

}  // namespace urd::detail

namespace urd
{

/// A dense set hashes as its keys, in order: as the sets it equals do, whatever words they keep. It is no value_range:
/// it compares by its words (operator==), not key by key.
template<typename K, bool TrackWords>
struct Hash<repr::DenseSet<K, TrackWords>>
{
    hash_t operator()(const repr::DenseSet<K, TrackWords>& set) const
        requires Hashable<K>
    { return urd::hash_range(set); }
};

}  // namespace urd
