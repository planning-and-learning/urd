#pragma once

// A map from indices to values, a slot per index, for dense keys: stored by position, not hashed.

#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/containers/spin_mutex.hpp"
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

/// A map from indices to values, a slot per index below the largest key, iterated in key order. With Generations,
/// each slot carries the generation it was written in, and clear only starts a new one (O(1)): older slots read
/// as absent and keep their storage. Without, a slot is a std::optional.
/// Iterating gives each key with a view of its value; values change by key (at, insert_or_assign, update). With
/// Stable, the slots lie in a SegmentedVector and never move: what is stable is the value at, operator[] and update
/// reach and the .second of an iterated entry (the entry itself is built on the fly). With ThreadSafe (which needs
/// Stable and Generations), the segments are the thread-safe ones and each slot has a spin lock: update and read
/// may run from threads at once; clear, erase, iteration and the rest need no other operation running.
template<typename K, typename V, bool Generations, bool ThreadSafe, bool Stable>
class IndexMap
{
    static_assert(indexable<K>, "urd::repr::IndexMap: keys must be indices (urd::index_of)");
    static_assert(!ThreadSafe || Generations, "urd::repr::IndexMap: thread-safe only with generations");
    static_assert(!ThreadSafe || Stable, "urd::repr::IndexMap: threads share slots that never move (Stable)");
    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

    struct generation_slot
    {
        std::uint64_t generation = 0;
        V value {};
        [[no_unique_address]] mutable std::conditional_t<ThreadSafe, SpinMutex, NoLock> lock;
    };
    using slot = std::conditional_t<Generations, generation_slot, std::optional<V>>;
    using Slots = std::conditional_t<Stable, SegmentedVector<slot, default_first_segment_size, ThreadSafe>, std::vector<slot>>;
    /// A count threads may change at once; moving or copying it, like the map, needs no other operation running.
    struct AtomicCount
    {
        std::atomic<std::size_t> value { 0 };

        explicit AtomicCount(std::size_t initial = 0) noexcept : value(initial) {}
        AtomicCount(const AtomicCount& other) noexcept : value(other.value.load()) {}
        AtomicCount& operator=(const AtomicCount& other) noexcept
        {
            value = other.value.load();
            return *this;
        }
        AtomicCount& operator=(std::size_t count) noexcept
        {
            value = count;
            return *this;
        }
        AtomicCount(AtomicCount&& other) noexcept : value(other.value.load()) {}
        AtomicCount& operator=(AtomicCount&& other) noexcept { return *this = static_cast<const AtomicCount&>(other); }
        ~AtomicCount() = default;
        AtomicCount& operator++() noexcept
        {
            ++value;
            return *this;
        }
        AtomicCount& operator--() noexcept
        {
            --value;
            return *this;
        }
        explicit operator std::size_t() const noexcept { return value.load(); }
    };
    using Count = std::conditional_t<ThreadSafe, AtomicCount, std::size_t>;

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
    [[nodiscard]] size_type size() const { return static_cast<size_type>(m_count); }
    [[nodiscard]] bool empty() const { return size() == 0; }

    /// Room for keys below `count` (segments grow as keys arrive).
    void reserve(size_type count)
    {
        if constexpr (!Stable)
            m_slots.reserve(count);
    }

    /// Calls update(value, held) with the value of `key`, where held says whether the key had one; where it had not,
    /// the key has one from now on, and the value handed over is a default one, or with Generations the value the slot
    /// kept from an earlier generation, storage and all, for update to reuse. Returns what update returns.
    template<typename Update>
        requires std::invocable<Update&, V&, bool>
    decltype(auto) update(const K& key, Update update)
    {
        auto& entry = slot_at(index_of<K>::to_index(key));
        return locked(entry,
                      [&]() -> decltype(auto)
                      {
                          if constexpr (Generations)
                          {
                              const bool held = entry.generation == m_generation;
                              entry.generation = m_generation;
                              if (!held)
                                  ++m_count;
                              return update(entry.value, held);
                          }
                          else
                          {
                              const bool held = entry.has_value();
                              if (!held)
                              {
                                  entry.emplace();
                                  ++m_count;
                              }
                              return update(*entry, held);
                          }
                      });
    }
    /// read(value) where `key` has a value, else `missing`.
    template<typename Read, typename Result>
        requires std::invocable<Read&, const V&>
    [[nodiscard]] Result read(const K& key, Read read, Result missing) const
    {
        if (!index_of<K>::is_index(key) || index_of<K>::to_index(key) >= m_slots.size())
            return missing;
        const auto index = index_of<K>::to_index(key);
        return locked(m_slots[index], [&] { return present(index) ? Result(read(value(index))) : missing; });
    }

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
        auto& entry = slot_at(index);
        const bool inserted = locked(entry,
                                     [&]
                                     {
                                         if (present(index))
                                             return false;
                                         place(entry, V(std::forward<Args>(args)...));
                                         return true;
                                     });
        return { iterator(this, index), inserted };
    }
    /// Sets the value for `key`, inserting it where there is none.
    template<typename M>
    std::pair<iterator, bool> insert_or_assign(const K& key, M&& mapped)
    {
        const auto index = index_of<K>::to_index(key);
        auto& entry = slot_at(index);
        const bool inserted = locked(entry,
                                     [&]
                                     {
                                         if (present(index))
                                         {
                                             value(index) = std::forward<M>(mapped);
                                             return false;
                                         }
                                         place(entry, V(std::forward<M>(mapped)));
                                         return true;
                                     });
        return { iterator(this, index), inserted };
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
            for (std::size_t index = 0; index < m_slots.size(); ++index)
                m_slots[index].reset();
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
    /// Puts a value in an empty slot, under its lock where slots have locks.
    void place(slot& entry, V&& mapped)
    {
        if constexpr (Generations)
        {
            entry.generation = m_generation;
            entry.value = std::move(mapped);
        }
        else
            entry.emplace(std::move(mapped));
        ++m_count;
    }
    /// The slot of `index`, growing to it. Thread-safe slots grow by appends, each under the vector's lock; threads
    /// growing at once may append more than asked, never fewer.
    slot& slot_at(std::size_t index)
    {
        if constexpr (ThreadSafe)
            while (m_slots.size() <= index)
                m_slots.emplace_back();
        else if (index >= m_slots.size())
            m_slots.resize(index + 1);
        return m_slots[index];
    }
    /// Runs `body` holding the slot's lock, where slots have locks.
    template<typename Body>
    static decltype(auto) locked(const slot& entry, Body body)
    {
        if constexpr (ThreadSafe)
        {
            const auto guard = std::scoped_lock(entry.lock);
            return body();
        }
        else
            return body();
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

    Slots m_slots;
    Count m_count { 0 };
    std::uint64_t m_generation = 1;  // slots start at 0, so a new slot is absent
};

}  // namespace urd::repr

namespace urd
{

template<>
struct include<"urd/repr/index_map.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<dense_map_decl, S>
{
    using type = repr::IndexMap<typename S::key_type, typename S::mapped_type, false>;
};
template<typename S>
struct representation_of<stable_dense_map_decl, S>
{
    using type = repr::IndexMap<typename S::key_type, typename S::mapped_type, false, false, true>;
};
template<typename S>
struct representation_of<generation_map_decl, S>
{
    using type = repr::IndexMap<typename S::key_type, typename S::mapped_type, true>;
};
template<typename S>
struct representation_of<stable_generation_map_decl, S>
{
    using type = repr::IndexMap<typename S::key_type, typename S::mapped_type, true, false, true>;
};
template<typename S>
struct representation_of<thread_safe_generation_map_decl, S>
{
    using type = repr::IndexMap<typename S::key_type, typename S::mapped_type, true, true>;
};

}  // namespace urd::detail
