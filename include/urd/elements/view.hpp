#pragma once

#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/data.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/less.hpp"

#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

/// Whether C resolves indices of T: `context[index<T>]` is the record (a T, or the stored record data<T>, which must
/// be Serializable), and C has its number (urd::layer). A positional set or a pool of Ts does, and so does a layer of
/// them, for the records of every layer below. And it makes the views of its indices, `context.view(index<T>)`: the one
/// place that says which context a view is through (a layer points it at the layer that owns the element); a context
/// made of several Stores writes one per type.
template<typename C, typename T>
concept resolves = requires(const C& context, index<T> at) {
    requires(std::same_as<std::remove_cvref_t<decltype(context[at])>, data<T>> && stored_record<T>)
                || std::convertible_to<decltype(context[at]), const T&>;
    { context.layer() } -> std::same_as<std::size_t>;
    { context.view(at) } -> std::same_as<view<T, C>>;
};

/// Hook for the named accessors of a record's views, one line each over get<>: `template<typename V> struct
/// urd::view_mixin<Atom, V> { auto predicate() const { return static_cast<const V&>(*this).template get<&urd::record_of<V>::predicate>(); } };`
/// (record_of<V>: the record, named through V, so that it may still be incomplete where the mixin is declared). The
/// view's get<&T::m>() hides any get a mixin would declare: name accessors otherwise.
template<typename T, typename V>
struct view_mixin
{
};

/// A builder's views read as the views of what it builds, unless its mixin says otherwise.
template<typename T, typename V>
struct view_mixin<builder<T>, V> : view_mixin<T, V>
{
};

/// The record a view V reads (its data()).
template<typename V>
using record_of = std::remove_cvref_t<decltype(std::declval<const V&>().data())>;


/// How a view resolves a member of type M through its context (urd::resolve): `resolution<M>::of(member, context)`.
/// As it is, unless a specialization for M says otherwise: given below for indices, ranges, pairs, variants and
/// optionals (std's and cista's: urd/elements/concepts.hpp).
template<typename M>
struct resolution
{
    static constexpr bool identity = true;

    // The member lives in the record, which outlives the call: never a temporary.
    template<typename C>
    static const M& of(const M& member, const C& /*context*/) noexcept
    { return member; }  // NOLINT(bugprone-return-const-ref-from-parameter)
};

/// A record's member, as a view sees it: what its resolution makes of it. The member is passed on as it is given, so
/// that a resolution which borrows it (a record or builder: urd::borrowed) refuses a temporary.
template<typename M, typename C>
[[nodiscard]] decltype(auto) resolve(M&& member, const C& context)
{ return resolution<std::remove_cvref_t<M>>::of(std::forward<M>(member), context); }

/// Whether resolving changes M: an index, or a composition holding one (urd::resolution below).
template<typename M>
concept resolving = !requires { resolution<M>::identity; };

/// The type resolve(member, context) gives for a member of type M.
template<typename M, typename C>
using resolved_t = std::remove_cvref_t<decltype(urd::resolve(std::declval<const M&>(), std::declval<const C&>()))>;

/// A handle (an index, or a composite one) as the view its context makes (pointing at the layer that owns the element,
/// where the context is a layer): a record's context must resolve every handle type the record holds.
template<handle H>
struct resolution<H>
{
    template<typename C>
    static auto of(H at, const C& context)
    { return context.view(at); }
};

/// A variant (std's or cista's) as the std::variant of its alternatives resolved. An empty one (a cista variant not
/// assigned, a std::variant valueless) throws std::bad_variant_access.
template<template<typename...> typename V, typename... Ts>
    requires variant_value<V<Ts...>>
struct resolution<V<Ts...>>
{
    template<typename C>
    static auto of(const V<Ts...>& member, const C& context)
    {
        using resolved = std::variant<resolved_t<Ts, C>...>;
        if (member.index() >= sizeof...(Ts))
            throw std::bad_variant_access();
        return visit([&context](const auto& alternative) { return resolved(urd::resolve(alternative, context)); }, member);
    }
};

/// An optional as the std::optional of its value resolved.
template<optional_value M>
struct resolution<M>
{
    template<typename C>
    static auto of(const M& member, const C& context)
    {
        using resolved = std::optional<resolved_t<std::remove_cvref_t<decltype(*member)>, C>>;
        return member.has_value() ? resolved(urd::resolve(*member, context)) : resolved();
    }
};

/// A pair (std's or cista's) as the std::pair of its members resolved.
template<pair_value M>
struct resolution<M>
{
    template<typename C>
    static auto of(const M& member, const C& context)
    {
        using resolved = std::pair<resolved_t<typename M::first_type, C>, resolved_t<typename M::second_type, C>>;
        return resolved(urd::resolve(member.first, context), urd::resolve(member.second, context));
    }
};

/// A range of resolving elements read through a context: each element resolved (urd::resolve) when it is read; held
/// are the range's iterators (a container's, or a borrowed view's: a span, a decoded row) and the context, both
/// borrowed. A named type, so that it can be spelled (an alias, a parameter) while the context is still incomplete.
template<typename M, typename C>
class resolved_range : public std::ranges::view_interface<resolved_range<M, C>>
{
    static_assert(!std::ranges::view<M> || std::ranges::borrowed_range<M>,
                  "urd::resolved_range: a view read through a context must be a borrowed range (its iterators outlive it)");
    using base_iterator = std::ranges::iterator_t<const M>;
    using held_range = decltype(std::ranges::subrange(std::declval<const M&>()));
    held_range m_range;
    const C* m_context;

public:
    class iterator
    {
        base_iterator m_it {};
        const C* m_context = nullptr;

    public:
        using difference_type = std::iter_difference_t<base_iterator>;
        using value_type = resolved_t<std::ranges::range_value_t<M>, C>;
        using iterator_concept = std::conditional_t<std::random_access_iterator<base_iterator>,
                                                    std::random_access_iterator_tag,
                                                    std::conditional_t<std::bidirectional_iterator<base_iterator>,
                                                                       std::bidirectional_iterator_tag,
                                                                       std::conditional_t<std::forward_iterator<base_iterator>,
                                                                                          std::forward_iterator_tag,
                                                                                          std::input_iterator_tag>>>;

        iterator() = default;
        iterator(base_iterator it, const C* context) : m_it(std::move(it)), m_context(context) {}

        [[nodiscard]] value_type operator*() const { return urd::resolve(*m_it, *m_context); }
        [[nodiscard]] value_type operator[](difference_type n) const
            requires std::random_access_iterator<base_iterator>
        { return *(*this + n); }
        iterator& operator++()
        {
            ++m_it;
            return *this;
        }
        iterator operator++(int)
        {
            auto old = *this;
            ++m_it;
            return old;
        }
        iterator& operator--()
            requires std::bidirectional_iterator<base_iterator>
        {
            --m_it;
            return *this;
        }
        iterator operator--(int)
            requires std::bidirectional_iterator<base_iterator>
        {
            auto old = *this;
            --m_it;
            return old;
        }
        iterator& operator+=(difference_type n)
            requires std::random_access_iterator<base_iterator>
        {
            m_it += n;
            return *this;
        }
        iterator& operator-=(difference_type n)
            requires std::random_access_iterator<base_iterator>
        {
            m_it -= n;
            return *this;
        }
        friend iterator operator+(iterator it, difference_type n)
            requires std::random_access_iterator<base_iterator>
        { return it += n; }
        friend iterator operator+(difference_type n, iterator it)
            requires std::random_access_iterator<base_iterator>
        { return it += n; }
        friend iterator operator-(iterator it, difference_type n)
            requires std::random_access_iterator<base_iterator>
        { return it -= n; }
        friend difference_type operator-(const iterator& lhs, const iterator& rhs)
            requires std::sized_sentinel_for<base_iterator, base_iterator>
        { return lhs.m_it - rhs.m_it; }
        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.m_it == rhs.m_it; }
        friend auto operator<=>(const iterator& lhs, const iterator& rhs)
            requires std::random_access_iterator<base_iterator>
        { return (lhs.m_it - rhs.m_it) <=> difference_type { 0 }; }
    };

    resolved_range(const M& range, const C& context) : m_range(range), m_context(&context) {}

    /// The stored range read through (its elements unresolved), as std's views name it, and the context it is read
    /// through.
    [[nodiscard]] const held_range& base() const noexcept { return m_range; }
    [[nodiscard]] const C& context() const noexcept { return *m_context; }

    [[nodiscard]] iterator begin() const { return { m_range.begin(), m_context }; }
    [[nodiscard]] iterator end() const
        requires std::ranges::common_range<const M>
    { return { m_range.end(), m_context }; }
    [[nodiscard]] auto size() const
        requires std::ranges::sized_range<const M>
    { return std::ranges::size(m_range); }
    /// The element at a position, checked as std's containers do (std::out_of_range).
    [[nodiscard]] auto at(std::size_t position) const
        requires std::ranges::random_access_range<const M> && std::ranges::sized_range<const M>
    {
        if (position >= std::ranges::size(m_range))
            throw std::out_of_range("urd::resolved_range::at: position out of range");
        return (*this)[position];
    }
};

/// A readout is a value range where the range it reads is one (or is a view: a span, a decoded row): Hash, EqualTo
/// and Less follow from its elements, the views.
template<typename M, typename C>
inline constexpr bool value_range<resolved_range<M, C>> = value_range<M> || std::ranges::view<M>;

/// A range of resolving elements (indices, or compositions of them, at any depth) as its resolved_range.
template<std::ranges::input_range M>
    requires resolving<std::ranges::range_value_t<M>>
struct resolution<M>
{
    template<typename C>
    static resolved_range<M, C> of(const M& member, const C& context)
    { return { member, context }; }
};

/// What a view holds, and how it reads its record (h.read(context)): one of four holding types.
/// - A handle (urd::handle: an index, or a library's composite handle): the element interned in the context, which
///   resolves it (context[handle] is the record).
/// - borrowed<R>: a record R stored elsewhere, which outlives the view (a stored record in place, a caller's, a
///   builder). Never a temporary.
/// - shared<R>: a record R the view shares ownership of (std::shared_ptr), released with the last copy.
/// - owned<R>: a record R computed on the fly, which the view owns by value.
/// Each compares and hashes by what identifies it: a handle by itself, a record holder by its record. A record holder
/// names the record it reads (record_type).
template<typename R>
struct borrowed
{
    using record_type = R;
    const R* record;

    borrowed(const R& held) noexcept : record(&held) {}  // NOLINT(google-explicit-constructor): a view is made of the record it borrows
    borrowed(const R&& held) = delete;

    [[nodiscard]] const R& read(const auto& /*context*/) const noexcept { return *record; }
    static constexpr auto identifying_members() noexcept { return Members<[](const borrowed& h) -> const R& { return *h.record; }> {}; }
};

template<typename R>
struct shared
{
    using record_type = R;
    std::shared_ptr<const R> record;

    template<typename P>
        requires std::convertible_to<P, std::shared_ptr<const R>>
    shared(P&& held) noexcept : record(std::forward<P>(held))  // NOLINT(google-explicit-constructor): a view is made of the record it shares
    {
    }

    [[nodiscard]] const R& read(const auto& /*context*/) const noexcept { return *record; }
    static constexpr auto identifying_members() noexcept { return Members<[](const shared& h) -> const R& { return *h.record; }> {}; }
};

template<typename R>
struct owned
{
    using record_type = R;
    R record;

    owned(R held) noexcept(std::is_nothrow_move_constructible_v<R>) : record(std::move(held)) {}  // NOLINT(google-explicit-constructor): a view is made of the record it owns

    [[nodiscard]] const R& read(const auto& /*context*/) const noexcept { return record; }
    static constexpr auto identifying_members() noexcept { return Members<&owned::record> {}; }
};

namespace detail
{
/// The mixin a view's accessors come from: a builder's own where H reads a builder (else T's, view_mixin<builder<T>, V>
/// falling back to it), T's otherwise.
template<typename T, typename H>
struct mixin_key
{
    using type = T;
};
template<typename T, typename H>
    requires is_builder<typename H::record_type>
struct mixin_key<T, H>
{
    using type = typename H::record_type;
};

/// A view of a handle gives it, index(); a view of a record names none, so that the record's mixin may.
template<typename V, typename H, typename M>
struct handle_index : M
{
};
template<typename V, handle H, typename M>
struct handle_index<V, H, M> : M
{
    [[nodiscard]] H index() const noexcept { return static_cast<const V&>(*this).m_held; }
};
}  // namespace detail

/// A T read through a context C it borrows, which must outlive it, holding what reads its record (H, a holding type
/// above: by default the index). It resolves the record's members through the context (get<&T::m>(), the view_mixin
/// accessors: a builder's own, else T's). It compares and hashes by what it holds and the number of its context
/// (urd::layer), so that equal views of an index are the same element wherever they were made. A context makes them:
/// context.view(index), context.view(record).
template<typename T, typename C, typename H>
class view :
    public detail::handle_index<view<T, C, H>, H, view_mixin<typename detail::mixin_key<T, H>::type, view<T, C, H>>>,
    public compared_by_identity<view<T, C, H>>
{
    friend detail::handle_index<view, H, view_mixin<typename detail::mixin_key<T, H>::type, view>>;

public:
    using value_type = T;

    view(H held, const C& context) noexcept(std::is_nothrow_move_constructible_v<H>) : m_held(std::move(held)), m_context(&context) {}

    [[nodiscard]] const C& context() const noexcept { return *m_context; }
    /// The record itself, as the holding type reads it (an index: as the context holds it, a T or the stored record
    /// data<T>).
    [[nodiscard]] decltype(auto) data() const { return m_held.read(*m_context); }
    /// A member of the record, resolved through the context (urd::resolve).
    template<auto Member>
    [[nodiscard]] decltype(auto) get() const
    { return urd::resolve(data().*Member, *m_context); }

    /// The identity: what it holds, and the number of its context (a context without a number gives none).
    static constexpr auto identifying_members() noexcept
        requires requires(const C& context) { context.layer(); }
    {
        return Members<[](const view& v) -> const H& { return v.m_held; }, [](const view& v) { return v.context().layer(); }> {};
    }

private:
    H m_held;
    const C* m_context;
};

/// The views of each holding type.
template<typename T, typename C>
using index_view = view<T, C, index<T>>;
template<typename T, typename C, typename R = data<T>>
using borrowed_view = view<T, C, borrowed<R>>;
template<typename T, typename C, typename R = data<T>>
using shared_view = view<T, C, shared<R>>;
template<typename T, typename C, typename R = data<T>>
using owned_view = view<T, C, owned<R>>;

/// A stored record held in place (inside a composition, or given to urd::resolve): its view, which borrows it.
template<typename T>
struct resolution<data<T>>
{
    static_assert(stored_record<T>, "urd::data<T> must be Serializable: cista_members() returning urd::Members<...>");

    template<typename M, typename C>
        requires std::constructible_from<borrowed<data<T>>, M>
    static borrowed_view<T, C> of(M&& member, const C& context) noexcept
    { return { std::forward<M>(member), context }; }
};

/// A builder read through a context: its view, which borrows it.
template<typename T>
struct resolution<builder<T>>
{
    template<typename M, typename C>
        requires std::constructible_from<borrowed<builder<T>>, M>
    static borrowed_view<T, C, builder<T>> of(M&& member, const C& context) noexcept
    { return { std::forward<M>(member), context }; }
};

}  // namespace urd
