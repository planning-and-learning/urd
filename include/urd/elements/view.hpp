#pragma once

#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/data.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/keyed.hpp"
#include "urd/elements/less.hpp"
#include "urd/elements/walk.hpp"
#include "urd/memory.hpp"

#include <algorithm>
#include <array>
#include <boost/mp11.hpp>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
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

/// Whether C resolves indices of T: urd::record_at(context, key<T>) is the record's stored_ref (over a T, or the
/// stored record data<T>), and C has its number (urd::layer). A positional set or a pool of Ts does, and so does a layer of
/// them, for the records of every layer below. And it makes the views of its indices, `context.view(key<T>)`: the one
/// place that says which context a view is through (a layer points it at the layer that owns the element); a context
/// made of several Stores writes one per type.
template<typename C, typename T>
concept resolves = requires(const C& context, key<T> at) {
    { urd::record_at<T>(context, at) } -> std::same_as<stored_ref<T>>;
    { context.layer() } -> std::same_as<std::size_t>;
    { context.view(at) } -> std::same_as<index_view<T, C>>;
};

/// Hook for the named accessors of a record's views, one line each over member<>: `template<typename V> struct
/// urd::view_mixin<Atom, V> { auto predicate() const { return static_cast<const V&>(*this).template member<&urd::record_of<V>::predicate>(); } };`
/// (record_of<V>: the record, named through V, so that it may still be incomplete where the mixin is declared). A mixin
/// must not declare member itself, which the view's member<&T::m>() would hide.
namespace detail
{
/// What a record's view has where its record declares no mixin of its own (urd::DeclaresViewMixin).
struct no_view_mixin
{
};
}  // namespace detail

template<typename T, typename V>
struct view_mixin : detail::no_view_mixin
{
};

/// A record view's accessors are its record's mixin (view_mixin<R, V>): a held record and a builder read as T, unless
/// a mixin of their own says otherwise. An index_view reads as T, whatever form its context hands its record out in
/// (stored_ref<T>): member<>() reads each field from that form (stored_field).
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

namespace detail
{
/// The way to a view's record whole, for urd itself (member<>, map_children): private to the views, whose friend this
/// is. A view hands out its members resolved; its stored form, with indices where records are and the layout of its
/// fields, stays inside urd.
struct view_access
{
    /// The whole record: an index_view's in its context, where it hands it out (HandsOutRecordsBy: data<T>, or a plain
    /// T); a borrowed, owning or shared view's own (data<T>, builder<T, C>, or a plain T).
    template<typename V>
    [[nodiscard]] static const typename V::record_type& record(const V& view)
    { return view.record(); }

    /// An index_view's record, through the stored_ref its context hands out (a record held in place: record_ref).
    template<typename T, typename C, typename K>
    [[nodiscard]] static const typename record_form<T>::type& record(const index_view<T, C, K>& view)
    { return *view.record(); }

    /// A view's record in its stored form: an index_view's stored_ref (a record held in place, or a reader of a record
    /// laid out by its store: its fields read through urd::stored_field), any other view's record itself.
    template<typename T, typename C, typename K>
    [[nodiscard]] static stored_ref<T> stored(const index_view<T, C, K>& view)
    { return view.record(); }

    template<typename V>
    [[nodiscard]] static const typename V::record_type& stored(const V& view)
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

    // The member lives in the record, which outlives the call.
    template<typename C>
    static const M& of(const M& member, const C& /*context*/) noexcept
    { return member; }  // NOLINT(bugprone-return-const-ref-from-parameter)

    /// A member read by value (a stored form of its own reads its fields so: urd::stored_field): by value.
    template<typename C>
    static M of(M&& member, const C& /*context*/) noexcept(std::is_nothrow_move_constructible_v<M>)
    { return std::move(member); }

    /// Written only from an M: no conversion.
    template<std::same_as<M> R>
    static M stored(const R& readout)
    { return readout; }

    /// A list of values written from another range of the same values (a builder's std::vector into a record's
    /// flat_vector), element by element.
    template<std::ranges::input_range R>
        requires(!std::same_as<R, M>) && std::ranges::input_range<M> && std::same_as<std::ranges::range_value_t<R>, std::ranges::range_value_t<M>>
    static M stored(const R& readout)
    {
        auto result = M();
        for (const auto& element : readout)
            result.push_back(element);
        return result;
    }
};

/// A record's member, as a view sees it: what its resolution makes of it. The member is passed on as it is given, so
/// that a resolution which borrows it (a record or builder: urd::borrowed_view) refuses a temporary.
template<typename M, typename C>
[[nodiscard]] decltype(auto) resolve(M&& member, const C& context)
{ return resolution<std::remove_cvref_t<M>>::of(std::forward<M>(member), context); }

/// Whether a view names its element by a key (index()): a view of an interned element (index_view), whatever its key.
template<typename V>
concept Indexed = requires(const V& view) { view.index(); };

/// Whether a view names its element by a key of type K (index()).
template<typename V, typename K>
concept IndexedBy = requires(const V& view) {
    { view.index() } -> std::same_as<K>;
};

/// Whether resolving changes M: an index, or a composition holding one (urd::resolution below).
template<typename M>
concept resolving = !requires { resolution<M>::identity; };

/// The type resolve(member, context) gives for a member of type M.
template<typename M, typename C>
using resolved_t = std::remove_cvref_t<decltype(urd::resolve(std::declval<const M&>(), std::declval<const C&>()))>;

/// A key as the view its context makes (an index: pointing at the layer that owns the element, where the context is a
/// layer; a key of several parts: the context finds the record through them, container by container): a record's
/// context must resolve every key type the record holds. The none key (an empty child, a key not given yet) is the none
/// view: it names no element, so nothing is looked up.
template<typename... Ts>
struct resolution<key<Ts...>>
{
    template<typename C>
    static auto of(key<Ts...> at, const C& context)
    {
        using View = decltype(context.view(at));
        if (at.is_none())
            return View();
        return context.view(at);
    }

    /// Written from the key itself, or from a view of the element (its index()).
    template<std::same_as<key<Ts...>> R>
    static key<Ts...> stored(const R& readout) noexcept
    { return readout; }

    template<IndexedBy<key<Ts...>> V>
    static key<Ts...> stored(const V& view) noexcept
    { return view.index(); }
};

/// Whether A is one of the types Ts (a variant's alternatives).
template<typename A, typename... Ts>
concept Alternative = (std::same_as<A, Ts> || ...);

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

    /// Written from one of its alternatives, as it is; from a readout of one alternative, or from a variant of such
    /// readouts: each to its own alternative, else to the one alternative whose stored takes it, whichever its position
    /// (none or several is a compile error).
    template<typename R>
        requires(Alternative<R, Ts...> || variant_value<R> || detail::takers<R, Ts...> == 1)
    static V<Ts...> stored(const R& readout)
    {
        if constexpr (!Alternative<R, Ts...> && variant_value<R>)
            return visit([](const auto& alternative) { return stored_alternative(alternative); }, readout);
        else
            return stored_alternative(readout);
    }

private:
    template<typename A>
    static V<Ts...> stored_alternative(const A& alternative)
    {
        if constexpr (Alternative<A, Ts...>)
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

namespace detail
{
/// Whether a range's function names the context it reads through (*function.context: urd::resolver).
template<typename F>
concept NamesContext = requires(const F& function) { *function.context; };

/// Whether a context C has layers (layer()): a view through it is identified by its record and its layer.
template<typename C>
concept HasLayers = requires(const C& context) { context.layer(); };

/// Whether a context C makes its own view of a record of T (view(record)).
template<typename C, typename T>
concept ViewsRecords = requires(const C& context, const data<T>& record) { context.view(record); };
}  // namespace detail

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
    /// Its source where it is a container (which outlives it, as its iterators do), not a view: what its ordered
    /// lookups ask.
    const R* m_source = nullptr;

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

    transformed_range(const R& range, F function) : m_range(range), m_function(std::move(function)), m_source(std::ranges::view<R> ? nullptr : &range) {}

    /// The first element not before `key` in its source's order, read through its function: where its source, a sorted
    /// container, looks up K by its order.
    template<typename K>
        requires(!std::ranges::view<R>) && OrderedLookup<R, K>
    [[nodiscard]] iterator lower_bound(const K& key) const
    { return { m_source->lower_bound(key), m_function }; }

    /// The first element after `key` in its source's order, read through its function: where its source looks up K by its
    /// order (with lower_bound, the range of elements equivalent to `key`: a prefix's, with a transparent order that
    /// compares one).
    template<typename K>
        requires(!std::ranges::view<R>) && OrderedLookup<R, K>
    [[nodiscard]] iterator upper_bound(const K& key) const
    { return { m_source->upper_bound(key), m_function }; }

    /// The range read through (its elements as they are), as std's views name it, and the function.
    [[nodiscard]] const held_range& base() const noexcept { return m_range; }

    [[nodiscard]] const F& function() const noexcept { return m_function; }

    /// The context it is read through, where its function names one (urd::resolver).
    [[nodiscard]] const auto& context() const noexcept
        requires detail::NamesContext<F>
    { return *m_function.context; }

    [[nodiscard]] iterator begin() const { return { m_range.begin(), m_function }; }

    /// Its end: an iterator where R is a common range, else R's sentinel. One function choosing, not two overloads
    /// constrained by C and !C: GCC 13 found both viable in a translation unit of runir (an end() over a range of keys
    /// resolved through a context), and the call ambiguous.
    [[nodiscard]] auto end() const
    {
        if constexpr (std::ranges::common_range<const R>)
            return iterator { m_range.end(), m_function };
        else
            return m_range.end();
    }

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
        if constexpr (std::ranges::sized_range<const R> && std::ranges::sized_range<M> && Reserving<M>)
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
        requires detail::HasLayers<C>
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
/// names its index, index(). K is urd::key<T>, or a handle of another shape that the context resolves (a key of
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

    /// The none view: the none key, no context (an empty urd::child). Only assigned to and compared by its key: it has no
    /// record to read.
    index_view() noexcept : detail::view_base<index_view, T, C, T>(nullptr), m_at() {}

    [[nodiscard]] K index() const noexcept { return m_at; }

    /// A member of its record, resolved through its context, which hands out the record of its key (HandsOutRecordsBy).
    template<auto Member>
        requires HandsOutRecordsBy<C, T, K>
    [[nodiscard]] decltype(auto) member() const
    { return urd::resolve(stored_field<Member, stored_ref<T>>::read(urd::record_at<T>(this->context(), m_at)), this->context()); }

private:
    /// Its record as its context hands it out, by value (urd::record_at(context, key)): its stored_ref.
    [[nodiscard]] stored_ref<T> record() const
        requires HandsOutRecordsBy<C, T, K>
    { return urd::record_at<T>(this->context(), m_at); }

    [[nodiscard]] const K& identity() const noexcept { return m_at; }

    K m_at;
};

/// A T through a context C, held as a record R (data<T>, or builder<T, C>) outside the context, which it points at: the
/// record must outlive the view. What urd::resolve makes of a record held in place. Read and identified by the record.
/// The views that own or share their record derive from it (owning_view, shared_view): each is the borrowed view of the
/// record it holds, so whatever reads a borrowed view reads them too.
template<typename T, typename C, typename R>
class borrowed_view : public detail::view_base<borrowed_view<T, C, R>, T, C, R>
{
    friend detail::view_base<borrowed_view, T, C, R>;
    friend detail::view_access;

public:
    using record_type = R;

    borrowed_view(const R* record, const C* context) noexcept : detail::view_base<borrowed_view, T, C, R>(context), m_record(record) {}

protected:
    /// Points at the record a derived view holds (its own copy, after a copy or a move of it).
    void lend(const R* record) noexcept { m_record = record; }

private:
    [[nodiscard]] const record_type& record() const noexcept { return *m_record; }

    [[nodiscard]] const R& identity() const noexcept { return *m_record; }

    const R* m_record;
};

/// A T through a context C, holding its record R (data<T>, or builder<T, C>) by value: a record computed rather than
/// stored. Read and identified by the record, as the borrowed view of it.
template<typename T, typename C, typename R>
class owning_view : public borrowed_view<T, C, R>
{
public:
    using record_type = R;

    owning_view(R record, const C* context) noexcept(std::is_nothrow_move_constructible_v<R>) :
        borrowed_view<T, C, R>(nullptr, context),
        m_owned(std::move(record))
    { this->lend(&m_owned); }

    owning_view(const owning_view& other)
        requires std::copy_constructible<R>
        : borrowed_view<T, C, R>(other), m_owned(other.m_owned)
    { this->lend(&m_owned); }

    owning_view(owning_view&& other) noexcept(std::is_nothrow_move_constructible_v<R>) : borrowed_view<T, C, R>(other), m_owned(std::move(other.m_owned))
    { this->lend(&m_owned); }

    owning_view& operator=(const owning_view& other)
        requires std::copy_constructible<R>
    {
        if (this != &other)
        {
            borrowed_view<T, C, R>::operator=(other);
            m_owned = other.m_owned;
            this->lend(&m_owned);
        }
        return *this;
    }

    owning_view& operator=(owning_view&& other) noexcept(std::is_nothrow_move_assignable_v<R>)
    {
        if (this != &other)
        {
            borrowed_view<T, C, R>::operator=(other);
            m_owned = std::move(other.m_owned);
            this->lend(&m_owned);
        }
        return *this;
    }

    ~owning_view() = default;

private:
    R m_owned;
};

/// A T through a context C, sharing its record R (data<T>, or builder<T, C>): released with the last copy. Read and
/// identified by the record, as the borrowed view of it (the shared record stays where it is: copies point at it).
template<typename T, typename C, typename R>
class shared_view : public borrowed_view<T, C, R>
{
public:
    using record_type = R;

    shared_view(std::shared_ptr<const R> record, const C* context) noexcept : borrowed_view<T, C, R>(record.get(), context), m_shared(std::move(record)) {}

private:
    std::shared_ptr<const R> m_shared;
};

/// A shared view owns no memory of its own: its record is shared.
template<typename T, typename C, typename R>
struct memory_of<shared_view<T, C, R>>
{
    static constexpr std::size_t bytes(const shared_view<T, C, R>& /*held*/) { return 0; }
};

/// An owning view owns what its record owns.
template<typename T, typename C, typename R>
struct memory_of<owning_view<T, C, R>>
{
    static std::size_t bytes(const owning_view<T, C, R>& held) { return memory_usage(detail::view_access::record(held)); }
};

/// A child's handle (a built or held child: urd::child) read in place: as the record it holds, through the reader's
/// context (resolution<builder<T, C>>, resolution<data<T>>). Never a temporary.
template<typename R>
struct resolution<handle<R>>
{
    template<typename P>
    static decltype(auto) of(const handle<R>& member, const P& context)
    { return urd::resolve(*member, context); }

    template<typename P>
    static void of(const handle<R>&& member, const P& context) = delete;
};

/// Whether V is one of urd's views: a T read through a context, held as its index (index_view) or as its record
/// (borrowed_view, owning_view, shared_view).
template<typename V>
inline constexpr bool is_view = false;
template<typename T, typename C, typename K>
inline constexpr bool is_view<index_view<T, C, K>> = true;
template<typename T, typename C, typename R>
inline constexpr bool is_view<borrowed_view<T, C, R>> = true;
template<typename T, typename C, typename R>
inline constexpr bool is_view<owning_view<T, C, R>> = true;
template<typename T, typename C, typename R>
inline constexpr bool is_view<shared_view<T, C, R>> = true;

template<typename V>
concept View = is_view<V>;

/// The kind T a view reads (index_view<T, C, K>, borrowed_view<T, C, R>, …: each view's first parameter).
template<typename V>
struct view_kind;

template<template<typename, typename, typename> typename W, typename T, typename C, typename X>
    requires View<W<T, C, X>>
struct view_kind<W<T, C, X>>
{
    using type = T;
};

/// The context C a view reads through (index_view<T, C, K>, borrowed_view<T, C, R>, …: each view's second parameter).
template<typename V>
struct view_context;

template<template<typename, typename, typename> typename W, typename T, typename C, typename X>
    requires View<W<T, C, X>>
struct view_context<W<T, C, X>>
{
    using type = C;
};

template<typename V>
using view_context_t = typename view_context<std::remove_cvref_t<V>>::type;

/// Whether V is a view of a T, in any form.
template<typename V, typename T>
concept ViewOf = View<std::remove_cvref_t<V>> && std::same_as<typename view_kind<std::remove_cvref_t<V>>::type, T>;

/// Whether every alternative of a std::variant R is a view of a T.
template<typename R, typename T>
inline constexpr bool views_of = false;
template<typename T, typename... Vs>
inline constexpr bool views_of<std::variant<Vs...>, T> = (ViewOf<Vs, T> && ...);

/// Whether R reads a T: a view of it, or a resolved child of it (a variant of the views of its forms), as a member of a
/// T's record and of its builder resolve. What a reader of either takes, through urd::visit_view.
template<typename R, typename T>
concept ReadsAs = ViewOf<R, T> || views_of<std::remove_cvref_t<R>, T>;

/// f of a view.
template<View V, typename F>
decltype(auto) visit_view(const V& view, F&& f)
{ return std::forward<F>(f)(view); }

/// f of the view a resolved child holds (urd::child resolved: a variant of the views of its forms). One reading serves a
/// member that resolves to a view in a record and to a variant of views in its builder.
template<View... Vs, typename F>
decltype(auto) visit_view(const std::variant<Vs...>& views, F&& f)
{ return std::visit(std::forward<F>(f), views); }

namespace detail
{
template<typename V>
using reads_builder = std::bool_constant<is_builder<record_of<V>>>;
}

/// The view of a reading's built form (a builder's view): the reading itself where it is one; the alternative of a
/// resolved child (urd::child resolved: a variant of the views of its forms) that reads its builder,
/// std::bad_variant_access where the child holds another form. What a reader that works on built records takes out of
/// a child, without naming the view.
template<View V>
    requires is_builder<record_of<V>>
V built_view(const V& view)
{ return view; }

template<View... Vs>
auto built_view(const std::variant<Vs...>& views)
{
    using Views = boost::mp11::mp_list<Vs...>;
    using Built = boost::mp11::mp_at<Views, boost::mp11::mp_find_if<Views, detail::reads_builder>>;
    return std::get<Built>(views);
}

namespace detail
{
template<typename M>
consteval bool holds_child();

/// Whether a type argument of a class template instance (an optional, a pair, a variant) holds a child.
template<template<typename...> typename V, typename... Ts>
consteval bool argument_holds_child(std::type_identity<V<Ts...>> /*instance*/)
{ return (detail::holds_child<Ts>() || ...); }

/// Any other type holds none (the more specialized overload above takes a class template instance).
template<typename M>
consteval bool argument_holds_child(std::type_identity<M> /*other*/)
{ return false; }

template<typename T>
inline constexpr bool data_type = false;
template<typename T>
inline constexpr bool data_type<data<T>> = true;

/// Whether T is a record, data<U>.
template<typename T>
concept Data = data_type<T>;

/// The tag of a record type: T of data<T>.
template<typename R>
struct tag_of_data;

template<typename T>
struct tag_of_data<data<T>>
{
    using type = T;
};

/// Whether a record's fields hold a child.
template<typename T>
consteval bool record_holds_child()
{
    if constexpr (HasFields<data<T>>)
        return []<typename... Fs>(Fields<Fs...> /*fields*/) { return (detail::holds_child<member_type_t<Fs::member>>() || ...); }(data<T>::fields());
    else
        return false;
}

/// Whether a member of type M holds a child (urd::child: an index of a stored record), anywhere in it: itself, its
/// elements (a range), its alternatives or parts (an optional, a pair, a variant), or the fields of a record held in it.
template<typename M>
consteval bool holds_child()
{
    if constexpr (Key<M>)
        return stored_record<typename M::element_type>;
    else if constexpr (Data<M>)
        return detail::record_holds_child<typename tag_of_data<M>::type>();
    else if constexpr (std::ranges::range<M>)
        return detail::holds_child<std::remove_cvref_t<std::ranges::range_value_t<M>>>();
    else
        return detail::argument_holds_child(std::type_identity<M> {});
}
}  // namespace detail

namespace detail
{
/// A stand-in view, to ask which mixin a record's views get.
struct any_view
{
};
}  // namespace detail

/// Whether T declares a view mixin of its own (urd::view_mixin<T, V>): its views read it through its context.
template<typename T>
concept DeclaresViewMixin = !std::is_base_of_v<detail::no_view_mixin, view_mixin<T, detail::any_view>>;

/// Whether data<T> is a plain value: no index (no Store holds it), no child in its fields, and no view mixin of its own (a
/// column's label and type, a pool's handle). Nothing in it is built or read through a context: it is held in a builder
/// and read through a view as it is (urd::built_form, urd::resolution), with no builder and no view of its own. A
/// value that is read through its context (a state's storage, its pool's handle in it) declares a mixin.
template<typename T>
concept PlainValue = !SelfIndexed<T> && !detail::record_holds_child<T>() && !DeclaresViewMixin<T>;

namespace detail
{
/// The context a value (a record with no index) is viewed through: of the layers owning the children it holds (every
/// key of a kind the context resolves, at any depth of its members: detail::walk, into the context's own type), the one
/// made last, which reaches the others (a layer's number exceeds its parent's); the context it is read through where it
/// holds none, or where that context has no layers. The same value read through any layer above is thus one view (a
/// view is identified by its record and its context's layer).
template<typename T, typename C>
[[nodiscard]] const C* value_context(const data<T>& record, const C& context)
{
    if constexpr (HasLayers<C>)
    {
        const C* owner = nullptr;
        auto visit = [&]<Key K>(const K& key)
            requires requires { context.view(key); }
        {
            const auto& child = context.view(key).context();
            if constexpr (std::same_as<std::remove_cvref_t<decltype(child)>, C>)
            {
                if (owner == nullptr || child.layer() > owner->layer())
                    owner = &child;
            }
        };
        for_each_cista_member(record, [&](const auto& member) { walk(member, visit); });
        if (owner != nullptr)
            return owner;
        else
            return &context;
    }
    else
        return &context;
}
}  // namespace detail

/// A stored record held in place (inside a composition, or given to urd::resolve): an interned kind (SelfIndexed) as the
/// view its context makes of it (context.view(record)), the context of the repository owning its kind, reached from the
/// context of its parent (the same where it holds the kind itself, a borrowed_view); a context that neither holds nor
/// reaches its kind does not compile: no view is ever through a context that does not own its record. A value (no
/// index: no repository owns its kind, its parent holds it) as its borrowed_view through the lowest layer that reaches
/// its children (detail::value_context); a temporary value as its owning_view, through the same.
template<typename T>
    requires(!PlainValue<T>)
struct resolution<data<T>>
{
    static_assert(stored_record<T>, "urd::data<T> must be Serializable: cista_members() returning urd::Members<...>");

    template<typename C>
        requires(!SelfIndexed<T> || detail::ViewsRecords<C, T>)
    static auto of(const data<T>& member, const C& context)
    {
        if constexpr (SelfIndexed<T>)
            return context.view(member);
        else
            return borrowed_view<T, C> { &member, detail::value_context(member, context) };
    }

    /// A value computed on the fly (a temporary, no index) as its owning_view, which holds it (detail::value_context).
    /// An interned record is never viewed from a temporary.
    template<typename C>
        requires(!SelfIndexed<T>)
    static owning_view<T, C> of(data<T>&& member, const C& context)
    {
        const auto* owner = detail::value_context(member, context);
        return { std::move(member), owner };
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

}  // namespace urd

namespace std::ranges
{
/// Its iterators hold what they read (the range's iterators and the function): they outlive it.
template<typename R, typename F>
inline constexpr bool enable_borrowed_range<urd::transformed_range<R, F>> = true;
}  // namespace std::ranges
