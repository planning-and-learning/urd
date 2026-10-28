#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>

namespace urd::compose
{

/// Compositions of Stores live in urd::compose: they hold Stores and choose no representation of their own.
///
/// One F<T> for each type T in Ts, reached by its type: a repository per kind of entity, a translation map per kind
/// of view. F is usually an alias of a Store: template<typename T> using Ids = urd::Store<urd::key<T>, urd::positional>.
/// Not a Store kind of its own: a fixed set of Stores named by type.
template<template<typename> class F, typename... Ts>
class Family
{
    template<typename T>
    static consteval std::size_t position()
    {
        constexpr auto matches = std::array<bool, sizeof...(Ts)> { std::same_as<T, Ts>... };
        std::size_t found = 0;
        while (found < matches.size() && !matches.at(found))
            ++found;
        return found;
    }
    template<typename T>
    static constexpr std::size_t occurrences = (0 + ... + static_cast<std::size_t>(std::same_as<T, Ts>));
    static_assert(((occurrences<Ts> == 1) && ...), "urd::compose::Family: each type once");

public:
    /// The member for T, which must be one of Ts.
    template<typename T>
    [[nodiscard]] F<T>& get()
    {
        static_assert(position<T>() < sizeof...(Ts), "urd::compose::Family::get: T is not one of the family's types");
        return std::get<position<T>()>(m_members);
    }
    template<typename T>
    [[nodiscard]] const F<T>& get() const
    {
        static_assert(position<T>() < sizeof...(Ts), "urd::compose::Family::get: T is not one of the family's types");
        return std::get<position<T>()>(m_members);
    }

    /// Calls `function` with each member in turn, in the order of Ts.
    template<typename Function>
    void for_each(Function function)
    {
        std::apply([&function](auto&... members) { (function(members), ...); }, m_members);
    }
    template<typename Function>
    void for_each(Function function) const
    {
        std::apply([&function](const auto&... members) { (function(members), ...); }, m_members);
    }

private:
    std::tuple<F<Ts>...> m_members;
};

}  // namespace urd::compose
