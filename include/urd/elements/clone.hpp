#pragma once
// urd::clone: a deep copy of a builder (or a record), its children with it. A built or held child is owned by its
// handle (a std::unique_ptr), so a builder that holds one is move-only; clone copies it into a slot of its own of the
// same pool, and the builder's other members as they are. Copies are explicit: a builder is never copied implicitly.
#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/handle.hpp"
#include "urd/elements/view.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace urd
{

namespace detail
{
template<typename V>
consteval bool copied();

template<typename R>
consteval bool fields_copied()
{
    return []<typename... Fs>(Fields<Fs...> /*fields*/) { return (detail::copied<member_type_t<Fs::member>>() && ...); }(R::fields());
}

template<template<typename...> typename W, typename... As>
consteval bool alternatives_copied(std::type_identity<W<As...>> /*variant*/)
{ return (detail::copied<As>() && ...); }

/// Whether a V is copied as it is: it is copyable, and so is everything it holds (a std or a flat vector reports itself
/// copyable whatever its elements; a builder or a record is copied as its fields are).
template<typename V>
consteval bool copied()
{
    if constexpr (StdVector<V> || FlatVector<V>)
        return detail::copied<typename V::value_type>();
    else if constexpr (StdOptional<V> || FlatOptional<V>)
        return detail::copied<std::remove_cvref_t<decltype(*std::declval<const V&>())>>();
    else if constexpr (pair_value<V>)
        return detail::copied<typename V::first_type>() && detail::copied<typename V::second_type>();
    else if constexpr (StdVariant<V> || FlatVariant<V>)
        return detail::alternatives_copied(std::type_identity<V> {});
    else if constexpr (Record<V> || Builder<V>)
        return std::copy_constructible<V> && detail::fields_copied<V>();
    else
        return std::copy_constructible<V>;
}
}  // namespace detail

/// Whether a V is copied as it is (detail::copied): what clone copies; anything else it clones member by member.
template<typename V>
concept CopiedAsIs = detail::copied<V>();

/// A view of a record held outside it (a borrowed view: valid while its record is), as a view owning a clone of that
/// record: kept beyond it, read through the same context.
template<typename T, typename C, typename R>
owning_view<T, C, R> clone(const borrowed_view<T, C, R>& view);

/// A deep copy: a value that is copied as it is (CopiedAsIs: an interned child, a key, a builder or a record of
/// copyable members) copied; a builder or a record that is not, field by field (its index as it is); std's and the
/// flat containers of what is not, element by element; a variant (a child: its form kept) its alternative cloned.
template<typename V>
V clone(const V& value)
{
    if constexpr (CopiedAsIs<V>)
        return value;
    else if constexpr (Record<V> || Builder<V>)
    {
        auto result = V();
        if constexpr (HoldsIndex<V>)
            result.index = value.index;
        for_each_field<V>([&]<typename F>() { result.*F::member = urd::clone(value.*F::member); });
        return result;
    }
    else if constexpr (detail::StdVector<V> || FlatVector<V>)
    {
        auto result = V();
        result.reserve(value.size());
        for (const auto& element : value)
            result.emplace_back(urd::clone(element));
        return result;
    }
    else if constexpr (detail::StdOptional<V> || FlatOptional<V>)
    {
        if (!value.has_value())
            return V();
        return V(urd::clone(*value));
    }
    else if constexpr (pair_value<V>)
        return V { urd::clone(value.first), urd::clone(value.second) };
    else if constexpr (detail::StdVariant<V>)
        return std::visit([]<typename A>(const A& alternative) { return V(std::in_place_type<A>, urd::clone(alternative)); }, value);
    else if constexpr (FlatVariant<V>)
        return value.apply([](const auto& alternative) { return V(urd::clone(alternative)); });
    else
        static_assert(false, "urd::clone: a value is copied as it is, or cloned member by member (a record, a builder, a container, a handle)");
}

/// A record held by its handle: a clone of it in a slot of its own of the same Store (or on the heap, where it is
/// owned: urd::make_owned). Declared beside the handle (urd/elements/handle.hpp).
template<typename R>
handle<R> clone(const handle<R>& held)
{
    const auto& release = held.get_deleter();
    if (release.acquire == nullptr)
        throw std::logic_error("urd::clone: a handle of a Store of keyed slots is not cloned (its slot is drawn under a key)");
    auto slot = std::uint64_t { 0 };
    auto* record = static_cast<R*>(release.acquire(release.store, slot));
    auto result = handle<R>(record, urd::release { release.store, slot, release.give_back, release.acquire });
    *result = urd::clone(*held);
    return result;
}

template<typename T, typename C, typename R>
owning_view<T, C, R> clone(const borrowed_view<T, C, R>& view)
{ return owning_view<T, C, R>(urd::clone(urd::record(view)), &view.context()); }

}  // namespace urd
