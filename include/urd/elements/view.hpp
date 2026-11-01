#pragma once

#include "urd/elements/concepts.hpp"
#include "urd/elements/index.hpp"

#include <cstddef>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

/// Whether C resolves indices of T: `context[index<T>]` is the record, and C has its number (urd::layer). A positional
/// set or a pool of Ts does, and so does a layer of them, for the records of every layer below. And it makes the views
/// of its indices, `context.view(index<T>)`: the one place that says which context a view is through (a layer points
/// it at the layer that owns the element); a context made of several Stores writes one per type.
template<typename C, typename T>
concept resolves = requires(const C& context, index<T> at) {
    { context[at] } -> std::convertible_to<const T&>;
    { context.layer() } -> std::same_as<std::size_t>;
    { context.view(at) } -> std::same_as<view<T, C>>;
};

/// Hook for the named accessors of a record's views, one line each over get<>: `template<typename V> struct
/// urd::view_mixin<Atom, V> { auto predicate() const { return static_cast<const V&>(*this).template get<&Atom::predicate>(); } };`.
template<typename T, typename V>
struct view_mixin
{
};

/// How a view resolves a member of type M through its context (urd::resolve): `resolution<M>::of(member, context)`.
/// As it is, unless a specialization for M says otherwise: given below for indices, ranges of indices, std::variant
/// and std::optional, and in urd/elements/flat.hpp for cista's variant and optional.
template<typename M>
struct resolution
{
    // The member lives in the record, which outlives the call: never a temporary.
    template<typename C>
    static const M& of(const M& member, const C& /*context*/) noexcept
    { return member; }  // NOLINT(bugprone-return-const-ref-from-parameter)
};

/// A record's member, as a view sees it: what its resolution makes of it.
template<typename M, typename C>
[[nodiscard]] decltype(auto) resolve(const M& member, const C& context)
{ return resolution<M>::of(member, context); }

/// The type resolve(member, context) gives for a member of type M.
template<typename M, typename C>
using resolved_t = std::remove_cvref_t<decltype(urd::resolve(std::declval<const M&>(), std::declval<const C&>()))>;

/// An index as the view its context makes (pointing at the layer that owns the element, where the context is a
/// layer): a record's context must resolve every index type the record holds.
template<typename U>
struct resolution<index<U>>
{
    template<typename C>
    static auto of(index<U> at, const C& context)
    { return context.view(at); }
};

/// A variant as the variant of its alternatives resolved.
template<typename... Ts>
struct resolution<std::variant<Ts...>>
{
    template<typename C>
    static auto of(const std::variant<Ts...>& member, const C& context)
    {
        using resolved = std::variant<resolved_t<Ts, C>...>;
        return std::visit([&context](const auto& alternative) { return resolved(urd::resolve(alternative, context)); }, member);
    }
};

/// An optional as the optional of its value resolved.
template<typename T>
struct resolution<std::optional<T>>
{
    template<typename C>
    static auto of(const std::optional<T>& member, const C& context)
    {
        using resolved = std::optional<resolved_t<T, C>>;
        return member ? resolved(urd::resolve(*member, context)) : resolved();
    }
};

/// A range of indices as a range of views.
template<std::ranges::input_range M>
    requires is_index<std::ranges::range_value_t<M>>
struct resolution<M>
{
    template<typename C>
    static auto of(const M& member, const C& context)
    {
        return std::views::transform(member, [&context](auto at) { return context.view(at); });
    }
};

/// A reference to an element held by index: the index and the context that resolves it, nothing else. It reads the
/// record through the context, resolves the record's members through the same context (get<&T::m>()), and compares
/// and hashes by index and by the number of its context (urd::layer), so that equal views are the same
/// element wherever they were made. It borrows the context, which must outlive it.
template<typename T, typename C>
class view : public view_mixin<T, view<T, C>>
{
public:
    using value_type = T;
    using context_type = C;

    view(index<T> at, const C& context) noexcept : m_index(at), m_context(&context) {}

    [[nodiscard]] index<T> id() const noexcept { return m_index; }
    [[nodiscard]] const C& context() const noexcept { return *m_context; }
    /// The record itself, as the context holds it.
    [[nodiscard]] decltype(auto) record() const { return (*m_context)[m_index]; }
    /// A member of the record, resolved through the context (urd::resolve).
    template<auto Member>
    [[nodiscard]] decltype(auto) get() const
    { return urd::resolve(record().*Member, *m_context); }

    /// The identity: the index, and the number of the Store it was made in (urd::layer).
    [[nodiscard]] auto identifying_members() const noexcept { return std::tuple(m_index, m_context->layer()); }

private:
    index<T> m_index;
    const C* m_context;
};

}  // namespace urd
