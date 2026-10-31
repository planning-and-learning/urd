#pragma once

// Pools of records, each one block of bytes read by index: declared, with what the registry reads of them. Their
// definitions are urd/repr/records.hpp (the record interner) and urd/repr/arena.hpp (the arena interner).

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

#include <type_traits>

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P record_interner(const T& t, bool concurrent_, bool stable)
{
    if (!t.trivially_copyable || !t.hashable)
        return {};
    return property_set::of({ tag::handles, tag::unique, tag::flat, tag::retains, references(stable), threads(concurrent_) });
}

constexpr P arena_interner(const T& t, bool concurrent_)
{
    if (!t.serializable || !t.hashable)
        return {};
    return property_set::of({ tag::handles, tag::unique, tag::flat, tag::stable_refs, tag::retains, threads(concurrent_) });
}

}  // namespace urd::detail::guarantees


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

namespace detail
{

/// record interner (trivially copyable records in an indexed set)
struct record_interner_decl
{
    static constexpr std::string_view id = "record_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        const auto held = guarantees::record_interner(f.element, false, false);
        if (!held)
            return {};
        return offer_of(urd::kind::records, *held, f);
    }
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
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

namespace detail
{

/// record interner over segments that never move (no lock)
struct stable_record_interner_decl
{
    static constexpr std::string_view id = "stable_record_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        const auto held = guarantees::record_interner(f.element, false, true);
        if (!held)
            return {};
        return offer_of(urd::kind::records, *held, f);
    }
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
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

namespace detail
{

/// record interner, thread-safe
struct thread_safe_record_interner_decl
{
    static constexpr std::string_view id = "thread_safe_record_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/records.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        const auto held = guarantees::record_interner(f.element, true, true);
        if (!held)
            return {};
        return offer_of(urd::kind::records, *held, f);
    }
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
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

namespace detail
{

/// arena interner (records serialized by cista into a byte arena)
struct arena_interner_decl
{
    static constexpr std::string_view id = "arena_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/arena.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        const auto held = guarantees::arena_interner(f.element, false);
        if (!held)
            return {};
        return offer_of(urd::kind::records, *held, f);
    }
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
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

namespace detail
{

/// arena interner, thread-safe
struct thread_safe_arena_interner_decl
{
    static constexpr std::string_view id = "thread_safe_arena_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/arena.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        const auto held = guarantees::arena_interner(f.element, true);
        if (!held)
            return {};
        return offer_of(urd::kind::records, *held, f);
    }
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
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

}  // namespace urd
