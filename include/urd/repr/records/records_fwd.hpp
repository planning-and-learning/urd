#pragma once

// Pools of records and values, each distinct one once, read by index: declared, with what the registry reads of them.
// Records copied as bytes are stored flat (record interners); other values are interned as values (value interners). Their
// definitions are urd/repr/records/records.hpp (the record interner) and urd/repr/records/arena.hpp (the arena interner).

#include "urd/cost.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <type_traits>

namespace urd::repr
{

/// A pool of trivially copyable records, each distinct one once, in an indexed set (urd/repr/records/records.hpp).
template<typename T, bool ThreadSafe = false, bool Stable = ThreadSafe, typename H = urd::index<T>>
class RecordInterner;

/// A pool of values interned whole, each distinct one once, in an indexed set: the record interner's engine for values
/// not stored as bytes (urd/repr/records/records.hpp).
template<typename T, bool ThreadSafe = false, bool Stable = ThreadSafe, typename H = urd::index<T>>
class ValueInterner;

/// A pool of records serialized by cista into a byte arena, each distinct one once (urd/repr/records/arena.hpp).
template<typename T, bool ThreadSafe = false, typename H = urd::index<T>>
class ArenaInterner;

}  // namespace urd::repr

namespace urd::detail
{

// The interners' declarations. A variant derives from the one it varies and says only what differs: what it provides
// (urd::with puts a tag in its category), what it costs in memory, what it is.

struct record_interner_decl
{
    static constexpr std::string_view id = "record_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records/records.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides =
        property_set::of({ handling::handles, uniqueness::unique, packing::flat, memory::retains, referencing::none, threading::none });
    static constexpr property_set relies_on = relies(traits<role::element, trait::copies_as_bytes, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert_row, costs::lin_row>,
                                       cost_entry<operation::find_row, costs::lin_row>,
                                       cost_entry<operation::access, costs::one>>;
    static constexpr double words = 2.5;
    static constexpr std::string_view description = "record interner (trivially copyable records in an indexed set)";
    static constexpr std::string_view holds = "each distinct record once, as one block of bytes";
};
struct stable_record_interner_decl : record_interner_decl
{
    static constexpr std::string_view id = "stable_record_interner";
    static constexpr property_set provides = with(record_interner_decl::provides, { referencing::stable_refs });
    static constexpr double words = 2.6;
    static constexpr std::string_view description = "record interner over segments that never move (no lock)";
    static constexpr std::string_view holds = "each distinct record once, in segments that never move";
};
struct thread_safe_record_interner_decl : stable_record_interner_decl
{
    static constexpr std::string_view id = "thread_safe_record_interner";
    static constexpr property_set provides = with(stable_record_interner_decl::provides, { threading::concurrent });
    static constexpr double words = 2.7;
    static constexpr std::string_view description = "record interner, thread-safe";
    static constexpr std::string_view holds = "each distinct record once; inserts under a lock";
};

/// The record interner's container for values not stored as bytes (no flat): the same costs and memory.
struct value_interner_decl : record_interner_decl
{
    static constexpr std::string_view id = "value_interner";
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, memory::retains, referencing::none, threading::none });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash, trait::whole_value>());
    static constexpr std::string_view description = "value interner (distinct values in an indexed set)";
    static constexpr std::string_view holds = "each distinct value once";
};
struct stable_value_interner_decl : value_interner_decl
{
    static constexpr std::string_view id = "stable_value_interner";
    static constexpr property_set provides = with(value_interner_decl::provides, { referencing::stable_refs });
    static constexpr double words = stable_record_interner_decl::words;
    static constexpr std::string_view description = "value interner over segments that never move (no lock)";
    static constexpr std::string_view holds = "each distinct value once, in segments that never move";
};
struct thread_safe_value_interner_decl : stable_value_interner_decl
{
    static constexpr std::string_view id = "thread_safe_value_interner";
    static constexpr property_set provides = with(stable_value_interner_decl::provides, { threading::concurrent });
    static constexpr double words = thread_safe_record_interner_decl::words;
    static constexpr std::string_view description = "value interner, thread-safe";
    static constexpr std::string_view holds = "each distinct value once; inserts under a lock";
};

/// Records serialized into an arena: the record interner's kind, costs and memory.
struct arena_interner_decl : record_interner_decl
{
    static constexpr std::string_view id = "arena_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records/arena.hpp" };
    static constexpr property_set provides =
        property_set::of({ handling::handles, uniqueness::unique, packing::flat, referencing::stable_refs, memory::retains, threading::none });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash, trait::names_members>());
    static constexpr std::string_view description = "arena interner (records serialized by cista into a byte arena)";
    static constexpr std::string_view holds = "each distinct record once, serialized into an arena";
};
struct thread_safe_arena_interner_decl : arena_interner_decl
{
    static constexpr std::string_view id = "thread_safe_arena_interner";
    static constexpr property_set provides = with(arena_interner_decl::provides, { threading::concurrent });
    static constexpr double words = 2.7;
    static constexpr std::string_view description = "arena interner, thread-safe";
    static constexpr std::string_view holds = "each distinct record once, serialized; inserts under a lock";
};

/// Which declaration a class is: by what it is built with (thread-safe needs stable).
template<typename T, typename H>
struct declaration_of<repr::RecordInterner<T, false, false, H>>
{
    using declaration = record_interner_decl;
    static constexpr property_set roles = roles_of<urd::pool<T, H>>();
};
template<typename T, typename H>
struct declaration_of<repr::RecordInterner<T, false, true, H>> : declaration_of<repr::RecordInterner<T, false, false, H>>
{
    using declaration = stable_record_interner_decl;
};
template<typename T, typename H>
struct declaration_of<repr::RecordInterner<T, true, true, H>> : declaration_of<repr::RecordInterner<T, false, false, H>>
{
    using declaration = thread_safe_record_interner_decl;
};
template<typename T, typename H>
struct declaration_of<repr::ValueInterner<T, false, false, H>> : declaration_of<repr::RecordInterner<T, false, false, H>>
{
    using declaration = value_interner_decl;
};
template<typename T, typename H>
struct declaration_of<repr::ValueInterner<T, false, true, H>> : declaration_of<repr::RecordInterner<T, false, false, H>>
{
    using declaration = stable_value_interner_decl;
};
template<typename T, typename H>
struct declaration_of<repr::ValueInterner<T, true, true, H>> : declaration_of<repr::RecordInterner<T, false, false, H>>
{
    using declaration = thread_safe_value_interner_decl;
};
template<typename T, typename H>
struct declaration_of<repr::ArenaInterner<T, false, H>> : declaration_of<repr::RecordInterner<T, false, false, H>>
{
    using declaration = arena_interner_decl;
};
template<typename T, typename H>
struct declaration_of<repr::ArenaInterner<T, true, H>> : declaration_of<repr::RecordInterner<T, false, false, H>>
{
    using declaration = thread_safe_arena_interner_decl;
};

}  // namespace urd::detail
