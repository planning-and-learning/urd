#pragma once

#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/data.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/less.hpp"

#include <algorithm>
#include <array>
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

template<typename T, typename C, typename V>
struct view_mixin<builder<T, C>, V> : view_mixin<T, V>
{
};

/// The record a view V reads, as V declares it (record_type): an index_view's data<T>, or T where T has no stored record
/// (a Store of plain values); a borrowed, owning or shared view's R. A view whose context keeps its records in a layout
/// of its own (bit-packed, a trie) reads data<T> all the same, field by field: whatever the layout, the key is the
/// handle and data<T> the schema.
template<typename V>
using record_of = typename std::remove_cvref_t<V>::record_type;

/// Whether a context C hands out the record of a key K whole: context[key].
template<typename C, typename K>
concept HandsOutRecords = requires(const C& context, const K& key) { context[key]; };

/// Whether a context C reads the field Member of the record of a key K by the key: context.field<Member>(key), where
/// it keeps its records in a layout of its own. It hands the field out as its views read it: resolved through itself
/// (urd::resolve), by value.
template<typename C, typename K, auto Member>
concept ReadsFieldsByKey = requires(const C& context, const K& key) { context.template field<Member>(key); };

namespace detail
{
/// The way to a view's record whole, for urd itself (member<>, map_children): private to the views, whose friend this
/// is. A view hands out its members resolved; its stored form, with indices where records are and the layout of its
/// fields, stays inside urd.
struct view_access
{
    /// The whole record: an index_view's in its context, where it hands it out (HandsOutRecords: data<T>, or a plain
    /// T); a borrowed, owning or shared view's own (data<T>, builder<T, C>, or a plain T).
    template<typename V>
    [[nodiscard]] static const typename V::record_type& record(const V& view)
    { return view.record(); }
};
}  // namespace detail

template<typename M>
struct resolution;

/// Whether a member of type M is written from a readout of type R (resolution<M>::stored).
template<typename M, typename R>
concept stores = requires(const R& readout) { resolution<M>::stored(readout); };

namespace detail
{
/// How many of the members Ts are written from a readout of type R.
template<typename R, typename... Ts>
inline constexpr std::size_t takers = (std::size_t { 0 } + ... + static_cast<std::size_t>(stores<Ts, R>));
}  // namespace detail

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

    /// Written only from an M: no conversion.
    template<std::same_as<M> R>
    static M stored(const R& readout)
    { return readout; }
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

    /// Written from the index itself, or from a view of the element (its index()).
    template<std::same_as<index<U>> R>
    static index<U> stored(const R& readout) noexcept
    { return readout; }

    template<typename V>
        requires std::same_as<std::remove_cvref_t<decltype(std::declval<const V&>().index())>, index<U>>
    static index<U> stored(const V& view) noexcept
    { return view.index(); }
};

/// A variant (std's or cista's) as the std::variant of its alternatives resolved. An empty one (a cista variant not
/// assigned, a std::variant valueless) throws std::bad_variant_access. Each alternative is constructed by its index:
/// std::variant's converting constructor would resolve its overloads over every alternative, for every alternative.
template<template<typename...> typename V, typename... Ts>
    requires variant_value<V<Ts...>>
struct resolution<V<Ts...>>
{
    /// Whether A is one of the alternatives.
    template<typename A>
    static constexpr bool is_alternative = (std::same_as<A, Ts> || ...);

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

    /// Written from one of its alternatives, as it is; from a readout of one alternative, or from a variant of such
    /// readouts: each to its own alternative, else to the one alternative whose stored takes it, whichever its position
    /// (none or several is a compile error).
    template<typename R>
        requires(is_alternative<R> || variant_value<R> || detail::takers<R, Ts...> == 1)
    static V<Ts...> stored(const R& readout)
    {
        if constexpr (!is_alternative<R> && variant_value<R>)
            return visit([](const auto& alternative) { return stored_alternative(alternative); }, readout);
        else
            return stored_alternative(readout);
    }

private:
    template<typename A>
    static V<Ts...> stored_alternative(const A& alternative)
    {
        if constexpr (is_alternative<A>)
            return V<Ts...>(alternative);
        else
        {
            static_assert(detail::takers<A, Ts...> == 1, "urd::resolution<variant>::stored: exactly one alternative takes the readout");
            constexpr auto takes = std::array<bool, sizeof...(Ts)> { stores<Ts, A>... };
            using Target = boost::mp11::mp_at_c<boost::mp11::mp_list<Ts...>, static_cast<std::size_t>(std::ranges::find(takes, true) - takes.begin())>;
            return V<Ts...>(resolution<Target>::stored(alternative));
        }
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

    /// Written from an optional readout (none: none), or from a readout of its value.
    template<typename R>
    static M stored(const R& readout)
    {
        using E = optional_value_t<M>;
        if constexpr (optional_value<R>)
            return readout.has_value() ? M(resolution<E>::stored(*readout)) : M();
        else
            return M(resolution<E>::stored(readout));
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

    /// Written from a pair of readouts, each as its member.
    template<pair_value R>
    static M stored(const R& readout)
    { return M { resolution<typename M::first_type>::stored(readout.first), resolution<typename M::second_type>::stored(readout.second) }; }
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

    /// Written from a range of readouts, element by element.
    template<std::ranges::input_range R>
    static M stored(const R& readout)
    {
        using E = std::ranges::range_value_t<M>;
        auto result = M();
        if constexpr (std::ranges::sized_range<const R> && std::ranges::sized_range<M>
                      && requires(M& stored) { stored.reserve(std::ranges::range_size_t<M> {}); })
            result.reserve(static_cast<std::ranges::range_size_t<M>>(std::ranges::size(readout)));
        for (const auto& element : readout)
            result.push_back(resolution<E>::stored(element));
        return result;
    }
};

namespace detail
{
/// What the four views share (V: the view; K: the key of its mixin): the context, a const C* the context must outlive;
/// the record, V's record() (private: urd reads it through detail::view_access); its members resolved through the
/// context, member<&T::m>() (an index_view reads its own, by its key where its context reads so); the mixin's
/// accessors; and the identity: what V identifies its record by (V's identity()) and the number of its context
/// (urd::layer), so that equal views are the same element wherever they were made.
template<typename V, typename T, typename C, typename K>
class view_base : public view_mixin<K, V>, public compared_by_identity<V>
{
public:
    using value_type = T;

    [[nodiscard]] const C& context() const noexcept { return *m_context; }

    /// A member of the record, resolved in place through the context (urd::resolve): the record is held (borrowed,
    /// owning and shared views); an index_view reads its own (index_view::member).
    template<auto Member>
    [[nodiscard]] decltype(auto) member() const
    { return urd::resolve(view_access::record(static_cast<const V&>(*this)).*Member, *m_context); }

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

/// A T through a context C, held as its index K: read through the context, context[index] (a T, or the stored record
/// data<T>), and identified by the index. What a context makes of its indices, context.view(index); the only view that
/// names its index, index(). K is urd::index<T>, or a handle of another shape that the context resolves (a key of
/// several parts: a row's pool and its position there).
template<typename T, typename C, typename K>
class index_view : public detail::view_base<index_view<T, C, K>, T, C, T>
{
    friend detail::view_base<index_view, T, C, T>;
    friend detail::view_access;

public:
    /// Its record: data<T> (named only here, so that a view of a record not yet complete may be declared).
    using record_type = data<T>;

    index_view(K at, const C* context) noexcept : detail::view_base<index_view, T, C, T>(context), m_at(at) {}

    [[nodiscard]] K index() const noexcept { return m_at; }

    /// A member of its record, resolved through its context: in place, where the context hands out the record of its
    /// key (HandsOutRecords); as the context reads it by the key, resolved and by value, where it keeps its records in a
    /// layout of its own (ReadsFieldsByKey).
    template<auto Member>
    [[nodiscard]] decltype(auto) member() const
    {
        if constexpr (HandsOutRecords<C, K>)
            return urd::resolve(this->context()[m_at].*Member, this->context());
        else if constexpr (ReadsFieldsByKey<C, K, Member>)
            return this->context().template field<Member>(m_at);
        else
            static_assert(false, "urd: a context hands out a key's record (context[key]) or reads its fields by the key (context.field<Member>(key))");
    }

private:
    /// Its record whole, where its context hands it out (context[key]).
    [[nodiscard]] const record_type& record() const
        requires HandsOutRecords<C, K>
    { return this->context()[m_at]; }

    [[nodiscard]] const K& identity() const noexcept { return m_at; }

    K m_at;
};

/// A T through a context C, held as a record R (data<T>, or builder<T, C>) outside the context, which it points at: the
/// record must outlive the view. What urd::resolve makes of a record held in place. Read and identified by the record.
template<typename T, typename C, typename R>
class borrowed_view : public detail::view_base<borrowed_view<T, C, R>, T, C, R>
{
    friend detail::view_base<borrowed_view, T, C, R>;
    friend detail::view_access;

public:
    using record_type = R;

    borrowed_view(const R* record, const C* context) noexcept : detail::view_base<borrowed_view, T, C, R>(context), m_record(record) {}

private:
    [[nodiscard]] const record_type& record() const noexcept { return *m_record; }

    [[nodiscard]] const R& identity() const noexcept { return *m_record; }

    const R* m_record;
};

/// A T through a context C, holding its record R (data<T>, or builder<T, C>) by value: a record computed rather than
/// stored. Read and identified by the record.
template<typename T, typename C, typename R>
class owning_view : public detail::view_base<owning_view<T, C, R>, T, C, R>
{
    friend detail::view_base<owning_view, T, C, R>;
    friend detail::view_access;

public:
    using record_type = R;

    owning_view(R record, const C* context) noexcept(std::is_nothrow_move_constructible_v<R>) :
        detail::view_base<owning_view, T, C, R>(context),
        m_record(std::move(record))
    {
    }

private:
    [[nodiscard]] const record_type& record() const noexcept { return m_record; }

    [[nodiscard]] const R& identity() const noexcept { return m_record; }

    R m_record;
};

/// A T through a context C, sharing its record R (data<T>, or builder<T, C>): released with the last copy. Read and
/// identified by the record.
template<typename T, typename C, typename R>
class shared_view : public detail::view_base<shared_view<T, C, R>, T, C, R>
{
    friend detail::view_base<shared_view, T, C, R>;
    friend detail::view_access;

public:
    using record_type = R;

    shared_view(std::shared_ptr<const R> record, const C* context) noexcept : detail::view_base<shared_view, T, C, R>(context), m_record(std::move(record)) {}

private:
    [[nodiscard]] const record_type& record() const noexcept { return *m_record; }

    [[nodiscard]] const R& identity() const noexcept { return *m_record; }

    std::shared_ptr<const R> m_record;
};

/// A stored record held in place (inside a composition, or given to urd::resolve): the view its context makes of it,
/// where it makes one (context.view(record): a record read through a context of its own, reached from the context of
/// its parent), else its borrowed_view through the context. Never a temporary.
template<typename T>
struct resolution<data<T>>
{
    static_assert(stored_record<T>, "urd::data<T> must be Serializable: cista_members() returning urd::Members<...>");

    template<typename C>
    static auto of(const data<T>& member, const C& context) noexcept
    {
        if constexpr (requires { context.view(member); })
            return context.view(member);
        else
            return borrowed_view<T, C> { &member, &context };
    }

    template<typename C>
    static void of(const data<T>&& member, const C& context) = delete;
};

/// A builder read through a context: its borrowed_view through its own context C, where the context is one; else
/// the view the context makes of it (context.view(builder): a child read through a context of its own, reached from
/// the context of its parent). Never a temporary.
template<typename T, typename C>
struct resolution<builder<T, C>>
{
    template<typename P>
    static auto of(const builder<T, C>& member, const P& context)
    {
        if constexpr (std::convertible_to<const P*, const C*>)
            return borrowed_view<T, C, builder<T, C>> { &member, &context };
        else
            return context.view(member);
    }

    template<typename P>
    static void of(const builder<T, C>&& member, const P& context) = delete;
};

/// A child's record not interned yet (urd::child), held by its handle: its record resolved. Never a temporary.
template<typename T>
struct resolution<handle<data<T>>>
{
    template<typename P>
    static auto of(const handle<data<T>>& member, const P& context)
    { return resolution<data<T>>::of(*member, context); }

    template<typename P>
    static void of(const handle<data<T>>&& member, const P& context) = delete;
};

/// A built child (urd::child) held by its handle: its builder resolved. Never a temporary.
template<typename T, typename C>
struct resolution<handle<builder<T, C>>>
{
    template<typename P>
    static auto of(const handle<builder<T, C>>& member, const P& context)
    { return resolution<builder<T, C>>::of(*member, context); }

    template<typename P>
    static void of(const handle<builder<T, C>>&& member, const P& context) = delete;
};

}  // namespace urd

namespace std::ranges
{
/// Its iterators hold what they read (the range's iterators and the function): they outlive it.
template<typename R, typename F>
inline constexpr bool enable_borrowed_range<urd::transformed_range<R, F>> = true;
}  // namespace std::ranges
