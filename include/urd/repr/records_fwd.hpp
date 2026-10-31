#pragma once

// Pools of records, each one block of bytes read by index: declared, with what the registry reads of them. Their
// definitions are urd/repr/records.hpp (the record interner) and urd/repr/arena.hpp (the arena interner).

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/element_concepts.hpp"
#include "urd/traits.hpp"

#include <type_traits>

namespace urd::repr
{

/// A pool of trivially copyable records, each distinct one once, in an indexed set (urd/repr/records.hpp).
template<typename T, bool ThreadSafe = false, bool Stable = ThreadSafe>
class RecordInterner;

/// A pool of records serialized by cista into a byte arena, each distinct one once (urd/repr/arena.hpp).
template<typename T, bool ThreadSafe = false>
class ArenaInterner;

}  // namespace urd::repr

namespace urd
{

/// Records that are blocks of bytes already, kept whole in an array (moved as it grows) or in segments (Stable).
template<typename T, bool ThreadSafe, bool Stable>
    requires(detail::trivially_copyable<T> && hashable<T>)
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

template<typename T, bool ThreadSafe, bool Stable>
inline constexpr auto defined_in<repr::RecordInterner<T, ThreadSafe, Stable>> = detail::fixed_string("urd/repr/records.hpp");
template<typename T, bool ThreadSafe>
inline constexpr auto defined_in<repr::ArenaInterner<T, ThreadSafe>> = detail::fixed_string("urd/repr/arena.hpp");

}  // namespace urd

namespace urd
{

/// record_interner.
template<typename T>
struct container_costs<repr::RecordInterner<T, false>>
{
    static constexpr cost insert_row = costs::lin_row_t;
    static constexpr cost find_row = costs::lin_row_t;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2.5;
};

/// stable_record_interner.
template<typename T>
struct container_costs<repr::RecordInterner<T, false, true>>
{
    static constexpr cost insert_row = costs::lin_row_t;
    static constexpr cost find_row = costs::lin_row_t;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2.6;
};

/// thread_safe_record_interner.
template<typename T>
struct container_costs<repr::RecordInterner<T, true>>
{
    static constexpr cost insert_row = costs::lin_row_t;
    static constexpr cost find_row = costs::lin_row_t;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2.7;
};

/// arena_interner.
template<typename T>
struct container_costs<repr::ArenaInterner<T, false>>
{
    static constexpr cost insert_row = costs::lin_row_t;
    static constexpr cost find_row = costs::lin_row_t;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2.5;
};

/// thread_safe_arena_interner.
template<typename T>
struct container_costs<repr::ArenaInterner<T, true>>
{
    static constexpr cost insert_row = costs::lin_row_t;
    static constexpr cost find_row = costs::lin_row_t;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2.7;
};

}  // namespace urd

namespace urd
{

template<typename T>
struct container_synopsis<repr::RecordInterner<T, false>>
{
    static constexpr std::string_view id = "record_interner";
    static constexpr std::string_view description = "record interner (trivially copyable records in an indexed set)";
    static constexpr std::string_view holds = "each distinct record once, as one block of bytes";
};

template<typename T>
struct container_synopsis<repr::RecordInterner<T, false, true>>
{
    static constexpr std::string_view id = "stable_record_interner";
    static constexpr std::string_view description = "record interner over segments that never move (no lock)";
    static constexpr std::string_view holds = "each distinct record once, in segments that never move";
};

template<typename T>
struct container_synopsis<repr::RecordInterner<T, true>>
{
    static constexpr std::string_view id = "thread_safe_record_interner";
    static constexpr std::string_view description = "record interner, thread-safe";
    static constexpr std::string_view holds = "each distinct record once; inserts under a lock";
};

template<typename T>
struct container_synopsis<repr::ArenaInterner<T, false>>
{
    static constexpr std::string_view id = "arena_interner";
    static constexpr std::string_view description = "arena interner (records serialized by cista into a byte arena)";
    static constexpr std::string_view holds = "each distinct record once, serialized into an arena";
};

template<typename T>
struct container_synopsis<repr::ArenaInterner<T, true>>
{
    static constexpr std::string_view id = "thread_safe_arena_interner";
    static constexpr std::string_view description = "arena interner, thread-safe";
    static constexpr std::string_view holds = "each distinct record once, serialized; inserts under a lock";
};

}  // namespace urd
