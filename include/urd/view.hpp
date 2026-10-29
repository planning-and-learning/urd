#pragma once

#include "urd/index.hpp"
#include "urd/semantics/concepts.hpp"

#include <cstddef>
#include <ranges>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

/// Whether C resolves indices of T: `context[index<T>]` is the record. A positional set or a pool of Ts does, and so
/// does a Family of such Stores, for every member type, and a layer of them, for the records of every layer below.
template<typename C, typename T>
concept resolves = requires(const C& context, index<T> at) {
    { context[at] } -> std::convertible_to<const T&>;
};

/// Hook for the named accessors of a record's views, one line each over get<>: `template<typename V> struct
/// urd::view_mixin<Atom, V> { auto predicate() const { return static_cast<const V&>(*this).template get<&Atom::predicate>(); } };`.
template<typename T, typename V>
struct view_mixin
{
};

namespace detail
{

/// A context along a chain of layers numbers itself; a view's identity then includes the number.
template<typename C>
concept numbered = requires(const C& context) {
    { context.layer() } -> std::convertible_to<std::size_t>;
};

}  // namespace detail

/// How a view resolves a variant: `resolved<C>` is the variant of the alternatives resolved through C, and `visit`
/// applies a function to the alternative held. Given for std::variant here, for a flat variant in urd/flat.hpp.
template<typename V>
struct variant_traits;

namespace detail
{

template<typename T>
concept a_variant = requires { typename variant_traits<T>::template resolved<int>; };

}  // namespace detail

template<typename M, typename C>
[[nodiscard]] decltype(auto) resolve(const M& member, const C& context);

/// The type resolve(member, context) gives for a member of type M.
template<typename M, typename C>
using resolved_t = std::remove_cvref_t<decltype(urd::resolve(std::declval<const M&>(), std::declval<const C&>()))>;

template<typename... Ts>
struct variant_traits<std::variant<Ts...>>
{
    template<typename C>
    using resolved = std::variant<resolved_t<Ts, C>...>;
    template<typename F>
    static decltype(auto) visit(F&& function, const std::variant<Ts...>& held)
    {
        return std::visit(std::forward<F>(function), held);
    }
};

/// A record's member, as a view sees it: an index<U> as a view<U, C> where the context resolves U, else the index;
/// a range of such indices as a range of views; characters as a string_view; another contiguous range as a span; a
/// variant as the variant of its alternatives resolved; anything else as it is.
template<typename M, typename C>
[[nodiscard]] decltype(auto) resolve(const M& member, const C& context)
{
    if constexpr (is_index<M>)
    {
        if constexpr (resolves<C, typename M::element_type>)
            return view<typename M::element_type, C>(member, context);
        else
            return member;
    }
    else if constexpr (detail::a_variant<M>)
    {
        using resolved = typename variant_traits<M>::template resolved<C>;
        return variant_traits<M>::visit([&context](const auto& alternative) -> resolved { return resolved(urd::resolve(alternative, context)); }, member);
    }
    else if constexpr (std::ranges::input_range<M>)
    {
        using element = std::ranges::range_value_t<M>;
        if constexpr (std::ranges::contiguous_range<M> && std::same_as<element, char>)
            return std::string_view(std::ranges::data(member), std::ranges::size(member));
        else if constexpr (is_index<element> && resolves<C, typename element::element_type>)
            return std::views::transform(member, [&context](element at) { return view<typename element::element_type, C>(at, context); });
        else if constexpr (std::ranges::contiguous_range<M>)
            return std::span<const element>(std::ranges::data(member), std::ranges::size(member));
        else
            return (member);
    }
    else
        return (member);
}

/// A reference to an element held by index: the index and the context that resolves it, nothing else. It reads the
/// record through the context, resolves the record's members through the same context (get<&T::m>()), and compares
/// and hashes by index, and by the layer's number where the context is a layer, so that equal views are the same
/// element wherever they were made. It borrows the context, which must outlive it.
template<typename T, typename C>
class view : public view_mixin<T, view<T, C>>
{
    static_assert(resolves<C, T>, "urd::view<T, C>: C must resolve indices of T: context[index<T>] gives the record");

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
    {
        return urd::resolve(record().*Member, *m_context);
    }

    /// The identity: the index, and the layer's number where the context has one.
    [[nodiscard]] auto identifying_members() const noexcept
    {
        if constexpr (detail::numbered<C>)
            return std::tuple(m_index, static_cast<std::size_t>(m_context->layer()));
        else
            return std::tuple(m_index);
    }
    friend bool operator==(const view& lhs, const view& rhs) noexcept { return lhs.identifying_members() == rhs.identifying_members(); }

private:
    index<T> m_index;
    const C* m_context;
};

}  // namespace urd
