#pragma once

#include "urd/elements.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/core/config.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
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
        return *this;
    }
    void remove(std::size_t index)
    {
        bits().reset(index);
        --m_count;
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

    std::vector<word> m_words;
    std::size_t m_count = 0;
};

/// A set of indices as a sparse set (Briggs and Torczon): the members in one array, and for each index its place
/// in it. Lookup, insert and erase are O(1); clear is O(1) for members that need no destructor; iteration visits
/// only the members, in insertion order until an erase moves the last member into the gap. With KeepOrder, erase
/// shifts the later members instead (O(n)), so the members stay in insertion order.
template<typename K, bool KeepOrder = false>
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

/// A map from indices to values, a slot per index below the largest key, iterated in key order. With Generations,
/// each slot carries the generation it was written in, and clear only starts a new one (O(1)): older slots read
/// as absent and keep their storage, as tyr's DenseRelationMap does. Without, a slot is a std::optional.
/// Iterating gives each key with a view of its value; values change by key (at, insert_or_assign).
template<typename K, typename V, bool Generations>
class IndexMap
{
    static_assert(indexable<K>, "urd::repr::IndexMap: keys must be indices (urd::index_of)");
    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

    struct generation_slot
    {
        std::uint64_t generation = 0;
        V value {};
    };
    using slot = std::conditional_t<Generations, generation_slot, std::optional<V>>;

public:
    class iterator;
    using const_iterator = iterator;
    using key_type = K;
    using mapped_type = V;
    /// What iterating gives: a key and a view of its value.
    using value_type = std::pair<const K, const V&>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    IndexMap() = default;

    template<std::input_iterator InputIt>
    IndexMap(InputIt first, InputIt last)
    { insert(first, last); }

    [[nodiscard]] iterator begin() const { return iterator(this, next_from(0)); }
    [[nodiscard]] iterator end() const { return iterator(this, npos); }
    [[nodiscard]] size_type size() const { return m_count; }
    [[nodiscard]] bool empty() const { return m_count == 0; }

    /// Room for keys below `count`.
    void reserve(size_type count) { m_slots.reserve(count); }

    [[nodiscard]] iterator find(const K& key) const { return contains(key) ? iterator(this, index_of<K>::to_index(key)) : end(); }
    [[nodiscard]] size_type count(const K& key) const { return contains(key) ? 1 : 0; }
    [[nodiscard]] bool contains(const K& key) const { return index_of<K>::is_index(key) && present(index_of<K>::to_index(key)); }
    [[nodiscard]] iterator lower_bound(const K& key) const
    { return iterator(this, index_of<K>::is_index(key) ? next_from(index_of<K>::to_index(key)) : next_from(0)); }
    [[nodiscard]] iterator upper_bound(const K& key) const
    { return iterator(this, index_of<K>::is_index(key) ? next_from(index_of<K>::to_index(key) + 1) : next_from(0)); }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) const { return { lower_bound(key), upper_bound(key) }; }

    [[nodiscard]] const V& at(const K& key) const
    {
        if (!contains(key))
            throw std::out_of_range("urd::repr::IndexMap::at: no value for the key");
        return value(index_of<K>::to_index(key));
    }
    [[nodiscard]] V& at(const K& key)
    {
        if (!contains(key))
            throw std::out_of_range("urd::repr::IndexMap::at: no value for the key");
        return value(index_of<K>::to_index(key));
    }

    /// A key and its value, as any pair (std::pair<const K, V>, or what iterating another map gives).
    template<typename Pair>
        requires requires(const Pair& pair) {
            K(pair.first);
            V(pair.second);
        }
    std::pair<iterator, bool> insert(const Pair& pair)
    { return try_emplace(K(pair.first), pair.second); }
    template<typename Pair>
    iterator insert(const_iterator /*hint*/, const Pair& pair)
    { return insert(pair).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->insert(pair); });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(std::pair<K, V>(std::forward<Args>(args)...)); }
    /// Inserts a value built from `args` unless `key` has one.
    template<typename... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args)
    {
        const auto index = index_of<K>::to_index(key);
        if (present(index))
            return { iterator(this, index), false };
        place(index, V(std::forward<Args>(args)...));
        return { iterator(this, index), true };
    }
    /// Sets the value for `key`, inserting it where there is none.
    template<typename M>
    std::pair<iterator, bool> insert_or_assign(const K& key, M&& mapped)
    {
        const auto index = index_of<K>::to_index(key);
        if (present(index))
        {
            value(index) = std::forward<M>(mapped);
            return { iterator(this, index), false };
        }
        place(index, V(std::forward<M>(mapped)));
        return { iterator(this, index), true };
    }

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
    /// With generations O(1): a new generation, in which no slot is present.
    void clear()
    {
        if constexpr (Generations)
            ++m_generation;
        else
            std::ranges::for_each(m_slots, [](slot& each) { each.reset(); });
        m_count = 0;
    }

    /// A key at a time, in key order, with a view of its value.
    class iterator
    {
    public:
        using value_type = typename IndexMap::value_type;
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

        reference operator*() const { return { index_of<K>::from_index(m_index), m_map->value(m_index) }; }
        arrow operator->() const { return { **this }; }
        iterator& operator++()
        {
            m_index = m_map->next_from(m_index + 1);
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
            m_index = m_map->previous_from(m_index == npos ? m_map->m_slots.size() : m_index);
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
        friend class IndexMap;
        iterator(const IndexMap* map, std::size_t index) : m_map(map), m_index(index) {}

        const IndexMap* m_map = nullptr;
        std::size_t m_index = npos;
    };

private:
    [[nodiscard]] bool present(std::size_t index) const
    {
        if (index >= m_slots.size())
            return false;
        if constexpr (Generations)
            return m_slots[index].generation == m_generation;
        else
            return m_slots[index].has_value();
    }
    [[nodiscard]] const V& value(std::size_t index) const
    {
        if constexpr (Generations)
            return m_slots[index].value;
        else
        {
            auto& present_slot = m_slots[index];
            URD_REPR_ASSUME(present_slot.has_value());  // callers look a value up only where present
            return *present_slot;
        }
    }
    [[nodiscard]] V& value(std::size_t index)
    {
        if constexpr (Generations)
            return m_slots[index].value;
        else
        {
            auto& present_slot = m_slots[index];
            URD_REPR_ASSUME(present_slot.has_value());  // callers look a value up only where present
            return *present_slot;
        }
    }
    void place(std::size_t index, V&& mapped)
    {
        if (index >= m_slots.size())
            m_slots.resize(index + 1);
        if constexpr (Generations)
            m_slots[index] = { m_generation, std::move(mapped) };
        else
            m_slots[index].emplace(std::move(mapped));
        ++m_count;
    }
    void remove(std::size_t index)
    {
        if constexpr (Generations)
            m_slots[index].generation = 0;
        else
            m_slots[index].reset();
        --m_count;
    }
    /// The first present key at or after `index`, else npos.
    [[nodiscard]] std::size_t next_from(std::size_t index) const
    {
        for (; index < m_slots.size(); ++index)
            if (present(index))
                return index;
        return npos;
    }
    /// The last present key before `index`, else npos.
    [[nodiscard]] std::size_t previous_from(std::size_t index) const
    {
        while (index-- > 0)
            if (present(index))
                return index;
        return npos;
    }

    std::vector<slot> m_slots;
    std::size_t m_count = 0;
    std::uint64_t m_generation = 1;  // slots start at 0, so a new slot is absent
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
/// With KeepOrder, in insertion order.
template<indexable K, bool KeepOrder>
struct container_traits<repr::SparseSet<K, KeepOrder>> :
    properties_of<unique,
                  dense,
                  contiguous,
                  shrinkable,
                  retains,
                  std::conditional_t<std::is_trivially_destructible_v<K>, constant_clear, proportional_clear>,
                  std::conditional_t<KeepOrder, insertion_ordered, any_order>>
{
};

/// A slot per key, in key order: sorted; erase frees what the value owns (std::optional::reset), clear scans every
/// slot.
template<indexable K, typename V>
struct container_traits<repr::IndexMap<K, V, false>> : properties_of<sorted, unique, dense, shrinkable, retains>
{
};

/// Clear starts a new generation; stale slots keep their values' storage until written again.
template<indexable K, typename V>
    requires std::default_initializable<V> && std::movable<V>
struct container_traits<repr::IndexMap<K, V, true>> : properties_of<sorted, unique, dense, shrinkable, retains, constant_clear>
{
};

}  // namespace urd
