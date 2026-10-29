#pragma once

#include "urd/elements.hpp"
#include "urd/index.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace urd::repr
{

/// A record cista can serialize into the arena: hashable (its identity), and naming its members for cista
/// (cista_members() gives urd::Members<&T::a, …>).
template<typename T>
concept flat_record = hashable<T> && requires { T::cista_members(); };

/// A pool of trivially copyable records, each distinct one once, in an indexed set: a record is its own block of
/// bytes already. With Stable the records lie in segments that never move, so [index] hands out references that
/// hold; thread-safe where ThreadSafe (which needs Stable): lookups without a lock, a new record appended under one.
template<typename T, bool ThreadSafe = false, bool Stable = ThreadSafe>
class RecordInterner
{
    using Records = IndexedSet<T, std::uint32_t, ThreadSafe, Stable>;

public:
    using value_type = T;
    using handle_type = urd::index<T>;
    using size_type = std::size_t;

    std::pair<handle_type, bool> insert(const T& record)
    {
        const auto [found, inserted] = m_records.insert(record);
        return { at(found), inserted };
    }
    /// The index `key` gets, built where new from `make(index)`, which must give a record equal to `key`.
    template<std::invocable<handle_type> Factory>
    std::pair<handle_type, bool> insert(const T& key, Factory make)
    {
        const auto [found, inserted] =
            m_records.insert_with(key, [&make](std::size_t position) { return make(handle_type(static_cast<std::uint32_t>(position))); });
        return { at(found), inserted };
    }
    [[nodiscard]] handle_type find(const T& record) const
    {
        const auto found = m_records.find(record);
        return found == m_records.end() ? handle_type() : at(found);
    }
    [[nodiscard]] bool contains(const T& record) const { return m_records.contains(record); }
    [[nodiscard]] const T& operator[](handle_type at) const { return m_records[at.value]; }
    [[nodiscard]] size_type size() const { return m_records.size(); }
    [[nodiscard]] bool empty() const { return m_records.empty(); }
    [[nodiscard]] size_type memory_usage() const { return m_records.memory_usage(); }
    void clear() { m_records.clear(); }

private:
    [[nodiscard]] handle_type at(typename Records::iterator it) const { return handle_type(static_cast<std::uint32_t>(it - m_records.begin())); }

    Records m_records;
};

/// A pool of records serialized by cista into a byte arena, each distinct one once: the stored record is one block
/// of bytes, its vectors and strings inside it. Declared here, so that the registry can name it; defined in
/// urd/repr/arena.hpp, which urd/flat.hpp includes.
template<typename T, bool ThreadSafe = false>
class ArenaInterner;

}  // namespace urd::repr

namespace urd
{

/// Records that are blocks of bytes already, kept whole in an array (moved as it grows) or in segments (Stable).
template<typename T, bool ThreadSafe, bool Stable>
    requires(std::is_trivially_copyable_v<T> && hashable<T>)
struct container_traits<repr::RecordInterner<T, ThreadSafe, Stable>> :
    properties_of<handles,
                  unique,
                  flat,
                  shrinkable,
                  retains,
                  std::conditional_t<Stable, stable_refs, moving_refs>,
                  std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

/// Records serialized into an arena, which never moves them; the arena's segments are kept on clear.
template<repr::flat_record T, bool ThreadSafe>
struct container_traits<repr::ArenaInterner<T, ThreadSafe>> :
    properties_of<handles, unique, flat, stable_refs, shrinkable, retains, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

}  // namespace urd
