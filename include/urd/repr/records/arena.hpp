#pragma once

// The arena interner: records serialized by cista into a byte arena, interned by an indexed set over them.

#include "urd/elements/concepts.hpp"
#include "urd/elements/index.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/byte_arena.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/core/position_iterator.hpp"
#include "urd/repr/records/records.hpp"
#include "urd/repr/records/records_fwd.hpp"
#include "urd/traits.hpp"

#include <cista/mode.h>
#include <cista/serialization.h>
#include <cista/targets/buf.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace urd::repr
{

/// The cista mode: checked in Debug builds, unchecked in Release.
#ifdef NDEBUG
inline constexpr ::cista::mode cista_mode = ::cista::mode::UNCHECKED;
#else
inline constexpr ::cista::mode cista_mode = ::cista::mode::NONE;
#endif

/// Records serialized by cista into a byte arena, appended and read back by position: the element storage of an indexed
/// set (urd/repr/values/indexed.hpp). A record goes in as an ordinary object; what is stored is its serialization, one
/// block in the arena, read back in place as a const record whose vectors and strings point inside the block. Nothing
/// moves. With ThreadSafe, records below size() may be read while one thread (under the owner's lock) appends; clear
/// needs no other operation running, and keeps the arena's segments.
template<typename T, bool ThreadSafe>
class ArenaRecords
{
    static_assert(alignof(T) <= alignof(std::max_align_t), "urd::repr::ArenaRecords: over-aligned records cannot be placed in the arena");
    static_assert(Serializable<T>,
                  "urd::Store: flat stores a record as bytes; one that is not trivially copyable is serialized by cista, which needs "
                  "its members named: give it static constexpr auto cista_members() (urd::Members<&T::a, ...>), or drop flat");
    friend struct urd::memory_of<ArenaRecords>;

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using const_iterator = PositionIterator<const ArenaRecords>;
    using iterator = const_iterator;

    [[nodiscard]] size_type size() const { return m_records.size(); }
    [[nodiscard]] bool empty() const { return size() == 0; }
    [[nodiscard]] const_iterator begin() const { return const_iterator(this, 0); }
    [[nodiscard]] const_iterator end() const { return const_iterator(this, size()); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend() const { return end(); }
    [[nodiscard]] const T& operator[](size_type position) const { return *m_records[position]; }

    /// Serializes `record` into the arena and appends the record read back in place: by one writer at a time.
    void push_back(const T& record)
    {
        m_scratch.reset();
        ::cista::serialize<cista_mode>(m_scratch, record);
        const auto* bytes = m_arena.write(m_scratch.base(), m_scratch.size(), alignof(T));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): cista reads a record from [begin, end) of bytes
        m_records.push_back(::cista::deserialize<const T, cista_mode>(bytes, bytes + m_scratch.size()));
    }
    void clear()
    {
        m_records.clear();
        m_arena.clear();
    }

private:
    std::conditional_t<ThreadSafe, SegmentedVector<const T*, default_first_segment_size, true, std::uint32_t>, std::vector<const T*>> m_records;
    ByteArena m_arena;
    ::cista::buf<std::vector<std::uint8_t>> m_scratch;
};

/// A pool of records serialized by cista into a byte arena, each distinct one once, under the index insert gave it: an
/// indexed set over ArenaRecords, which hashes and compares the stored records. Thread-safe where ThreadSafe: lookups
/// without a lock, a new record serialized and appended under the writer lock; clear, move and destruction need
/// quiescence. H is the index it hands out (urd::pool<V, H>).
template<typename T, bool ThreadSafe, typename H>
class ArenaInterner : public detail::Interner<IndexedSet<T, std::uint32_t, ThreadSafe, true, false, ArenaRecords<T, ThreadSafe>>, H>
{
};

}  // namespace urd::repr

namespace urd
{

/// The pointers to its records, the arena's segments and the scratch buffer.
template<typename T, bool ThreadSafe>
struct memory_of<repr::ArenaRecords<T, ThreadSafe>>
{
    static std::size_t bytes(const repr::ArenaRecords<T, ThreadSafe>& held)
    { return memory_usage(held.m_records) + memory_usage(held.m_arena) + held.m_scratch.buf_.capacity(); }
};
template<typename T, bool ThreadSafe, typename H>
struct memory_of<repr::ArenaInterner<T, ThreadSafe, H>> :
    memory_of<repr::detail::Interner<repr::IndexedSet<T, std::uint32_t, ThreadSafe, true, false, repr::ArenaRecords<T, ThreadSafe>>, H>>
{
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
    using type = repr::ArenaInterner<typename S::element_type, false, typename S::handle_type>;
};
template<typename S>
struct representation_of<thread_safe_arena_interner_decl, S>
{
    using type = repr::ArenaInterner<typename S::element_type, true, typename S::handle_type>;
};

}  // namespace urd::detail
