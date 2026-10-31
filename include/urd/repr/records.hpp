#pragma once

#include "urd/elements.hpp"
#include "urd/index.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/records_fwd.hpp"
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
    [[nodiscard]] size_type memory_usage() const { return m_records.memory_usage(); }
    void clear() { m_records.clear(); }

private:
    [[nodiscard]] handle_type at(typename Records::iterator it) const { return handle_type(static_cast<std::uint32_t>(it - m_records.begin())); }

    Records m_records;
};

}  // namespace urd::repr

namespace urd
{

template<>
struct include<"urd/repr/records.hpp">
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

}  // namespace urd::detail
