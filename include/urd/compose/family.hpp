#pragma once

#include "urd/index.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace urd::compose
{

/// Compositions of Stores live in urd::compose: they hold Stores and choose no representation of their own.
///
/// One F<T> for each type T in Ts, reached by its type: a repository per kind of entity, a translation map per kind
/// of view. F is usually an alias of a Store: template<typename T> using Ids = urd::Store<urd::key<T>, urd::positional>.
/// Not a Store kind of its own: a fixed set of Stores named by type.
///
/// Where the members number their elements (positional sets, pools), the family resolves an index of any member
/// type (urd::resolves), so a view<T, Family> reads a record and the indices in it through one context. Where the
/// members layer, a family layers too: Family(&parent) layers each member over the parent's member of its type and
/// numbers itself along the chain, and a view made of the family points at the layer that owns the element.
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

    static constexpr bool layerable = (requires(F<Ts>& member, const F<Ts>* parent) { member.layer_over(parent); } && ...);
    template<typename T>
    static constexpr bool chained = requires(const F<T>& member) { member.base(); };

public:
    Family() = default;
    /// A layer over `parent` (nullptr: a root): each member layered over the parent's member of its type. The
    /// parent must outlive this family and not grow after the branch.
    explicit Family(const Family* parent)
        requires layerable
        : m_parent(parent), m_layer(parent == nullptr ? 0 : parent->m_layer + 1)
    {
        if (parent != nullptr)
            (get<Ts>().layer_over(&parent->template get<Ts>()), ...);
    }

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

    [[nodiscard]] const Family* parent() const noexcept { return m_parent; }
    /// This family's number along the chain: 0 for a root.
    [[nodiscard]] std::size_t layer() const noexcept { return m_layer; }

    /// The record of `at`, from the member for T, whichever layer holds it.
    template<typename T>
    [[nodiscard]] decltype(auto) operator[](urd::index<T> at) const
    {
        return get<T>()[at];
    }
    /// A view of the record at `at` through this family, which resolves the indices in it; where the members layer,
    /// through the layer that owns the record.
    template<typename T>
    [[nodiscard]] auto view(urd::index<T> at) const
    {
        static_assert(requires { sizeof(urd::view<T, Family>); }, "urd::compose::Family::view: include <urd/view.hpp>");
        return urd::view<T, Family>(at, owner(at));
    }
    /// The record's index in the member for T, and whether it was new.
    template<typename T>
    std::pair<urd::index<T>, bool> insert(const T& record)
    {
        auto& member = get<T>();
        const auto result = member.insert(record);
        if constexpr (std::same_as<std::remove_cvref_t<decltype(result.first)>, urd::index<T>>)
            return result;
        else
            return { member.index(result.first), result.second };
    }
    /// The index `key` gets, built where new by `make(index)` (a pool of records).
    template<typename T, std::invocable<urd::index<T>> Factory>
    std::pair<urd::index<T>, bool> insert(const T& key, Factory make)
    {
        return get<T>().insert(key, make);
    }
    /// The index of an equal record in the member for T, or none.
    template<typename T>
    [[nodiscard]] urd::index<T> find(const T& record) const
    {
        const auto& member = get<T>();
        const auto found = member.find(record);
        if constexpr (std::same_as<std::remove_cvref_t<decltype(found)>, urd::index<T>>)
            return found;
        else
            return found == member.end() ? urd::index<T>() : member.index(found);
    }
    /// Whether the member for T has a record at `at`.
    template<typename T>
    [[nodiscard]] bool contains(urd::index<T> at) const
    {
        return !at.is_none() && (at.value) < get<T>().size();
    }
    /// Clears every member.
    void clear()
        requires(requires(F<Ts>& member) { member.clear(); } && ...)
    {
        (get<Ts>().clear(), ...);
    }

private:
    /// The family along the chain whose member for T holds `at`: this one, or an ancestor.
    template<typename T>
    [[nodiscard]] const Family& owner(urd::index<T> at) const
    {
        if constexpr (chained<T>)
        {
            const auto* family = this;
            while ((at.value) < family->template get<T>().base())
                family = family->m_parent;
            return *family;
        }
        else
            return *this;
    }

    std::tuple<F<Ts>...> m_members;
    const Family* m_parent = nullptr;
    std::size_t m_layer = 0;
};

}  // namespace urd::compose
