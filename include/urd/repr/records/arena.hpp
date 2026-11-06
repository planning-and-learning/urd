#pragma once

// The arena interner: records serialized by cista into a byte arena. Copied in shape from yggdrasil
// (include/yggdrasil/buffer/indexed_hash_set.hpp), names moved to urd::repr, the handle an urd::index.

#include "urd/elements/concepts.hpp"
#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/prehashed.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/byte_arena.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/containers/spin_mutex.hpp"
#include "urd/repr/core/boost_memory.hpp"
#include "urd/repr/records/records_fwd.hpp"
#include "urd/traits.hpp"

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
    static_assert(Serializable<T>,
                  "urd::Store: flat stores a record as bytes; one that is not trivially copyable is serialized by cista, which needs "
                  "its members named: give it static constexpr auto cista_members() (urd::Members<&T::a, ...>), or drop flat");
    using Records = std::conditional_t<ThreadSafe, SegmentedVector<const T*, default_first_segment_size, true, std::uint32_t>, std::vector<const T*>>;

    struct Position
    {
        std::uint32_t value;

        friend bool operator==(Position, Position) = default;
    };
    /// A position entering the table with its record's hash, so the table does not hash the record again.
    struct Placed
    {
        std::uint32_t value;
        hash_t hash;

        operator Position() const noexcept { return Position { value }; }  // NOLINT(google-explicit-constructor)
    };
    struct PositionHash
    {
        using is_transparent = void;
        const Records* records = nullptr;

        [[nodiscard]] std::size_t operator()(Position position) const { return Hash<T> {}(*(*records)[position.value]); }
        [[nodiscard]] std::size_t operator()(const T& record) const { return Hash<T> {}(record); }
        [[nodiscard]] std::size_t operator()(const prehashed<T>& key) const noexcept { return key.hash; }
        [[nodiscard]] std::size_t operator()(Placed placed) const noexcept { return placed.hash; }
    };
    struct PositionEqual
    {
        using is_transparent = void;
        const Records* records = nullptr;

        [[nodiscard]] bool operator()(Position lhs, Position rhs) const { return EqualTo<T> {}(*(*records)[lhs.value], *(*records)[rhs.value]); }
        [[nodiscard]] bool operator()(const T& lhs, Position rhs) const { return EqualTo<T> {}(lhs, *(*records)[rhs.value]); }
        [[nodiscard]] bool operator()(Position lhs, const T& rhs) const { return EqualTo<T> {}(*(*records)[lhs.value], rhs); }
        [[nodiscard]] bool operator()(const T& lhs, const T& rhs) const { return EqualTo<T> {}(lhs, rhs); }
        [[nodiscard]] bool operator()(const prehashed<T>& lhs, Position rhs) const { return (*this)(lhs.key, rhs); }
        [[nodiscard]] bool operator()(Position lhs, const prehashed<T>& rhs) const { return (*this)(lhs, rhs.key); }
        [[nodiscard]] bool operator()(Placed lhs, Position rhs) const { return (*this)(Position { lhs.value }, rhs); }
        [[nodiscard]] bool operator()(Position lhs, Placed rhs) const { return (*this)(lhs, Position { rhs.value }); }
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
    ArenaInterner(ArenaInterner&& other) noexcept : m_state(std::exchange(other.m_state, fresh_state())) {}
    ArenaInterner& operator=(ArenaInterner&& other) noexcept
    {
        if (this != &other)
            m_state = std::exchange(other.m_state, fresh_state());
        return *this;
    }
    ~ArenaInterner() = default;

    /// The record's index, and whether it was new: an equal record already stored gives its index.
    /// Each lookup and insert takes the record or a prehashed<T> of it (urd/elements/prehashed.hpp), not hashed again.
    std::pair<handle_type, bool> insert(const T& record) { return insert(prehash(record)); }
    std::pair<handle_type, bool> insert(const prehashed<T>& key)
    {
        return insert(key, [&key](handle_type) -> const T& { return key.key; });
    }
    /// The index `key` gets; where it is new, what `make(index)` gives is stored, built with the index reserved for it.
    /// It must equal `key` (asserted in Debug builds), and `make` must not touch this pool.
    template<std::invocable<handle_type> Factory>
    std::pair<handle_type, bool> insert(const T& key, Factory make)
    { return insert(prehash(key), std::move(make)); }
    template<std::invocable<handle_type> Factory>
    std::pair<handle_type, bool> insert(const prehashed<T>& key, Factory make)
    {
        if (const auto found = position_of(key))
            return { handle_type(*found), false };
        const auto lock = std::scoped_lock(m_writer);
        if constexpr (ThreadSafe)
            if (const auto found = position_of(key))
                return { handle_type(*found), false };
        auto& state = this->state();
        const auto position = static_cast<std::uint32_t>(state.records.size());
        const T* stored = publish(make(handle_type(position)));
        assert(EqualTo<T> {}(*stored, key.key) && "urd::repr::ArenaInterner: the factory must give a record equal to the key");
        state.records.push_back(stored);
        state.positions.insert(Placed { position, key.hash });
        return { handle_type(position), true };
    }
    /// The index of an equal record, or none.
    [[nodiscard]] handle_type find(const T& record) const { return find(prehash(record)); }
    [[nodiscard]] handle_type find(const prehashed<T>& key) const
    {
        const auto found = position_of(key);
        return found ? handle_type(*found) : handle_type();
    }
    [[nodiscard]] bool contains(const T& record) const { return contains(prehash(record)); }
    [[nodiscard]] bool contains(const prehashed<T>& key) const { return position_of(key).has_value(); }
    [[nodiscard]] const T& operator[](handle_type at) const
    {
        assert((at.value) < m_state->records.size());
        return *m_state->records[at.value];
    }
    [[nodiscard]] size_type size() const { return m_state ? m_state->records.size() : 0; }
    [[nodiscard]] bool empty() const { return size() == 0; }
    /// Forgets every record; the arena keeps its segments.
    void clear()
    {
        if (!m_state)
            return;
        auto& state = *m_state;
        state.positions.clear();
        state.records.clear();
        state.arena.clear();
    }

private:
    friend struct urd::memory_of<ArenaInterner>;

    [[nodiscard]] std::optional<std::uint32_t> position_of(const prehashed<T>& key) const
    {
        if (!m_state)
            return std::nullopt;
        if constexpr (ThreadSafe)
        {
            auto found = std::optional<std::uint32_t>();
            m_state->positions.cvisit(key, [&found](const Position& position) { found = position.value; });
            return found;
        }
        else
        {
            const auto found = m_state->positions.find(key);
            return found == m_state->positions.end() ? std::nullopt : std::optional<std::uint32_t>(found->value);
        }
    }
    /// Serializes `record` into the arena and reads it back in place: the stored record. Under the writer lock.
    const T* publish(const T& record)
    {
        auto& state = this->state();
        state.scratch.reset();
        ::cista::serialize<cista_mode>(state.scratch, record);
        const auto* bytes = state.arena.write(state.scratch.base(), state.scratch.size(), alignof(T));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): cista reads a record from [begin, end) of bytes
        return ::cista::deserialize<const T, cista_mode>(bytes, bytes + state.scratch.size());
    }

    /// The state a new or moved-from interner has: none until the first insert where single-threaded (an empty pool
    /// owns no memory); made at once where threads share it, as lookups without a lock cannot race its making.
    [[nodiscard]] static std::unique_ptr<State> fresh_state()
    {
        if constexpr (ThreadSafe)
            return std::make_unique<State>();
        else
            return nullptr;
    }
    /// The state, made where there is none. Under the writer lock.
    State& state()
    {
        if (!m_state)
            m_state = std::make_unique<State>();
        return *m_state;
    }

    std::unique_ptr<State> m_state = fresh_state();
    // A spin lock: the critical section is an append and a table insert, short as yggdrasil's benchmarks of its
    // interners found, where it beat std::mutex.
    [[no_unique_address]] std::conditional_t<ThreadSafe, SpinMutex, NoLock> m_writer;
};

}  // namespace urd::repr

namespace urd
{

/// Its state: the pointers to the records, the table, the arena's segments and the scratch buffer.
template<typename T, bool ThreadSafe>
struct memory_of<repr::ArenaInterner<T, ThreadSafe>>
{
    static std::size_t bytes(const repr::ArenaInterner<T, ThreadSafe>& held)
    {
        if (!held.m_state)
            return 0;
        const auto& state = *held.m_state;
        return sizeof(state) + memory_usage(state.records) + memory_usage(state.positions) + memory_usage(state.arena) + state.scratch.buf_.capacity();
    }
};

template<>
struct include<"urd/repr/records/arena.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<arena_interner_decl, S>
{
    using type = repr::ArenaInterner<typename S::element_type>;
};
template<typename S>
struct representation_of<thread_safe_arena_interner_decl, S>
{
    using type = repr::ArenaInterner<typename S::element_type, true>;
};

}  // namespace urd::detail
