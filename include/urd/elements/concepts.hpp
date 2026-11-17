/*
 * Copyright (C) 2025-2026 Dominik Drexler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace urd
{

/// What Hash returns: 64 bits on every platform, so hashes are the same everywhere.
using hash_t = std::uint64_t;

/// std::isnan, also in constant evaluation, where std::isnan is constexpr only from C++23: every value but NaN
/// lies within [-infinity, infinity].
template<std::floating_point T>
constexpr bool is_nan(T value) noexcept
{
    constexpr auto infinity = std::numeric_limits<T>::infinity();
    return !(value >= -infinity && value <= infinity);
}

/// With T void (EqualTo<>, Hash<>, Less<>): transparent, by the type of what they are given.
template<typename T = void>
struct EqualTo;

template<typename T = void>
struct Hash;

template<typename T = void>
struct Less;

/// The members that identify a record: data members, const getters, or captureless lambdas over the record for a part
/// that is computed or held elsewhere (each read by std::invoke):
/// `static constexpr auto identifying_members() noexcept { return urd::Members<&Data::a, &Data::b> {}; }`.
template<auto... Ms>
struct Members
{
};

/// A name as a template argument: the characters of a string literal, with its terminating null.
template<std::size_t N>
struct fixed_string
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,google-explicit-constructor): a string literal, as written
    constexpr fixed_string(const char (&text)[N]) noexcept { std::ranges::copy(text, characters.begin()); }

    [[nodiscard]] constexpr std::string_view view() const noexcept { return { characters.data(), N - 1 }; }

    std::array<char, N> characters {};
};

/// `prefix` then `name`, as one name.
template<fixed_string Prefix, fixed_string Name>
consteval auto concatenated() noexcept
{
    constexpr auto prefix = Prefix.view();
    constexpr auto name = Name.view();
    auto result = std::array<char, prefix.size() + name.size() + 1> {};
    std::ranges::copy(name, std::ranges::copy(prefix, result.begin()).out);
    return result;
}

namespace detail
{
/// The characters of Head followed by those of each of Parts, null-terminated.
template<fixed_string Head, const auto&... Parts>
struct joined_characters
{
    static constexpr auto characters = []
    {
        constexpr auto head = Head.view();
        auto result = std::array<char, (head.size() + ... + std::string_view(Parts).size()) + 1> {};
        auto out = std::ranges::copy(head, result.begin()).out;
        ((out = std::ranges::copy(std::string_view(Parts), out).out), ...);
        return result;
    }();
};
}  // namespace detail

/// A name made of `Head` and the names `Parts` (each a tag's name: a string literal, a C string or a string_view), as
/// one name. A tag that is named after the tags it is made of declares so:
/// `static constexpr std::string_view name = urd::joined_name<"Atom", T::name, F::name>;`.
template<fixed_string Head, const auto&... Parts>
inline constexpr std::string_view joined_name { detail::joined_characters<Head, Parts...>::characters.data(),
                                                detail::joined_characters<Head, Parts...>::characters.size() - 1 };

/// A field of a record: its public name and the data member that holds it.
template<fixed_string Name, auto Member>
struct field
{
    /// The name, null-terminated (a template parameter object: it lives as long as the program).
    static constexpr std::string_view name = Name.view();
    /// The name as a C string (where a function takes one).
    static constexpr const char* c_name = Name.characters.data();
    static constexpr auto member = Member;
    /// The name after `Prefix` (get_lhs), null-terminated.
    template<fixed_string Prefix>
    static constexpr auto prefixed = concatenated<Prefix, Name>();
};

/// The type of the data member `Member` points to.
template<typename T, typename C>
T member_type_of(T C::* /*member*/);
template<auto Member>
using member_type_t = decltype(member_type_of(Member));

/// A record's fields, in data order: what it is, under the names it is read and written by. A record declares them
/// once, `static constexpr auto fields() noexcept { return urd::Fields<urd::field<"lhs", &Data::lhs>, ...> {}; }`,
/// and its identifying members are their members (urd::members_of); serialization, printing and bindings iterate them
/// (urd::for_each_field), each field read through a view as `view.template member<F::member>()`.
template<typename... Fs>
struct Fields
{
};

/// The members of `fields`, as identifying_members gives them.
template<typename... Fs>
consteval auto members_of(Fields<Fs...> /*fields*/) noexcept
{ return Members<Fs::member...> {}; }

/// A record that declares its fields.
template<typename T>
concept HasFields = requires { T::fields(); };

/// Whether record T declares a field named `name`.
template<typename T>
    requires requires { T::fields(); }
consteval bool has_field(std::string_view name)
{
    return []<typename... Fs>(Fields<Fs...> /*fields*/, std::string_view wanted) { return ((Fs::name == wanted) || ...); }(T::fields(), name);
}

/// `f.template operator()<F>()` for each field F of record T, in order.
template<HasFields T, typename Function>
constexpr void for_each_field(const Function& f)
{
    [&]<typename... Fs>(Fields<Fs...> /*fields*/) { (f.template operator()<Fs>(), ...); }(T::fields());
}

/// The name of a parameter of a function beside its view (a member's this), as C++ names it: what Python calls it by
/// (urd::python::bind_functions), as nanobind's nb::arg.
template<fixed_string Name>
struct arg
{
    /// The name as a C string.
    static constexpr const char* c_name = Name.characters.data();
};

/// A function of a view beyond its field getters: its public name, the function (a member of its mixin, or a free
/// function whose sole parameter is the view), and an arg per parameter, in order.
template<fixed_string Name, auto Function, typename... Args>
struct function
{
    /// The name as a C string (where a function takes one).
    static constexpr const char* c_name = Name.characters.data();
    static constexpr auto pointer = Function;

    /// Calls f with each arg, in order (urd::python::bind_functions: one nb::arg each).
    template<typename F>
    static constexpr decltype(auto) with_arguments(F&& f)
    { return std::forward<F>(f).template operator()<Args...>(); }
};

/// A view's functions beyond its field getters, under the names Python calls them by. A view mixin lists every member
/// it has besides self() and the field getters, and every free function over the view, once:
/// `static constexpr auto functions() noexcept { return urd::Functions<urd::function<"is_ground", &view_mixin::is_ground>, ...> {}; }`.
template<typename... Fs>
struct Functions
{
};

/// Two function lists as one, in order: a list extended where a condition holds (`if constexpr`), each entry named
/// only in the branch that keeps it.
template<typename... As, typename... Bs>
constexpr Functions<As..., Bs...> operator+(Functions<As...> /*a*/, Functions<Bs...> /*b*/) noexcept
{ return {}; }

/// A view whose mixin lists its functions.
template<typename V>
concept HasFunctions = requires { V::functions(); };

/// `f.template operator()<F>()` for each function F of view V, in order.
template<HasFunctions V, typename Function>
constexpr void for_each_function(const Function& f)
{
    [&]<typename... Fs>(Functions<Fs...> /*functions*/) { (f.template operator()<Fs>(), ...); }(V::functions());
}

namespace detail
{
template<typename L>
inline constexpr bool is_members_v = false;
template<auto... Ms>
inline constexpr bool is_members_v<Members<Ms...>> = true;
}  // namespace detail

/// @brief T names its members as a Members type (cista_members), so cista can serialize it.
template<typename T>
concept Serializable = requires { T::cista_members(); } && detail::is_members_v<decltype(T::cista_members())>;

/// @brief Empties every member a record names for cista (Serializable), keeping the buffers they own: each by its
/// clear(), or else set to its default. A record reused in place (a builder) is reset with it.
template<Serializable T>
void clear_members(T& record)
{
    const auto clear = [](auto& member)
    {
        if constexpr (requires { member.clear(); })
            member.clear();
        else
            member = std::remove_cvref_t<decltype(member)> {};
    };
    [&]<auto... Ms>(Members<Ms...> /*members*/) { (clear(record.*Ms), ...); }(T::cista_members());
}

/// @brief T names what identifies it, as a Members list (its identifying_members()); Hash, EqualTo and Less then
/// follow from that.
template<typename T>
concept Identifiable = requires { T::identifying_members(); } && detail::is_members_v<decltype(T::identifying_members())>;

namespace detail
{

/// What identifies `x`, as one value that Hash, EqualTo and Less read: its identifying members, in order, as a tuple
/// (a data member by reference, a getter's result as the getter gives it).
template<Identifiable T>
constexpr auto identity(const T& x)
{
    return []<auto... Ms>(const T& of, Members<Ms...> /*members*/)
    { return std::tuple<decltype(std::invoke(Ms, of))...>(std::invoke(Ms, of)...); }(x, T::identifying_members());
}

/// What member M (a data member or a getter) gives of a T: what identity() holds of it.
template<typename T, auto M>
using member_read_t = decltype(std::invoke(M, std::declval<const T&>()));
template<typename T, typename Ms>
struct identity_of_members;

template<typename T, auto... Ms>
struct identity_of_members<T, Members<Ms...>>
{
    using type = std::tuple<member_read_t<T, Ms>...>;
};

/// The type identity() gives, named from the members: naming it instantiates neither identity() nor the tuple.
template<Identifiable T>
using identity_t = typename identity_of_members<T, decltype(T::identifying_members())>::type;

}  // namespace detail

/// Whether a range's elements, in the order it iterates them, are its value: Hash, EqualTo and Less then follow from
/// its elements'. True for the standard containers that keep an order of their own; a container type opts in by
/// specializing it (a hash table does not: its order is not its contents').
template<typename R>
inline constexpr bool value_range = false;
template<typename T, std::size_t N>
inline constexpr bool value_range<std::array<T, N>> = true;
template<typename T, typename Allocator>
inline constexpr bool value_range<std::vector<T, Allocator>> = true;
template<typename Key, typename Compare, typename Allocator>
inline constexpr bool value_range<std::set<Key, Compare, Allocator>> = true;
template<typename Key, typename T, typename Compare, typename Allocator>
inline constexpr bool value_range<std::map<Key, T, Compare, Allocator>> = true;
template<typename T, std::size_t Extent>
inline constexpr bool value_range<std::span<T, Extent>> = true;

template<typename T>
concept Enumeration = std::is_enum_v<T>;

/// A hasher H of T, an equality E of Lhs and Rhs, an order C of Lhs and Rhs: what a container takes as its predicates.
template<typename H, typename T>
concept HashFor = requires(const H& hash, const T& value) {
    { hash(value) } -> std::same_as<hash_t>;
};
template<typename E, typename Lhs, typename Rhs = Lhs>
concept EqualToFor = requires(const E& equal_to, const Lhs& lhs, const Rhs& rhs) {
    { equal_to(lhs, rhs) } -> std::same_as<bool>;
};
template<typename C, typename Lhs, typename Rhs = Lhs>
concept LessFor = requires(const C& less, const Lhs& lhs, const Rhs& rhs) {
    { less(lhs, rhs) } -> std::same_as<bool>;
};

template<typename T>
concept Hashable = HashFor<Hash<std::remove_cvref_t<T>>, T>;

template<typename T>
concept EqualityComparableByEqualTo = EqualToFor<EqualTo<std::remove_cvref_t<T>>, T>;

template<typename T>
concept OrderedByLess = LessFor<Less<std::remove_cvref_t<T>>, T>;

namespace detail
{

/// Whether every identifying member of T is hashable, equality-comparable or ordered: what Hash, EqualTo and Less of T
/// require, folded over the members, so that no tuple of them is instantiated for a record (identity_t only names one).
template<typename T, auto... Ms>
consteval bool members_hashable(Members<Ms...> /*members*/)
{ return (Hashable<member_read_t<T, Ms>> && ...); }

template<typename T, auto... Ms>
consteval bool members_equality_comparable(Members<Ms...> /*members*/)
{ return (EqualityComparableByEqualTo<member_read_t<T, Ms>> && ...); }

template<typename T, auto... Ms>
consteval bool members_ordered(Members<Ms...> /*members*/)
{ return (OrderedByLess<member_read_t<T, Ms>> && ...); }

}  // namespace detail

/// U is what identifies T, given alone (its identity_t: data members by reference, as std::tie gives them, getters' results
/// as they give them), so a transparent lookup takes a U for a T.
template<typename U, typename T>
concept identity_of = Identifiable<T> && std::same_as<std::remove_cvref_t<U>, detail::identity_t<T>>;

/// Whether M is an optional value: std::optional, or cista's.
template<typename M>
concept optional_value = requires(const M& member) {
    { member.has_value() } -> std::convertible_to<bool>;
    *member;
};

/// Whether M is a pair: std::pair, or cista's.
template<typename M>
concept pair_value = requires(const M& member) {
    typename M::first_type;
    typename M::second_type;
    member.first;
    member.second;
};

/// Whether M is a variant: std::variant, or cista's, each visited by its own visit (found by ADL).
template<typename M>
concept variant_value = requires(const M& member) {
    { member.index() } -> std::convertible_to<std::size_t>;
    visit([](const auto&) {}, member);
};

/// The value type of an optional value.
template<optional_value M>
using optional_value_t = std::remove_cvref_t<decltype(*std::declval<const M&>())>;

}  // namespace urd
