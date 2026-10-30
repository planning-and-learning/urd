#pragma once

// Pools of records, each one block of bytes read by index: declared, with what the registry reads of them. Their
// definitions are urd/repr/records.hpp (the record interner) and urd/repr/arena.hpp (the arena interner), which
// urd/use/records.hpp includes.

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <type_traits>

namespace urd::repr
{

/// A record cista can serialize into the arena: hashable (its identity), and naming its members for cista
/// (cista_members() gives urd::Members<&T::a, …>). Never a Store, which asking would complete.
template<typename T>
concept flat_record = !is_store<T> && hashable<T> && requires { T::cista_members(); };

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
inline constexpr auto defined_in<repr::RecordInterner<T, ThreadSafe, Stable>> = detail::fixed_string("urd/use/records.hpp");
template<typename T, bool ThreadSafe>
inline constexpr auto defined_in<repr::ArenaInterner<T, ThreadSafe>> = detail::fixed_string("urd/use/records.hpp");

}  // namespace urd
