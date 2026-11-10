#pragma once

#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/containers/spin_mutex.hpp"
#include "urd/repr/slots/slots_fwd.hpp"
#include "urd/traits.hpp"

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <gtl/phmap.hpp>
#include <limits>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

/// Elements in slots that never move, with the indices of erased slots kept for reuse: erase anywhere in O(1),
/// references to the other elements stay valid. An erased element is not destroyed; the slot keeps it, and its
/// storage, until an insert assigns over it or acquire hands it out as it is. With ThreadSafe, inserts, acquires
/// and erases of different handles may run concurrently, and with reads of elements not erased.
///
/// With a key type K (single-threaded), slots are only acquired by key: acquire(key, args...) hands out a slot erased
/// under an equal key as it is, else one made from T(key, args...). Each slot keeps its key, and its free list is its
/// key's (a hash map of K, urd::Hash and urd::EqualTo): once each key has had as many slots in use at once, acquiring
/// and erasing allocate nothing.
template<typename T, bool ThreadSafe, typename K>
class SlotVector
{
    friend struct urd::memory_of<SlotVector>;

    static constexpr bool keyed = !std::is_void_v<K>;
    static_assert(!(keyed && ThreadSafe), "urd::SlotVector: keyed slots are single-threaded");
    struct NoKeys
    {
    };
    using Key = std::conditional_t<keyed, K, char>;  ///< K, where it is a type
    using Keys = std::conditional_t<keyed, std::vector<Key>, NoKeys>;
    using FreeByKey = std::conditional_t<keyed, gtl::flat_hash_map<Key, std::vector<std::uint32_t>, urd::Hash<Key>, urd::EqualTo<Key>>, NoKeys>;

    using Mutex = std::conditional_t<ThreadSafe, std::mutex, NoLock>;
    using Count = std::conditional_t<ThreadSafe, std::atomic<std::size_t>, std::size_t>;
    using Pool = std::conditional_t<ThreadSafe, std::pmr::synchronized_pool_resource, std::pmr::unsynchronized_pool_resource>;
    /// The pool the control blocks come from, over the heap, counting the bytes it holds of it (for memory_usage: a
    /// pool resource does not tell).
    struct Blocks final : std::pmr::memory_resource
    {
        Count bytes { 0 };
        Pool pool { this };

    private:
        void* do_allocate(std::size_t size, std::size_t alignment) override
        {
            bytes += size;
            return std::pmr::new_delete_resource()->allocate(size, alignment);
        }
        void do_deallocate(void* at, std::size_t size, std::size_t alignment) override
        {
            bytes -= size;
            std::pmr::new_delete_resource()->deallocate(at, size, alignment);
        }
        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
    };

public:
    using value_type = T;
    /// An element's handle is the index of its slot. A slot is reused once its element is erased, so a handle is valid
    /// until then; it keys a dense Store of what goes with the element.
    using handle_type = urd::index<T>;
    using size_type = std::size_t;

    SlotVector() = default;

    /// A slot holding `element`: an erased one where there is one, else a new one.
    handle_type insert(const T& element)
    {
        return place([&element] { return element; }, [&element](T& slot) { slot = element; });
    }
    handle_type insert(T&& element)
    {
        return place([&element] { return std::move(element); }, [&element](T& slot) { slot = std::move(element); });
    }
    template<typename... Args>
    handle_type emplace(Args&&... args)
    { return insert(T(std::forward<Args>(args)...)); }
    /// A slot to use as it is: an erased element, with whatever it held and the storage it kept (a scratch buffer
    /// reused without allocating), else a new default-constructed one.
    handle_type acquire()
        requires std::default_initializable<T> && (!keyed)
    {
        return place([] { return T(); }, [](T&) {});
    }
    /// A slot to use as it is, of `key`: one erased under an equal key, with whatever it held, else a new T(key, args...).
    template<typename... Args>
        requires keyed && std::constructible_from<T, const Key&, Args...>
    handle_type acquire(const Key& key, Args&&... args)
    {
        if (const auto found = m_free_by_key.find(key); found != m_free_by_key.end() && !found->second.empty())
        {
            const auto slot = found->second.back();
            found->second.pop_back();
            ++m_count;
            return handle_type { slot };
        }
        const auto index = m_slots.emplace_back_with_index([&](std::size_t) { return T(key, std::forward<Args>(args)...); },
                                                           std::numeric_limits<std::uint32_t>::max() - 1);
        m_keys.push_back(key);
        ++m_count;
        return handle_type { static_cast<std::uint32_t>(index) };
    }

    /// Frees the slot for reuse (keyed: under its key); the handle and references to the element mean nothing after.
    void erase(handle_type handle)
    {
        assert((handle.value) < m_slots.size());
        const auto lock = std::scoped_lock(m_mutex);
        if constexpr (keyed)
            m_free_by_key[m_keys[handle.value]].push_back(handle.value);
        else
            m_free.push_back(handle.value);
        --m_count;
    }

    [[nodiscard]] const T& operator[](handle_type handle) const
    {
        assert((handle.value) < m_slots.size());
        return m_slots[handle.value];
    }
    [[nodiscard]] T& operator[](handle_type handle)
    {
        assert((handle.value) < m_slots.size());
        return m_slots[handle.value];
    }

    /// Where shared owners of its elements (urd/store/slots.hpp, make_shared) take their control blocks from: made on
    /// first use, so a SlotVector that shares nothing allocates none, and a moved-from one makes a new one.
    [[nodiscard]] std::pmr::memory_resource* blocks()
    {
        if (!m_blocks)
            m_blocks = std::make_unique<Blocks>();
        return &m_blocks->pool;
    }

    /// How many elements are in use.
    [[nodiscard]] size_type size() const { return m_count; }
    [[nodiscard]] bool empty() const { return size() == 0; }
    /// Frees every slot, keeping the segments' storage. Not concurrent with anything.
    void clear()
    {
        m_slots.clear();
        m_free.clear();
        if constexpr (keyed)
        {
            m_keys.clear();
            m_free_by_key.clear();
        }
        m_count = 0;
    }

private:
    /// Reuses an erased slot with `reuse`, else appends one built by `make`.
    template<typename Make, typename Reuse>
        requires(!keyed)
    handle_type place(Make make, Reuse reuse)
    {
        auto slot = std::numeric_limits<std::uint32_t>::max();
        {
            const auto lock = std::scoped_lock(m_mutex);
            if (!m_free.empty())
            {
                slot = m_free.back();
                m_free.pop_back();
            }
            ++m_count;
        }
        if (slot != std::numeric_limits<std::uint32_t>::max())
        {
            reuse(m_slots[slot]);
            return handle_type { slot };
        }
        const auto index = m_slots.emplace_back_with_index([&make](std::size_t) { return make(); }, std::numeric_limits<std::uint32_t>::max() - 1);
        return handle_type { static_cast<std::uint32_t>(index) };
    }

    SegmentedVector<T, default_first_segment_size, ThreadSafe> m_slots;
    std::vector<std::uint32_t> m_free;
    /// Keyed: each slot's key, by slot, and the erased slots of each key.
    [[no_unique_address]] Keys m_keys;
    [[no_unique_address]] FreeByKey m_free_by_key;
    Mutex m_mutex;
    Count m_count { 0 };
    /// Every control block taken from it must be back before it goes. Made on first use where single-threaded (blocks());
    /// made up front where thread-safe, which a concurrent first use could otherwise race on (and which never moves).
    std::unique_ptr<Blocks> m_blocks = ThreadSafe ? std::make_unique<Blocks>() : nullptr;
};

}  // namespace urd::repr

template<> struct urd::include<"urd/repr/slots/slots.hpp"> {};

namespace urd::detail
{

template<typename S>
struct representation_of<slot_vector_decl, S>
{
    using type = repr::SlotVector<typename S::element_type, false, typename S::slot_key_type>;
};
template<typename S>
struct representation_of<thread_safe_slot_vector_decl, S>
{
    using type = repr::SlotVector<typename S::element_type, true>;
};

}  // namespace urd::detail

/// Its slots, freed ones included, the free list (keyed: the keys and the free lists' storage) and the control
/// blocks' pool.
template<typename T, bool ThreadSafe, typename K>
struct urd::memory_of<urd::repr::SlotVector<T, ThreadSafe, K>>
{
    static std::size_t bytes(const repr::SlotVector<T, ThreadSafe, K>& held)
    {
        auto result = memory_usage(held.m_slots) + memory_usage(held.m_free) + (held.m_blocks ? sizeof(*held.m_blocks) + held.m_blocks->bytes : 0);
        if constexpr (!std::is_void_v<K>)
        {
            result += held.m_keys.capacity() * sizeof(K) + held.m_free_by_key.capacity() * sizeof(*held.m_free_by_key.begin());
            for (const auto& [key, free] : held.m_free_by_key)
                result += memory_usage(free);
        }
        return result;
    }
};
