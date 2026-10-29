#pragma once

// The arena interner: records serialized by cista into a byte arena. Copied in shape from yggdrasil
// (include/yggdrasil/buffer/indexed_hash_set.hpp), names moved to urd::repr, the handle an urd::index.

#include "urd/elements.hpp"
#include "urd/index.hpp"
#include "urd/repr/containers/byte_arena.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/containers/spin_mutex.hpp"
#include "urd/repr/records.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"

#include <boost/unordered/concurrent_flat_set.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cassert>
#include <cista/mode.h>
#include <cista/serialization.h>
#include <cista/targets/buf.h>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

/// The cista mode: checked in Debug builds, unchecked in Release.
#ifdef NDEBUG
inline constexpr ::cista::mode cista_mode = ::cista::mode::UNCHECKED;
#else
inline constexpr ::cista::mode cista_mode = ::cista::mode::NONE;
#endif

/// A pool of records serialized by cista into a byte arena, each distinct one once, under the index insert gave it.
/// A record goes in as an ordinary object; what is stored is its serialization, one block in the arena, read back in
/// place as a const record whose vectors and strings point inside the block. Lookups go through a hash table of
/// indices that hashes and compares the stored records. Thread-safe where ThreadSafe: lookups without a lock, a new
/// record serialized and published under the writer lock; clear, move and destruction need quiescence.
template<typename T, bool ThreadSafe>
class ArenaInterner
{
    static_assert(alignof(T) <= alignof(std::max_align_t), "urd::repr::ArenaInterner: over-aligned records cannot be placed in the arena");
    using Records = std::conditional_t<ThreadSafe, SegmentedVector<const T*, default_first_segment_size, true, std::uint32_t>, std::vector<const T*>>;

    struct Position
    {
        std::uint32_t value;

        friend bool operator==(Position, Position) = default;
    };
    struct PositionHash
    {
        using is_transparent = void;
        const Records* records = nullptr;

        [[nodiscard]] std::size_t operator()(Position position) const { return Hash<T> {}(*(*records)[position.value]); }
        [[nodiscard]] std::size_t operator()(const T& record) const { return Hash<T> {}(record); }
    };
    struct PositionEqual
    {
        using is_transparent = void;
        const Records* records = nullptr;

        [[nodiscard]] bool operator()(Position lhs, Position rhs) const { return EqualTo<T> {}(*(*records)[lhs.value], *(*records)[rhs.value]); }
        [[nodiscard]] bool operator()(const T& lhs, Position rhs) const { return EqualTo<T> {}(lhs, *(*records)[rhs.value]); }
        [[nodiscard]] bool operator()(Position lhs, const T& rhs) const { return EqualTo<T> {}(*(*records)[lhs.value], rhs); }
        [[nodiscard]] bool operator()(const T& lhs, const T& rhs) const { return EqualTo<T> {}(lhs, rhs); }
    };
    using Table = std::conditional_t<ThreadSafe,
                                     boost::concurrent_flat_set<Position, PositionHash, PositionEqual>,
                                     boost::unordered_flat_set<Position, PositionHash, PositionEqual>>;

    /// What the interner owns, at one address: the table's functors point into it.
    struct State
    {
        Records records;
        Table positions = Table(0, PositionHash { &records }, PositionEqual { &records });
        ByteArena arena;
        ::cista::buf<std::vector<std::uint8_t>> scratch;
    };

public:
    using value_type = T;
    using handle_type = urd::index<T>;
    using size_type = std::size_t;

    ArenaInterner() = default;
    ArenaInterner(const ArenaInterner&) = delete;
    ArenaInterner& operator=(const ArenaInterner&) = delete;
    ArenaInterner(ArenaInterner&& other) noexcept : m_state(std::exchange(other.m_state, std::make_unique<State>())) {}
    ArenaInterner& operator=(ArenaInterner&& other) noexcept
    {
        if (this != &other)
            m_state = std::exchange(other.m_state, std::make_unique<State>());
        return *this;
    }
    ~ArenaInterner() = default;

    /// The record's index, and whether it was new: an equal record already stored gives its index.
    std::pair<handle_type, bool> insert(const T& record)
    {
        return insert(record, [&record](handle_type) -> const T& { return record; });
    }
    /// The index `key` gets; where it is new, what `make(index)` gives is stored, built with the index reserved for it.
    /// It must equal `key` (asserted in Debug builds), and `make` must not touch this pool.
    template<std::invocable<handle_type> Factory>
    std::pair<handle_type, bool> insert(const T& key, Factory make)
    {
        if (const auto found = position_of(key))
            return { handle_type(*found), false };
        const auto lock = std::scoped_lock(m_writer);
        if constexpr (ThreadSafe)
            if (const auto found = position_of(key))
                return { handle_type(*found), false };
        auto& state = *m_state;
        const auto position = static_cast<std::uint32_t>(state.records.size());
        const T* stored = publish(make(handle_type(position)));
        assert(EqualTo<T> {}(*stored, key) && "urd::repr::ArenaInterner: the factory must give a record equal to the key");
        state.records.push_back(stored);
        state.positions.insert(Position { position });
        return { handle_type(position), true };
    }
    /// The index of an equal record, or none.
    [[nodiscard]] handle_type find(const T& record) const
    {
        const auto found = position_of(record);
        return found ? handle_type(*found) : handle_type();
    }
    [[nodiscard]] bool contains(const T& record) const { return position_of(record).has_value(); }
    [[nodiscard]] const T& operator[](handle_type at) const
    {
        assert(at.value < m_state->records.size());
        return *m_state->records[at.value];
    }
    [[nodiscard]] size_type size() const { return m_state->records.size(); }
    [[nodiscard]] bool empty() const { return size() == 0; }
    /// The bytes held: the arena's segments, the scratch buffer, the pointers and the table's slots.
    [[nodiscard]] size_type memory_usage() const
    {
        const auto& state = *m_state;
        auto pointers = size_type { 0 };
        if constexpr (ThreadSafe)
            pointers = state.records.memory_usage();
        else
            pointers = state.records.capacity() * sizeof(const T*);
        auto table = size_type { 0 };
        if constexpr (ThreadSafe)
            table = state.positions.size() * (sizeof(Position) + 1);
        else
            table = state.positions.bucket_count() * (sizeof(Position) + 1);
        return state.arena.capacity() + state.scratch.buf_.capacity() + pointers + table;
    }
    /// Forgets every record; the arena keeps its segments.
    void clear()
    {
        auto& state = *m_state;
        state.positions.clear();
        state.records.clear();
        state.arena.clear();
    }

private:
    [[nodiscard]] std::optional<std::uint32_t> position_of(const T& record) const
    {
        if constexpr (ThreadSafe)
        {
            auto found = std::optional<std::uint32_t>();
            m_state->positions.cvisit(record, [&found](const Position& position) { found = position.value; });
            return found;
        }
        else
        {
            const auto found = m_state->positions.find(record);
            return found == m_state->positions.end() ? std::nullopt : std::optional<std::uint32_t>(found->value);
        }
    }
    /// Serializes `record` into the arena and reads it back in place: the stored record. Under the writer lock.
    const T* publish(const T& record)
    {
        auto& state = *m_state;
        state.scratch.reset();
        ::cista::serialize<cista_mode>(state.scratch, record);
        const auto* bytes = state.arena.write(state.scratch.base(), state.scratch.size(), alignof(T));
        return ::cista::deserialize<const T, cista_mode>(bytes, bytes + state.scratch.size());
    }

    std::unique_ptr<State> m_state = std::make_unique<State>();
    [[no_unique_address]] std::conditional_t<ThreadSafe, std::mutex, NoLock> m_writer;
};

}  // namespace urd::repr
