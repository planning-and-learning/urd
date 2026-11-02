#pragma once

#include "urd/elements/index.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/records/records_fwd.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace urd::repr
{

/// A pool of trivially copyable records, each distinct one once, in an indexed set: a record is its own block of
/// bytes already. With Stable the records lie in segments that never move, so [index] hands out references that
/// hold; thread-safe where ThreadSafe (which needs Stable): lookups without a lock, a new record appended under one.
template<typename T, bool ThreadSafe, bool Stable>
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
        const auto found = m_records.position(record);
        return found ? handle_type(static_cast<std::uint32_t>(*found)) : handle_type();
    }
    [[nodiscard]] bool contains(const T& record) const { return m_records.contains(record); }
    [[nodiscard]] const T& operator[](handle_type at) const { return m_records[at.value]; }
    [[nodiscard]] size_type size() const { return m_records.size(); }
    [[nodiscard]] bool empty() const { return m_records.empty(); }
    void clear() { m_records.clear(); }

private:
    friend struct urd::memory_of<RecordInterner>;

    [[nodiscard]] handle_type at(typename Records::iterator it) const { return handle_type(static_cast<std::uint32_t>(it - m_records.begin())); }

    Records m_records;
};

/// A pool of values interned whole: the record interner's indexed set for any hashable value, not stored as bytes.
template<typename T, bool ThreadSafe, bool Stable>
class ValueInterner : public RecordInterner<T, ThreadSafe, Stable>
{
public:
    using RecordInterner<T, ThreadSafe, Stable>::RecordInterner;
};

}  // namespace urd::repr

namespace urd
{

/// Its indexed set's.
template<typename T, bool ThreadSafe, bool Stable>
struct memory_of<repr::RecordInterner<T, ThreadSafe, Stable>>
{
    static std::size_t bytes(const repr::RecordInterner<T, ThreadSafe, Stable>& held) { return memory_usage(held.m_records); }
};
template<typename T, bool ThreadSafe, bool Stable>
struct memory_of<repr::ValueInterner<T, ThreadSafe, Stable>> : memory_of<repr::RecordInterner<T, ThreadSafe, Stable>>
{
};

template<>
struct include<"urd/repr/records/records.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<record_interner_decl, S>
{
    using type = repr::RecordInterner<typename S::element_type>;
};
template<typename S>
struct representation_of<stable_record_interner_decl, S>
{
    using type = repr::RecordInterner<typename S::element_type, false, true>;
};
template<typename S>
struct representation_of<thread_safe_record_interner_decl, S>
{
    using type = repr::RecordInterner<typename S::element_type, true>;
};
template<typename S>
struct representation_of<value_interner_decl, S>
{
    using type = repr::ValueInterner<typename S::element_type>;
};
template<typename S>
struct representation_of<stable_value_interner_decl, S>
{
    using type = repr::ValueInterner<typename S::element_type, false, true>;
};
template<typename S>
struct representation_of<thread_safe_value_interner_decl, S>
{
    using type = repr::ValueInterner<typename S::element_type, true>;
};

}  // namespace urd::detail
