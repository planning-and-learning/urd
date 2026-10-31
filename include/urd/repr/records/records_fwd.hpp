#pragma once

// Pools of records, each one block of bytes read by index: declared, with what the registry reads of them. Their
// definitions are urd/repr/records/records.hpp (the record interner) and urd/repr/records/arena.hpp (the arena interner).

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

#include <type_traits>

namespace urd::repr
{

/// A pool of trivially copyable records, each distinct one once, in an indexed set (urd/repr/records/records.hpp).
template<typename T, bool ThreadSafe = false, bool Stable = ThreadSafe>
class RecordInterner;

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
    static constexpr urd::kind kind = urd::kind::records;
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, packing::flat, memory::retains, referencing::none, threading::none });
    static constexpr role_tags relies_on { .element = property_set::of({ copying::copies_as_bytes, hashability::can_hash }) };
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
    static constexpr urd::kind kind = urd::kind::records;
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, packing::flat, memory::retains, referencing::stable_refs, threading::none });
    static constexpr role_tags relies_on { .element = property_set::of({ copying::copies_as_bytes, hashability::can_hash }) };
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
    static constexpr urd::kind kind = urd::kind::records;
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, packing::flat, memory::retains, referencing::stable_refs, threading::concurrent });
    static constexpr role_tags relies_on { .element = property_set::of({ copying::copies_as_bytes, hashability::can_hash }) };
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
    static constexpr urd::kind kind = urd::kind::records;
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, packing::flat, referencing::stable_refs, memory::retains, threading::none });
    static constexpr role_tags relies_on { .element = property_set::of({ hashability::can_hash, member_naming::names_members }) };
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
    static constexpr urd::kind kind = urd::kind::records;
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, packing::flat, referencing::stable_refs, memory::retains, threading::concurrent });
    static constexpr role_tags relies_on { .element = property_set::of({ hashability::can_hash, member_naming::names_members }) };
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
