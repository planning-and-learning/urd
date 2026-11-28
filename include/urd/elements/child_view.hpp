#pragma once

// A child read through a context (urd::child resolved): the view of whichever form it holds, interned (index_view), built
// (a builder's view) or held (a record's view), read with T's getters (urd::view_mixin) as any view of T, whatever the
// form. Its members unify the forms' (child_view, child_range); it compares and hashes as the view of the form it holds.

#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/less.hpp"
#include "urd/elements/view.hpp"

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

template<typename T, typename... Vs>
class child_view;

template<typename E, typename... Rs>
class child_range;

namespace detail
{
template<typename R>
inline constexpr bool is_child_view = false;
template<typename T, typename... Vs>
inline constexpr bool is_child_view<child_view<T, Vs...>> = true;

template<typename R>
inline constexpr bool is_std_variant_of_views = false;
template<View... Vs>
inline constexpr bool is_std_variant_of_views<std::variant<Vs...>> = true;

/// Whether R reads one element: a view, a child's variant of views, or a child view.
template<typename R>
concept Reading = View<R> || is_std_variant_of_views<R> || is_child_view<R>;

/// The kind a reading reads, and the views of its forms.
template<typename R>
struct reading_traits;

template<View R>
struct reading_traits<R>
{
    using kind = typename view_kind<R>::type;
    using forms = boost::mp11::mp_list<R>;
};

template<View V, View... Vs>
struct reading_traits<std::variant<V, Vs...>>
{
    using kind = typename view_kind<V>::type;
    using forms = boost::mp11::mp_list<V, Vs...>;
};

template<typename T, typename... Vs>
struct reading_traits<child_view<T, Vs...>>
{
    using kind = T;
    using forms = boost::mp11::mp_list<Vs...>;
};

/// The type that unifies what the forms of a child give for one of their members (Rs, one per form): their type where
/// all give the same; the child view of their forms where all read elements of one kind; an optional of the unified
/// value where all give optionals; the child range of the unified element where all give ranges.
template<typename... Rs>
struct unified;

template<typename R, typename... Rs>
    requires(std::same_as<R, Rs> && ...)
struct unified<R, Rs...>
{
    using type = R;
};

template<typename R, typename... Rs>
    requires(!(std::same_as<R, Rs> && ...)) && Reading<R> && (Reading<Rs> && ...)
            && (std::same_as<typename reading_traits<R>::kind, typename reading_traits<Rs>::kind> && ...)
struct unified<R, Rs...>
{
    using forms = boost::mp11::mp_unique<boost::mp11::mp_append<typename reading_traits<R>::forms, typename reading_traits<Rs>::forms...>>;
    using type = boost::mp11::mp_apply<child_view, boost::mp11::mp_push_front<forms, typename reading_traits<R>::kind>>;
};

template<typename R, typename... Rs>
    requires(!(std::same_as<R, Rs> && ...)) && StdOptional<R> && (StdOptional<Rs> && ...)
struct unified<R, Rs...>
{
    using type = std::optional<typename unified<typename R::value_type, typename Rs::value_type...>::type>;
};

template<typename R, typename... Rs>
    requires(!(std::same_as<R, Rs> && ...)) && (!Reading<R>) && (!StdOptional<R>) && std::ranges::forward_range<const R>
            && (std::ranges::forward_range<const Rs> && ...)
struct unified<R, Rs...>
{
    using element = typename unified<std::ranges::range_value_t<const R>, std::ranges::range_value_t<const Rs>...>::type;
    using type = boost::mp11::mp_apply<child_range, boost::mp11::mp_push_front<boost::mp11::mp_unique<boost::mp11::mp_list<R, Rs...>>, element>>;
};

template<typename R, typename... Rs>
    requires(!(std::same_as<R, Rs> && ...)) && (!Reading<R>) && StdVariant<R> && ((StdVariant<Rs> && !Reading<Rs>) && ...)
            && ((std::variant_size_v<R> == std::variant_size_v<Rs>) && ...)
struct unified<R, Rs...>
{
    template<std::size_t I>
    using at = typename unified<std::variant_alternative_t<I, R>, std::variant_alternative_t<I, Rs>...>::type;

    using type = typename decltype([]<std::size_t... I>(std::index_sequence<I...>)
                                   { return std::type_identity<std::variant<at<I>...>> {}; }(std::make_index_sequence<std::variant_size_v<R>> {}))::type;
};

template<typename... Rs>
using unified_t = typename unified<std::remove_cvref_t<Rs>...>::type;

/// What one form gave for a member, as the unified type U.
template<typename U, typename R>
U unify(R&& result)
{
    using Result = std::remove_cvref_t<R>;
    if constexpr (std::same_as<U, Result>)
        return std::forward<R>(result);
    else if constexpr (is_child_view<U>)
    {
        if constexpr (View<Result>)
            return U(typename U::form_type(std::in_place_type<Result>, std::forward<R>(result)));
        else if constexpr (is_child_view<Result>)
            return std::visit([](const auto& view) { return U(typename U::form_type(std::in_place_type<std::remove_cvref_t<decltype(view)>>, view)); },
                              result.form());
        else
            return std::visit([](const auto& view) { return U(typename U::form_type(std::in_place_type<std::remove_cvref_t<decltype(view)>>, view)); }, result);
    }
    else if constexpr (StdOptional<U>)
        return result.has_value() ? U(detail::unify<typename U::value_type>(*std::forward<R>(result))) : U();
    else if constexpr (StdVariant<U>)
    {
        // A variant value (not a child): its alternative unified, at the same position.
        auto unified_value = std::optional<U>();
        [&]<std::size_t... I>(std::index_sequence<I...>)
        {
            ((result.index() == I ? (unified_value.emplace(std::in_place_index<I>, detail::unify<std::variant_alternative_t<I, U>>(std::get<I>(result))), 0) : 0), ...);
        }(std::make_index_sequence<std::variant_size_v<U>> {});
        return std::move(*unified_value);
    }
    else
        return U(typename U::form_type(std::in_place_type<Result>, std::forward<R>(result)));
}

/// The position of the field whose member is Member among record R's fields.
template<typename R, auto Member>
consteval std::size_t field_position()
{
    return []<typename... Fs>(Fields<Fs...> /*fields*/)
    {
        auto position = std::size_t { 0 };
        auto found = sizeof...(Fs);
        (
            [&]
            {
                if constexpr (std::same_as<std::remove_cv_t<decltype(Fs::member)>, std::remove_cv_t<decltype(Member)>>)
                    if (Fs::member == Member)
                        found = position;
                ++position;
            }(),
            ...);
        return found;
    }(R::fields());
}

/// The number of record R's fields.
template<typename R>
consteval std::size_t field_count()
{ return []<typename... Fs>(Fields<Fs...> /*fields*/) { return sizeof...(Fs); }(R::fields()); }

/// The member of the field at Position among record R's fields.
template<typename R, std::size_t Position>
consteval auto field_member()
{ return []<typename... Fs>(Fields<Fs...> /*fields*/) { return boost::mp11::mp_at_c<boost::mp11::mp_list<Fs...>, Position>::member; }(R::fields()); }
}  // namespace detail

/// A child of kind T read through its context: the view of the form it holds (one of Vs, the views of T's forms), read
/// with T's getters. Its record is named data<T> (the mixin's members name its fields); each member is read from the form
/// it holds, at the same field (a builder's fields are its record's, in order), and unified across the forms: a value as
/// it is, a nested child as a child view, a range as a child range, an optional or a variant value element-wise. It
/// compares and hashes as the view of the form it holds (its form and that view's identity).
template<typename T, typename... Vs>
class child_view : public view_mixin<T, child_view<T, Vs...>>, public compared_by_identity<child_view<T, Vs...>>
{
public:
    using value_type = T;
    using record_type = data<T>;
    using form_type = std::variant<Vs...>;

    explicit child_view(form_type form) : m_form(std::move(form)) {}

    /// The view of the form it holds.
    [[nodiscard]] const form_type& form() const noexcept { return m_form; }

    /// The context it is read through (the same for every form).
    [[nodiscard]] decltype(auto) context() const
    {
        return std::visit([](const auto& view) -> decltype(auto) { return view.context(); }, m_form);
    }

    /// The member Member (a member of data<T>) of the form it holds, unified across the forms: as it is (a reference stays
    /// one) where every form gives the same type.
    template<auto Member>
    [[nodiscard]] decltype(auto) member() const
    {
        constexpr auto position = detail::field_position<data<T>, Member>();
        static_assert(position < detail::field_count<data<T>>(), "urd::child_view::member: a member of one of data<T>'s fields");
        using Results = boost::mp11::mp_list<decltype(std::declval<const Vs&>().template member<detail::field_member<record_of<Vs>, position>()>())...>;
        using U = std::conditional_t<boost::mp11::mp_size<boost::mp11::mp_unique<Results>>::value == 1,
                                     boost::mp11::mp_front<Results>,
                                     boost::mp11::mp_apply<detail::unified_t, Results>>;
        return std::visit(
            [](const auto& view) -> U
            {
                using View = std::remove_cvref_t<decltype(view)>;
                if constexpr (boost::mp11::mp_size<boost::mp11::mp_unique<Results>>::value == 1)
                    return view.template member<detail::field_member<record_of<View>, position>()>();
                else
                    return detail::unify<U>(view.template member<detail::field_member<record_of<View>, position>()>());
            },
            m_form);
    }

    /// Whether it holds the form `view` is a view of, and reads what that view reads.
    template<typename V>
        requires(std::same_as<V, Vs> || ...)
    [[nodiscard]] friend bool operator==(const child_view& child, const V& view)
    {
        const auto* held = std::get_if<V>(&child.m_form);
        return held != nullptr && *held == view;
    }

    /// Its identity: the form it holds and that form's view (so it compares as the view of its form does; an interned
    /// child and a built one are different readings, whatever their fields).
    static constexpr auto identifying_members() noexcept
    {
        return Members<[](const child_view& view) -> const form_type& { return view.form(); }> {};
    }

private:
    form_type m_form;
};

/// A range of children (a member of a child's forms that is a range in each), read as child views: the range of the form
/// the child holds, its elements unified (detail::unify). As capable as its forms' ranges together: a forward range, a
/// random-access one (operator[], iterator arithmetic) where every form's is, sized where every form's is.
template<typename E, typename... Rs>
class child_range : public std::ranges::view_base
{
    static constexpr bool random_access = (std::ranges::random_access_range<const Rs> && ...);

public:
    using form_type = std::variant<Rs...>;

    class iterator
    {
        using positions = std::variant<std::ranges::iterator_t<const Rs>...>;

    public:
        using value_type = E;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::conditional_t<random_access, std::random_access_iterator_tag, std::forward_iterator_tag>;
        using iterator_category = iterator_concept;

        iterator() = default;

        explicit iterator(positions at) : m_at(std::move(at)) {}

        [[nodiscard]] E operator*() const
        {
            return std::visit([](const auto& at) { return detail::unify<E>(*at); }, m_at);
        }

        iterator& operator++()
        {
            std::visit([](auto& at) { ++at; }, m_at);
            return *this;
        }

        iterator operator++(int)
        {
            auto previous = *this;
            ++*this;
            return previous;
        }

        iterator& operator--()
            requires random_access
        {
            std::visit([](auto& at) { --at; }, m_at);
            return *this;
        }

        iterator operator--(int)
            requires random_access
        {
            auto previous = *this;
            --*this;
            return previous;
        }

        iterator& operator+=(difference_type offset)
            requires random_access
        {
            std::visit([offset](auto& at) { at += offset; }, m_at);
            return *this;
        }

        iterator& operator-=(difference_type offset)
            requires random_access
        { return *this += -offset; }

        [[nodiscard]] E operator[](difference_type offset) const
            requires random_access
        { return *(*this + offset); }

        [[nodiscard]] friend iterator operator+(iterator at, difference_type offset)
            requires random_access
        { return at += offset; }

        [[nodiscard]] friend iterator operator+(difference_type offset, iterator at)
            requires random_access
        { return at += offset; }

        [[nodiscard]] friend iterator operator-(iterator at, difference_type offset)
            requires random_access
        { return at -= offset; }

        /// The distance between two positions in one range (of the same form).
        [[nodiscard]] friend difference_type operator-(const iterator& lhs, const iterator& rhs)
            requires random_access
        {
            return std::visit(
                [](const auto& left, const auto& right) -> difference_type
                {
                    if constexpr (std::same_as<decltype(left), decltype(right)>)
                        return static_cast<difference_type>(left - right);
                    else
                        throw std::bad_variant_access();  // positions in ranges of different forms
                },
                lhs.m_at,
                rhs.m_at);
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.m_at == rhs.m_at; }

        friend std::strong_ordering operator<=>(const iterator& lhs, const iterator& rhs)
            requires random_access
        {
            const auto distance = lhs - rhs;
            return distance < 0 ? std::strong_ordering::less : distance > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
        }

    private:
        positions m_at;
    };

    child_range() = default;

    explicit child_range(form_type form) : m_form(std::move(form)) {}

    [[nodiscard]] iterator begin() const
    {
        return at([](const auto& range) { return std::ranges::begin(range); });
    }

    [[nodiscard]] iterator end() const
    {
        return at([](const auto& range) { return std::ranges::end(range); });
    }

    [[nodiscard]] std::size_t size() const
        requires(std::ranges::sized_range<const Rs> && ...)
    {
        return std::visit([](const auto& range) { return static_cast<std::size_t>(std::ranges::size(range)); }, m_form);
    }

    [[nodiscard]] bool empty() const { return begin() == end(); }

    /// Its element at `position`, read from the form's range directly.
    [[nodiscard]] E operator[](std::size_t position) const
        requires random_access
    {
        return std::visit([position](const auto& range) { return detail::unify<E>(std::ranges::begin(range)[static_cast<std::ptrdiff_t>(position)]); },
                          m_form);
    }

private:
    /// The iterator `f` gives for the range of the form held, at the form's position (forms whose ranges share an
    /// iterator type stay apart).
    template<typename F>
    iterator at(F f) const
    {
        return [&]<std::size_t... I>(std::index_sequence<I...>)
        {
            auto result = std::optional<iterator>();
            ((m_form.index() == I ? (result.emplace(std::variant<std::ranges::iterator_t<const Rs>...>(std::in_place_index<I>, f(std::get<I>(m_form)))), 0) : 0),
             ...);
            return std::move(*result);
        }(std::make_index_sequence<sizeof...(Rs)> {});
    }

    form_type m_form;
};

template<typename E, typename... Rs>
inline constexpr bool value_range<child_range<E, Rs...>> = true;

/// A child resolved through a context (a builder's member): the child view of the view its form resolves to (an interned
/// child its index_view, a built one its builder's view, a held one its record's view). Written back from a child view
/// as from the variant of its forms (detail::variant_resolution::stored).
template<typename U, typename C, typename K>
struct resolution<child<U, C, K>> : detail::variant_resolution<std::variant, K, built_child<U, C>, held_child<U, C>>
{
    using Base = detail::variant_resolution<std::variant, K, built_child<U, C>, held_child<U, C>>;
    using Base::stored;

    template<typename P>
    static auto of(const child<U, C, K>& member, const P& context)
    {
        return []<typename... Ws>(std::variant<Ws...> forms) { return child_view<U, Ws...>(std::move(forms)); }(Base::of(member, context));
    }

    template<typename... Vs>
    static child<U, C, K> stored(const child_view<U, Vs...>& readout)
    { return Base::stored(readout.form()); }
};

/// Whether V is a child view (a child resolved), whose form a reader visits (urd::visit_view).
template<typename V>
concept ChildView = detail::is_child_view<std::remove_cvref_t<V>>;

/// The kind a child view reads and the context it reads through: its forms'.
template<typename T, typename... Vs>
struct view_kind<child_view<T, Vs...>>
{
    using type = T;
};

template<typename T, typename V, typename... Vs>
struct view_context<child_view<T, V, Vs...>>
{
    using type = typename view_context<V>::type;
};

/// A child view reads a T (urd::ReadsAs), as the views of its forms do.
template<typename T, typename... Vs>
inline constexpr bool views_of<child_view<T, Vs...>, T> = true;

/// f of the view the child holds.
template<typename T, typename... Vs, typename F>
decltype(auto) visit_view(const child_view<T, Vs...>& view, F&& f)
{ return std::visit(std::forward<F>(f), view.form()); }

namespace detail
{
/// Whether a view reads a builder; whether it is an interned view (index_view): the form of a reading that has a key.
template<typename V>
using reads_builder = std::bool_constant<Builder<record_of<V>>>;

template<typename V>
struct is_index_view : std::false_type
{
};

template<typename T, typename C, typename K>
struct is_index_view<index_view<T, C, K>> : std::true_type
{
};

/// The view of the form of a resolved child (urd::child resolved: the variant of the views of its forms) that `Form`
/// picks: std::bad_variant_access where the child holds another form.
template<template<typename> typename Form, View... Vs>
auto form_view(const std::variant<Vs...>& views)
{
    using Views = boost::mp11::mp_list<Vs...>;
    return std::get<boost::mp11::mp_at<Views, boost::mp11::mp_find_if<Views, Form>>>(views);
}
}  // namespace detail

/// The view of a reading's built form (a builder's view): the reading itself where it is one; of a resolved child or a
/// child view, the form that reads its builder, std::bad_variant_access where the child holds another form. What a
/// reader that works on built records takes out of a child, without naming the view.
template<BuilderView V>
V built_view(const V& view)
{ return view; }

template<View... Vs>
auto built_view(const std::variant<Vs...>& views)
{ return detail::form_view<detail::reads_builder>(views); }

template<typename T, typename... Vs>
auto built_view(const child_view<T, Vs...>& view)
{ return urd::built_view(view.form()); }

/// The interned view a reading of a T reads: an interned one as it is; of a resolved child or a child view, the form
/// that has a key (a built or held record has none, its holder's: std::bad_variant_access).
template<typename T, typename C, typename K>
index_view<T, C, K> interned_view(const index_view<T, C, K>& interned)
{ return interned; }

template<View... Vs>
auto interned_view(const std::variant<Vs...>& reading)
{ return detail::form_view<detail::is_index_view>(reading); }

template<typename T, typename... Vs>
auto interned_view(const child_view<T, Vs...>& reading)
{ return urd::interned_view(reading.form()); }

}  // namespace urd

