#pragma once

#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/data.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/less.hpp"

#include <boost/mp11.hpp>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
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
    requires(std::same_as<std::remove_cvref_t<decltype(context[at])>, data<T>> && stored_record<T>) || std::convertible_to<decltype(context[at]), const T&>;
    { context.layer() } -> std::same_as<std::size_t>;
    { context.view(at) } -> std::same_as<index_view<T, C>>;
};

/// Hook for the named accessors of a record's views, one line each over member<>: `template<typename V> struct
/// urd::view_mixin<Atom, V> { auto predicate() const { return static_cast<const V&>(*this).template member<&urd::record_of<V>::predicate>(); } };`
/// (record_of<V>: the record, named through V, so that it may still be incomplete where the mixin is declared). A mixin
/// must not declare member itself, which the view's member<&T::m>() would hide.
template<typename T, typename V>
struct view_mixin
{
};

/// A record view's accessors are its record's mixin (view_mixin<R, V>): a stored record and a builder read as T,
/// unless a mixin of their own says otherwise.
template<typename T, typename V>
struct view_mixin<data<T>, V> : view_mixin<T, V>
{
};
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
/// that a resolution which borrows it (a record or builder: urd::borrowed_view) refuses a temporary.
template<typename M, typename C>
[[nodiscard]] decltype(auto) resolve(M&& member, const C& context)
{ return resolution<std::remove_cvref_t<M>>::of(std::forward<M>(member), context); }

/// Whether resolving changes M: an index, or a composition holding one (urd::resolution below).
template<typename M>
concept resolving = !requires { resolution<M>::identity; };

/// The type resolve(member, context) gives for a member of type M.
template<typename M, typename C>
using resolved_t = std::remove_cvref_t<decltype(urd::resolve(std::declval<const M&>(), std::declval<const C&>()))>;

/// An index as the view its context makes (pointing at the layer that owns the element, where the context is a layer):
/// a record's context must resolve every index type the record holds.
template<typename U>
struct resolution<index<U>>
{
    template<typename C>
    static auto of(index<U> at, const C& context)
    { return context.view(at); }
};

/// A variant (std's or cista's) as the std::variant of its alternatives resolved. An empty one (a cista variant not
/// assigned, a std::variant valueless) throws std::bad_variant_access. Each alternative is constructed by its index:
/// std::variant's converting constructor would resolve its overloads over every alternative, for every alternative.
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
        return visit(
            [&context](const auto& alternative)
            {
                constexpr auto at = boost::mp11::mp_find<boost::mp11::mp_list<Ts...>, std::remove_cvref_t<decltype(alternative)>>::value;
                return resolved(std::in_place_index<at>, urd::resolve(alternative, context));
            },
            member);
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

/// A range R read through a function F: each element x read as f(x). Held are R's iterators (a container's, or a
/// borrowed view's: a span, iota, the set bits of a bitset) and the function, so that, unlike std::views::transform,
/// its iterators do not point into it: they outlive it, and it is a borrowed range. It traverses as R does (random
/// access where R is); its end is R's sentinel where R has one. R must outlive it where R is not a view. F is a small
/// semiregular function object, as an iterator must be default-constructible and assignable: a lambda without captures,
/// a struct naming what it reads through, or a function with its context (urd::transform(range, context, f)); each
/// iterator holds its own copy. A named type, so that it can be spelled (an alias, a parameter) before F can be called
/// (urd::resolved_range): F is semiregular, but that is checked where one is made (urd::transform), not in the template
/// head (a context named while incomplete would be instantiated too early).
template<std::ranges::input_range R, typename F>
class transformed_range : public std::ranges::view_interface<transformed_range<R, F>>
{
    static_assert(!std::ranges::view<R> || std::ranges::borrowed_range<R>,
                  "urd::transformed_range: a view read through a function must be a borrowed range (its iterators outlive it)");
    using base_iterator = std::ranges::iterator_t<const R>;
    using base_sentinel = std::ranges::sentinel_t<const R>;
    using held_range = decltype(std::ranges::subrange(std::declval<const R&>()));
    held_range m_range;
    F m_function {};

public:
    class iterator
    {
        base_iterator m_it {};
        F m_function {};

    public:
        using reference = std::invoke_result_t<const F&, std::iter_reference_t<base_iterator>>;
        using difference_type = std::iter_difference_t<base_iterator>;
        using value_type = std::remove_cvref_t<reference>;
        using iterator_concept = std::conditional_t<
            std::random_access_iterator<base_iterator>,
            std::random_access_iterator_tag,
            std::conditional_t<std::bidirectional_iterator<base_iterator>,
                               std::bidirectional_iterator_tag,
                               std::conditional_t<std::forward_iterator<base_iterator>, std::forward_iterator_tag, std::input_iterator_tag>>>;

        iterator() = default;
        iterator(base_iterator it, const F& function) : m_it(std::move(it)), m_function(function) {}

        [[nodiscard]] reference operator*() const { return std::invoke(m_function, *m_it); }
        [[nodiscard]] reference operator[](difference_type n) const
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
            m_it += n;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic): the base iterator, a pointer where the range read is a flat_vector
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
        friend bool operator==(const iterator& it, const base_sentinel& end)
            requires(!std::same_as<base_sentinel, base_iterator>)
        { return it.m_it == end; }
        friend auto operator<=>(const iterator& lhs, const iterator& rhs)
            requires std::random_access_iterator<base_iterator>
        { return (lhs.m_it - rhs.m_it) <=> difference_type { 0 }; }
    };

    transformed_range(const R& range, F function) : m_range(range), m_function(std::move(function)) {}

    /// The range read through (its elements as they are), as std's views name it, and the function.
    [[nodiscard]] const held_range& base() const noexcept { return m_range; }
    [[nodiscard]] const F& function() const noexcept { return m_function; }
    /// The context it is read through, where its function names one (urd::resolver).
    [[nodiscard]] const auto& context() const noexcept
        requires requires(const F& function) { *function.context; }
    { return *m_function.context; }

    [[nodiscard]] iterator begin() const { return { m_range.begin(), m_function }; }
    [[nodiscard]] iterator end() const
        requires std::ranges::common_range<const R>
    { return { m_range.end(), m_function }; }
    [[nodiscard]] base_sentinel end() const
        requires(!std::ranges::common_range<const R>)
    { return m_range.end(); }
    [[nodiscard]] auto size() const
        requires std::ranges::sized_range<const R>
    { return std::ranges::size(m_range); }
    /// The element at a position, checked as std's containers do (std::out_of_range).
    [[nodiscard]] decltype(auto) at(std::size_t position) const
        requires std::ranges::random_access_range<const R> && std::ranges::sized_range<const R>
    {
        if (position >= std::ranges::size(m_range))
            throw std::out_of_range("urd::transformed_range::at: position out of range");
        return (*this)[static_cast<std::ranges::range_difference_t<const R>>(position)];
    }
};

/// `range` read through `function` (urd::transformed_range): a borrowed std::views::transform. An rvalue range only
/// where it is borrowed (a view whose iterators outlive it); a container must outlive the result.
template<std::ranges::input_range R, typename F>
    requires std::ranges::borrowed_range<R> && std::semiregular<std::decay_t<F>>
[[nodiscard]] transformed_range<std::remove_cvref_t<R>, std::decay_t<F>> transform(R&& range, F&& function)
{ return { std::forward<R>(range), std::forward<F>(function) }; }

namespace detail
{
/// `f(context, x)` as a function of `x`: the state a function reads (a pointer, a pair), held beside a function that
/// captures nothing, so that both stay semiregular, as transformed_range's iterators need.
template<std::semiregular C, std::semiregular F>
struct with_context
{
    C context {};
    F function {};

    template<typename X>
    [[nodiscard]] decltype(auto) operator()(X&& x) const
    { return std::invoke(function, context, std::forward<X>(x)); }
};
}  // namespace detail

/// `range` read through `function(context, x)`: urd::transform where the function needs state. `function` captures
/// nothing (a lambda without captures is semiregular); `context` is what it would have captured.
template<std::ranges::input_range R, std::semiregular C, typename F>
    requires std::ranges::borrowed_range<R> && std::semiregular<std::decay_t<F>>
[[nodiscard]] auto transform(R&& range, C context, F&& function)
{ return transform(std::forward<R>(range), detail::with_context<C, std::decay_t<F>> { std::move(context), std::forward<F>(function) }); }

/// What a resolved_range reads its elements through: urd::resolve with the context, each element passed on as the
/// range gives it (so that a stored record, which its view borrows, is refused as a temporary).
template<typename C>
struct resolver
{
    const C* context = nullptr;

    template<typename M>
    [[nodiscard]] auto operator()(M&& member) const
    { return urd::resolve(std::forward<M>(member), *context); }
};

/// A range of resolving elements read through a context: each element resolved (urd::resolve) when it is read; the
/// transformed_range whose function is the context's resolver (the context borrowed: context() gives it). A named
/// type, so that it can be spelled (an alias, a parameter) while the context is still incomplete.
template<typename M, typename C>
using resolved_range = transformed_range<M, resolver<C>>;

/// A transformed range is a value range where the range it reads is one (or is a view: a span, a decoded row, iota):
/// Hash, EqualTo and Less follow from its elements.
template<typename R, typename F>
inline constexpr bool value_range<transformed_range<R, F>> = value_range<R> || std::ranges::view<R>;

/// A range of resolving elements (indices, or compositions of them, at any depth) as its resolved_range.
template<std::ranges::input_range M>
    requires resolving<std::ranges::range_value_t<M>>
struct resolution<M>
{
    template<typename C>
    static resolved_range<M, C> of(const M& member, const C& context)
    { return { member, resolver<C> { &context } }; }
};

namespace detail
{
/// What the four views share (V: the view; K: the key of its mixin): the context, a const C* the context must outlive;
/// the record, V's data(); its members resolved through the context, member<&T::m>(); the mixin's accessors; and the
/// identity: what V identifies its record by (V's identity()) and the number of its context (urd::layer), so that
/// equal views are the same element wherever they were made.
template<typename V, typename T, typename C, typename K>
class view_base : public view_mixin<K, V>, public compared_by_identity<V>
{
public:
    using value_type = T;

    [[nodiscard]] const C& context() const noexcept { return *m_context; }
    /// A member of the record, resolved through the context (urd::resolve).
    template<auto Member>
    [[nodiscard]] decltype(auto) member() const
    { return urd::resolve(static_cast<const V&>(*this).data().*Member, *m_context); }

    /// The identity: what identifies the record, and the number of the context (a context without a number gives
    /// none).
    static constexpr auto identifying_members() noexcept
        requires requires(const C& context) { context.layer(); }
    {
        return Members<[](const V& v) -> decltype(auto) { return v.identity(); }, [](const V& v) { return v.context().layer(); }> {};
    }

protected:
    explicit view_base(const C* context) noexcept : m_context(context) {}

private:
    const C* m_context;
};

}  // namespace detail

/// A T through a context C, held as its index: read through the context, context[index] (a T, or the stored record
/// data<T>), and identified by the index. What a context makes of its indices, context.view(index); the only view that
/// names its index, index().
template<typename T, typename C>
class index_view : public detail::view_base<index_view<T, C>, T, C, T>
{
    friend detail::view_base<index_view, T, C, T>;

public:
    index_view(urd::index<T> at, const C* context) noexcept : detail::view_base<index_view, T, C, T>(context), m_at(at) {}

    [[nodiscard]] urd::index<T> index() const noexcept { return m_at; }
    [[nodiscard]] decltype(auto) data() const { return this->context()[m_at]; }

private:
    [[nodiscard]] const urd::index<T>& identity() const noexcept { return m_at; }

    urd::index<T> m_at;
};

/// A T through a context C, held as a record R (data<T>, or builder<T>) outside the context, which it points at: the
/// record must outlive the view. What urd::resolve makes of a record held in place. Read and identified by the record.
template<typename T, typename C, typename R>
class borrowed_view : public detail::view_base<borrowed_view<T, C, R>, T, C, R>
{
    friend detail::view_base<borrowed_view, T, C, R>;

public:
    borrowed_view(const R* record, const C* context) noexcept : detail::view_base<borrowed_view, T, C, R>(context), m_record(record) {}

    [[nodiscard]] const R& data() const noexcept { return *m_record; }

private:
    [[nodiscard]] const R& identity() const noexcept { return *m_record; }

    const R* m_record;
};

/// A T through a context C, holding its record R (data<T>, or builder<T>) by value: a record computed rather than
/// stored. Read and identified by the record.
template<typename T, typename C, typename R>
class owning_view : public detail::view_base<owning_view<T, C, R>, T, C, R>
{
    friend detail::view_base<owning_view, T, C, R>;

public:
    owning_view(R record, const C* context) noexcept(std::is_nothrow_move_constructible_v<R>) :
        detail::view_base<owning_view, T, C, R>(context),
        m_record(std::move(record))
    {
    }

    [[nodiscard]] const R& data() const noexcept { return m_record; }

private:
    [[nodiscard]] const R& identity() const noexcept { return m_record; }

    R m_record;
};

/// A T through a context C, sharing its record R (data<T>, or builder<T>): released with the last copy. Read and
/// identified by the record.
template<typename T, typename C, typename R>
class shared_view : public detail::view_base<shared_view<T, C, R>, T, C, R>
{
    friend detail::view_base<shared_view, T, C, R>;

public:
    shared_view(std::shared_ptr<const R> record, const C* context) noexcept : detail::view_base<shared_view, T, C, R>(context), m_record(std::move(record)) {}

    [[nodiscard]] const R& data() const noexcept { return *m_record; }

private:
    [[nodiscard]] const R& identity() const noexcept { return *m_record; }

    std::shared_ptr<const R> m_record;
};

/// A stored record held in place (inside a composition, or given to urd::resolve): its borrowed_view. Never a
/// temporary.
template<typename T>
struct resolution<data<T>>
{
    static_assert(stored_record<T>, "urd::data<T> must be Serializable: cista_members() returning urd::Members<...>");

    template<typename C>
    static borrowed_view<T, C> of(const data<T>& member, const C& context) noexcept
    { return { &member, &context }; }
    template<typename C>
    static void of(const data<T>&& member, const C& context) = delete;
};

/// A builder read through a context: its borrowed_view. Never a temporary.
template<typename T>
struct resolution<builder<T>>
{
    template<typename C>
    static borrowed_view<T, C, builder<T>> of(const builder<T>& member, const C& context) noexcept
    { return { &member, &context }; }
    template<typename C>
    static void of(const builder<T>&& member, const C& context) = delete;
};

}  // namespace urd

namespace std::ranges
{
/// Its iterators hold what they read (the range's iterators and the function): they outlive it.
template<typename R, typename F>
inline constexpr bool enable_borrowed_range<urd::transformed_range<R, F>> = true;
}  // namespace std::ranges
