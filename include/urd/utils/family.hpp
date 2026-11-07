#pragma once

// A family of Stores, one per type, reached by type. A utility, not a kind: it selects no representation (its members
// are Stores, which do).

#include "urd/elements/index.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace urd::utils
{

/// One F<T> for each type T in Ts, reached by its type: a repository per kind of entity, a translation map per kind
/// of view. F is usually an alias of a Store: template<typename T> using Ids = urd::Store<urd::key<T>, urd::positional>.
/// Not a Store kind of its own: a fixed set of Stores named by type.
///
/// Where the members layer, a family layers too: Family(&parent) layers each member over the parent's member of its
/// type and numbers itself along the chain (urd::layer). Its operator[] reads an element by its member's handle (a
/// pool of urd::data<T> handing out index<T>: urd::pool<V, H>), whichever layer holds it.
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
    static_assert(((occurrences<Ts> == 1) && ...), "urd::utils::Family: each type once");

    static constexpr bool layerable = (requires(F<Ts>& member, const F<Ts>* parent) { member.layer_over(parent); } && ...);

    /// A member numbered as the family is (urd::layer), where it numbers its elements.
    template<typename M>
    static M make(std::size_t number)
    {
        if constexpr (std::constructible_from<M, urd::layer>)
            return M(urd::layer { number });
        else
            return M();
    }

public:
    Family() : m_members(make<F<Ts>>(0)...) {}
    /// A layer over `parent` (nullptr: a root): each member layered over the parent's member of its type. The
    /// parent must outlive this family and not grow after the branch.
    explicit Family(const Family* parent)
        requires layerable
        : m_members(make<F<Ts>>(parent == nullptr ? 0 : parent->m_layer + 1)...),
          m_parent(parent),
          m_layer(parent == nullptr ? 0 : parent->m_layer + 1)
    {
        if (parent != nullptr)
            (get<Ts>().layer_over(&parent->template get<Ts>()), ...);
    }

    /// The member for T, which must be one of Ts.
    template<typename T>
    [[nodiscard]] F<T>& get()
    {
        static_assert(position<T>() < sizeof...(Ts), "urd::utils::Family::get: T is not one of the family's types");
        return std::get<position<T>()>(m_members);
    }
    template<typename T>
    [[nodiscard]] const F<T>& get() const
    {
        static_assert(position<T>() < sizeof...(Ts), "urd::utils::Family::get: T is not one of the family's types");
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

    /// The element of `at`, from the member for T, whichever layer holds it.
    template<typename T>
    [[nodiscard]] decltype(auto) operator[](urd::index<T> at) const
    {
        return get<T>()[at];
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
    std::tuple<F<Ts>...> m_members;
    const Family* m_parent = nullptr;
    std::size_t m_layer = 0;
};

}  // namespace urd::utils
