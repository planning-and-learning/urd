#pragma once

// A map from indices to values, a slot per index, for dense keys: stored by position, not hashed.

#include "urd/elements/hash.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/containers/spin_mutex.hpp"
#include "urd/config.hpp"
#include "urd/repr/maps/dense_map_fwd.hpp"
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
/// may run from threads at once; clear, erase, iteration and the rest need no other operation running. A slot is
/// written under its lock, and a new one is published (its generation stored, release) once its value is made; read,
/// contains and find load the generation (acquire) and read a present value without the lock. So a value is read
/// while whatever update or at reaches it changes it: it must take that (a concurrent Store), or not change once made.
template<typename K, typename V, bool Generations, bool ThreadSafe, bool Stable>
class IndexMap
{
    static_assert(indexable<K>, "urd::repr::IndexMap: keys must be indices (urd::index_of)");
    static_assert(!ThreadSafe || Generations, "urd::repr::IndexMap: thread-safe only with generations");
    static_assert(!ThreadSafe || Stable, "urd::repr::IndexMap: threads share slots that never move (Stable)");
    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

    struct generation_slot
    {
        /// mutable for std::atomic_ref, which C++20 has only of non-const objects.
        mutable std::uint64_t generation = 0;
        V value {};
        [[no_unique_address]] mutable std::conditional_t<ThreadSafe, SpinMutex, NoLock> lock;
    };
    using optional_slot = std::optional<V>;
    using slot = std::conditional_t<Generations, generation_slot, optional_slot>;
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
                          const bool held = claim(entry);
                          if (!held)
                              ++m_count;
                          // Readers see a new slot only whole: after update made its value.
                          const auto publish_new_slot_once_made = publish_on_exit { this, held ? nullptr : &entry };
                          return update(value_of(entry), held);
                      });
    }
    /// read(value) where `key` has a value, else `missing`.
    template<typename Read, typename Result>
        requires std::invocable<Read&, const V&>
    [[nodiscard]] Result read(const K& key, Read read, Result missing) const
    {
        const auto* entry = held_slot(key);
        return entry == nullptr ? missing : Result(read(value_of(*entry)));
    }

    [[nodiscard]] iterator find(const K& key) const { return contains(key) ? iterator(this, index_of<K>::to_index(key)) : end(); }
    [[nodiscard]] size_type count(const K& key) const { return contains(key) ? 1 : 0; }
    [[nodiscard]] bool contains(const K& key) const { return held_slot(key) != nullptr; }
    [[nodiscard]] iterator lower_bound(const K& key) const
    { return iterator(this, index_of<K>::is_index(key) ? next_from(index_of<K>::to_index(key)) : next_from(0)); }
    [[nodiscard]] iterator upper_bound(const K& key) const
    { return iterator(this, index_of<K>::is_index(key) ? next_from(index_of<K>::to_index(key) + 1) : next_from(0)); }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) const { return { lower_bound(key), upper_bound(key) }; }

    [[nodiscard]] const V& at(const K& key) const
    {
        if (const auto* entry = held_slot(key))
            return value_of(*entry);
        throw std::out_of_range("urd::repr::IndexMap::at: no value for the key");
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
        return index < m_slots.size() && holds(m_slots[index]);
    }
    /// The slot of `key` where it holds a value, else nullptr: the slot is looked up once.
    [[nodiscard]] const slot* held_slot(const K& key) const
    {
        if (!index_of<K>::is_index(key))
            return nullptr;
        const auto index = index_of<K>::to_index(key);
        if (index >= m_slots.size())
            return nullptr;
        const auto& entry = m_slots[index];
        return holds(entry) ? &entry : nullptr;
    }
    [[nodiscard]] const V& value(std::size_t index) const { return value_of(m_slots[index]); }
    [[nodiscard]] V& value(std::size_t index) { return value_of(m_slots[index]); }

    // What a slot type does; the storage mode is the slot type. claim makes a slot's value ready and says whether it
    // held one (a generation slot reuses the value it kept); publish makes a new slot visible.
    [[nodiscard]] bool holds(const generation_slot& entry) const noexcept { return generation_of(entry) == m_generation; }
    [[nodiscard]] static bool holds(const optional_slot& entry) noexcept { return entry.has_value(); }
    template<typename Slot>
        requires std::same_as<std::remove_const_t<Slot>, generation_slot>
    [[nodiscard]] static auto& value_of(Slot& entry) noexcept
    { return entry.value; }
    template<typename Slot>
        requires std::same_as<std::remove_const_t<Slot>, optional_slot>
    [[nodiscard]] static auto& value_of(Slot& entry) noexcept
    {
        URD_ASSUME(entry.has_value());  // callers look a value up only where present
        return *entry;
    }
    [[nodiscard]] bool claim(generation_slot& entry) const noexcept { return holds(entry); }
    [[nodiscard]] static bool claim(optional_slot& entry)
    {
        if (entry.has_value())
            return true;
        entry.emplace();
        return false;
    }
    /// Stores the generation with release where threads share slots: after the value, which readers then see whole.
    void publish(generation_slot& entry) const noexcept { store_generation(entry, m_generation); }
    static void publish(optional_slot& /*entry*/) noexcept {}
    static void vacate(generation_slot& entry) noexcept { store_generation(entry, 0); }
    static void vacate(optional_slot& entry) noexcept { entry.reset(); }
    /// Publishes a new slot when it goes out of scope (null: the slot was held, nothing to publish).
    struct publish_on_exit
    {
        const IndexMap* map;
        slot* entry;

        ~publish_on_exit()
        {
            if (entry != nullptr)
                map->publish(*entry);
        }
    };
    /// Puts a value in an empty slot, under its lock where slots have locks.
    void place(generation_slot& entry, V&& mapped)
    {
        entry.value = std::move(mapped);
        publish(entry);
        ++m_count;
    }
    void place(optional_slot& entry, V&& mapped)
    {
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
    /// A slot's generation, as published: loaded with acquire where threads share slots.
    [[nodiscard]] static std::uint64_t generation_of(const generation_slot& entry) noexcept
    {
        if constexpr (ThreadSafe)
            return std::atomic_ref(entry.generation).load(std::memory_order_acquire);
        else
            return entry.generation;
    }
    /// Stores a slot's generation: with release where threads share slots, after its value.
    static void store_generation(generation_slot& entry, std::uint64_t generation) noexcept
    {
        if constexpr (ThreadSafe)
            std::atomic_ref(entry.generation).store(generation, std::memory_order_release);
        else
            entry.generation = generation;
    }
    void remove(std::size_t index)
    {
        vacate(m_slots[index]);
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

    friend struct urd::memory_of<IndexMap>;

    Slots m_slots;
    Count m_count { 0 };
    std::uint64_t m_generation = 1;  // slots start at 0, so a new slot is absent
};

}  // namespace urd::repr

/// A slot per key index up to the greatest held (its capacity), and what the values held own.
template<typename K, typename V, bool Generations, bool ThreadSafe, bool Stable>
struct urd::memory_of<urd::repr::IndexMap<K, V, Generations, ThreadSafe, Stable>>
{
    using map = repr::IndexMap<K, V, Generations, ThreadSafe, Stable>;
    static std::size_t bytes(const map& held)
    {
        auto result = held.m_slots.capacity() * sizeof(typename map::slot);
        if constexpr (!std::is_trivially_copyable_v<V>)
            for (const auto& [key, value] : held)
                result += memory_usage(value);
        return result;
    }
};

namespace urd
{

template<>
struct include<"urd/repr/maps/index_map.hpp">
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
