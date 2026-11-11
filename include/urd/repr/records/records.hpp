#pragma once

#include "urd/elements/index.hpp"
#include "urd/elements/prehashed.hpp"
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

namespace detail
{

/// A pool over an indexed set: each distinct element once, under the index H of its position. What the interners here
/// and the arena interner (urd/repr/records/arena.hpp) are, each over its own set.
template<typename Records, typename H>
class Interner
{
    using T = typename Records::value_type;

public:
    using value_type = T;
    using handle_type = H;
    using size_type = std::size_t;

    // Each lookup and insert takes the record or a prehashed<T> of it (urd/elements/prehashed.hpp), not hashed again.
    std::pair<handle_type, bool> insert(const T& record) { return inserted(m_records.insert(record)); }
    std::pair<handle_type, bool> insert(const prehashed<T>& key) { return inserted(m_records.insert(key)); }
    /// The index `key` gets, built where new from `make(index)`, which must give a record equal to `key` and must not
    /// touch this pool.
    template<std::invocable<handle_type> Factory>
    std::pair<handle_type, bool> insert(const T& key, Factory make)
    { return insert(prehash(key), std::move(make)); }
    template<std::invocable<handle_type> Factory>
    std::pair<handle_type, bool> insert(const prehashed<T>& key, Factory make)
    {
        return inserted(
            m_records.insert_with(key, [&make](std::size_t position) -> decltype(auto) { return make(handle_type(static_cast<std::uint32_t>(position))); }));
    }
    [[nodiscard]] handle_type find(const T& record) const { return find(prehash(record)); }
    [[nodiscard]] handle_type find(const prehashed<T>& key) const
    {
        const auto found = m_records.position(key);
        return found ? handle_type(static_cast<std::uint32_t>(*found)) : handle_type();
    }
    [[nodiscard]] bool contains(const T& record) const { return m_records.contains(record); }
    [[nodiscard]] bool contains(const prehashed<T>& key) const { return m_records.contains(key); }
    [[nodiscard]] const T& operator[](handle_type at) const { return m_records[at.value]; }
    [[nodiscard]] size_type size() const { return m_records.size(); }
    [[nodiscard]] bool empty() const { return m_records.empty(); }
    void clear() { m_records.clear(); }

private:
    [[nodiscard]] std::pair<handle_type, bool> inserted(std::pair<typename Records::iterator, bool> result) const
    { return { handle_type(static_cast<std::uint32_t>(result.first - m_records.begin())), result.second }; }

    Records m_records;

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept { return Members<&Interner::m_records> {}; }
};

}  // namespace detail

/// A pool of trivially copyable records, each distinct one once, in an indexed set: a record is its own block of
/// bytes already. With Stable the records lie in segments that never move, so [index] hands out references that
/// hold; thread-safe where ThreadSafe (which needs Stable): lookups without a lock, a new record appended under one.
/// H is the index it hands out (urd::pool<V, H>).
template<typename T, bool ThreadSafe, bool Stable, typename H>
class RecordInterner : public detail::Interner<IndexedSet<T, std::uint32_t, ThreadSafe, Stable>, H>
{
};

/// A pool of values interned whole: the record interner's indexed set for any hashable value, not stored as bytes.
template<typename T, bool ThreadSafe, bool Stable, typename H>
class ValueInterner : public detail::Interner<IndexedSet<T, std::uint32_t, ThreadSafe, Stable>, H>
{
};

}  // namespace urd::repr

template<>
struct urd::include<"urd/repr/records/records.hpp">
{
};

namespace urd::detail
{

template<typename S>
struct representation_of<record_interner_decl, S>
{
    using type = repr::RecordInterner<typename S::element_type, false, false, typename S::handle_type>;
};
template<typename S>
struct representation_of<stable_record_interner_decl, S>
{
    using type = repr::RecordInterner<typename S::element_type, false, true, typename S::handle_type>;
};
template<typename S>
struct representation_of<thread_safe_record_interner_decl, S>
{
    using type = repr::RecordInterner<typename S::element_type, true, true, typename S::handle_type>;
};
template<typename S>
struct representation_of<value_interner_decl, S>
{
    using type = repr::ValueInterner<typename S::element_type, false, false, typename S::handle_type>;
};
template<typename S>
struct representation_of<stable_value_interner_decl, S>
{
    using type = repr::ValueInterner<typename S::element_type, false, true, typename S::handle_type>;
};
template<typename S>
struct representation_of<thread_safe_value_interner_decl, S>
{
    using type = repr::ValueInterner<typename S::element_type, true, true, typename S::handle_type>;
};

}  // namespace urd::detail
