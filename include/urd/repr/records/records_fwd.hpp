#pragma once

// Pools of records and values, each distinct one once, read by index: declared, with what the registry reads of them.
// Records copied as bytes are stored flat (record interners); other values are interned as values (value interners). Their
// definitions are urd/repr/records/records.hpp (the record interner) and urd/repr/records/arena.hpp (the arena interner).

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <type_traits>

namespace urd::repr
{

/// A pool of trivially copyable records, each distinct one once, in an indexed set (urd/repr/records/records.hpp).
template<typename T, bool ThreadSafe = false, bool Stable = ThreadSafe>
class RecordInterner;

/// A pool of values interned whole, each distinct one once, in an indexed set: the record interner's engine for values
/// not stored as bytes (urd/repr/records/records.hpp).
template<typename T, bool ThreadSafe = false, bool Stable = ThreadSafe>
class ValueInterner;

/// A pool of records serialized by cista into a byte arena, each distinct one once (urd/repr/records/arena.hpp).
template<typename T, bool ThreadSafe = false>
class ArenaInterner;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// record interner (trivially copyable records in an indexed set)
struct record_interner_decl
{
    static constexpr std::string_view id = "record_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records/records.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides =
        property_set::of({ handling::handles, uniqueness::unique, packing::flat, memory::retains, referencing::none, threading::none });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::copies_as_bytes, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert_row = costs::lin_row;
        static constexpr cost find_row = costs::lin_row;
        static constexpr cost access = costs::one;
        static constexpr double words = 2.5;
    };
    static constexpr std::string_view description = "record interner (trivially copyable records in an indexed set)";
    static constexpr std::string_view holds = "each distinct record once, as one block of bytes";
};
/// value interner (distinct values in an indexed set): the record interner's container for values not stored as bytes (no flat)
struct value_interner_decl
{
    static constexpr std::string_view id = "value_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records/records.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, memory::retains, referencing::none, threading::none });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::can_hash, trait::whole_value>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = record_interner_decl::operation_costs;
    static constexpr std::string_view description = "value interner (distinct values in an indexed set)";
    static constexpr std::string_view holds = "each distinct value once";
};
template<typename T>
struct declaration_of<repr::ValueInterner<T, false>>
{
    using declaration = value_interner_decl;
    static constexpr role_tags roles = element_roles<T>();
};
template<typename T>
struct declaration_of<repr::RecordInterner<T, false>>
{
    using declaration = record_interner_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// record interner over segments that never move (no lock)
struct stable_record_interner_decl
{
    static constexpr std::string_view id = "stable_record_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records/records.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides =
        property_set::of({ handling::handles, uniqueness::unique, packing::flat, memory::retains, referencing::stable_refs, threading::none });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::copies_as_bytes, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert_row = costs::lin_row;
        static constexpr cost find_row = costs::lin_row;
        static constexpr cost access = costs::one;
        static constexpr double words = 2.6;
    };
    static constexpr std::string_view description = "record interner over segments that never move (no lock)";
    static constexpr std::string_view holds = "each distinct record once, in segments that never move";
};
/// value interner over segments that never move (no lock): the record interner's container for values not stored as bytes (no flat)
struct stable_value_interner_decl
{
    static constexpr std::string_view id = "stable_value_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records/records.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides =
        property_set::of({ handling::handles, uniqueness::unique, memory::retains, referencing::stable_refs, threading::none });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::can_hash, trait::whole_value>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = stable_record_interner_decl::operation_costs;
    static constexpr std::string_view description = "value interner over segments that never move (no lock)";
    static constexpr std::string_view holds = "each distinct value once, in segments that never move";
};
template<typename T>
struct declaration_of<repr::ValueInterner<T, false, true>>
{
    using declaration = stable_value_interner_decl;
    static constexpr role_tags roles = element_roles<T>();
};
template<typename T>
struct declaration_of<repr::RecordInterner<T, false, true>>
{
    using declaration = stable_record_interner_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// record interner, thread-safe
struct thread_safe_record_interner_decl
{
    static constexpr std::string_view id = "thread_safe_record_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records/records.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides =
        property_set::of({ handling::handles, uniqueness::unique, packing::flat, memory::retains, referencing::stable_refs, threading::concurrent });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::copies_as_bytes, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert_row = costs::lin_row;
        static constexpr cost find_row = costs::lin_row;
        static constexpr cost access = costs::one;
        static constexpr double words = 2.7;
    };
    static constexpr std::string_view description = "record interner, thread-safe";
    static constexpr std::string_view holds = "each distinct record once; inserts under a lock";
};
/// value interner, thread-safe: the record interner's container for values not stored as bytes (no flat)
struct thread_safe_value_interner_decl
{
    static constexpr std::string_view id = "thread_safe_value_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records/records.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides =
        property_set::of({ handling::handles, uniqueness::unique, memory::retains, referencing::stable_refs, threading::concurrent });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::can_hash, trait::whole_value>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = thread_safe_record_interner_decl::operation_costs;
    static constexpr std::string_view description = "value interner, thread-safe";
    static constexpr std::string_view holds = "each distinct value once; inserts under a lock";
};
template<typename T>
struct declaration_of<repr::ValueInterner<T, true>>
{
    using declaration = thread_safe_value_interner_decl;
    static constexpr role_tags roles = element_roles<T>();
};
template<typename T>
struct declaration_of<repr::RecordInterner<T, true>>
{
    using declaration = thread_safe_record_interner_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// arena interner (records serialized by cista into a byte arena)
struct arena_interner_decl
{
    static constexpr std::string_view id = "arena_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records/arena.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides =
        property_set::of({ handling::handles, uniqueness::unique, packing::flat, referencing::stable_refs, memory::retains, threading::none });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::can_hash, trait::names_members>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert_row = costs::lin_row;
        static constexpr cost find_row = costs::lin_row;
        static constexpr cost access = costs::one;
        static constexpr double words = 2.5;
    };
    static constexpr std::string_view description = "arena interner (records serialized by cista into a byte arena)";
    static constexpr std::string_view holds = "each distinct record once, serialized into an arena";
};
template<typename T>
struct declaration_of<repr::ArenaInterner<T, false>>
{
    using declaration = arena_interner_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// arena interner, thread-safe
struct thread_safe_arena_interner_decl
{
    static constexpr std::string_view id = "thread_safe_arena_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records/arena.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides =
        property_set::of({ handling::handles, uniqueness::unique, packing::flat, referencing::stable_refs, memory::retains, threading::concurrent });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::can_hash, trait::names_members>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert_row = costs::lin_row;
        static constexpr cost find_row = costs::lin_row;
        static constexpr cost access = costs::one;
        static constexpr double words = 2.7;
    };
    static constexpr std::string_view description = "arena interner, thread-safe";
    static constexpr std::string_view holds = "each distinct record once, serialized; inserts under a lock";
};
template<typename T>
struct declaration_of<repr::ArenaInterner<T, true>>
{
    using declaration = thread_safe_arena_interner_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

}  // namespace urd
