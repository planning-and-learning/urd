#pragma once

#include "urd/elements.hpp"
#include "urd/index.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/pools_fwd.hpp"
#include "urd/traits.hpp"

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
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
template<typename T, bool ThreadSafe>
class SlotVector
{
    struct NoMutex
    {
        static void lock() noexcept {}
        static void unlock() noexcept {}
    };
    using Mutex = std::conditional_t<ThreadSafe, std::mutex, NoMutex>;
    using Count = std::conditional_t<ThreadSafe, std::atomic<std::size_t>, std::size_t>;

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
        requires std::default_initializable<T>
    {
        return place([] { return T(); }, [](T&) {});
    }

    /// Frees the slot for reuse; the handle and references to the element mean nothing after.
    void erase(handle_type handle)
    {
        assert((handle.value) < m_slots.size());
        const auto lock = std::scoped_lock(m_mutex);
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

    /// How many elements are in use.
    [[nodiscard]] size_type size() const { return m_count; }
    [[nodiscard]] bool empty() const { return size() == 0; }
    /// Frees every slot, keeping the segments' storage. Not concurrent with anything.
    void clear()
    {
        m_slots.clear();
        m_free.clear();
        m_count = 0;
    }

private:
    /// Reuses an erased slot with `reuse`, else appends one built by `make`.
    template<typename Make, typename Reuse>
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
    Mutex m_mutex;
    Count m_count { 0 };
};

}  // namespace urd::repr

namespace urd
{

}  // namespace urd

namespace urd
{

template<>
struct include<"urd/repr/slots.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<slot_vector_decl, S>
{
    using type = repr::SlotVector<typename S::element_type>;
};
template<typename S>
struct representation_of<thread_safe_slot_vector_decl, S>
{
    using type = repr::SlotVector<typename S::element_type, true>;
};

}  // namespace urd::detail
