#ifndef URD_ELEMENTS_WALK_HPP_
#define URD_ELEMENTS_WALK_HPP_

#include "urd/elements/concepts.hpp"
#include "urd/elements/flat_containers.hpp"
#include "urd/elements/index.hpp"

#include <concepts>
#include <type_traits>

namespace urd::detail
{
/// Calls f with each member of a record whose cista_members() is a Members list.
template<typename R, typename F>
void for_each_cista_member(R& record, F f)
{
    [&]<auto... Ms>(Members<Ms...>) { (f(record.*Ms), ...); }(std::remove_const_t<R>::cista_members());
}

/// Walks member (const or not, as member is) in member order, calling f with each slot f takes (std::invocable) and
/// not descending into it; any other slot is descended into where it is one of the flat containers a stored record
/// holds (flat_vector, flat_variant, flat_optional, flat_pair) or a nested record, else skipped. An index f does not
/// take is skipped too (its own cista member, value, is no slot).
template<typename M, typename F>
void walk(M& member, F& f)
{
    using V = std::remove_const_t<M>;
    if constexpr (std::invocable<F&, M&>)
        f(member);
    else if constexpr (Key<V>)
        return;
    else if constexpr (FlatVector<V>)
        for (auto& element : member)
            walk(element, f);
    else if constexpr (FlatVariant<V>)
        member.apply([&](auto& alternative) { walk(alternative, f); });
    else if constexpr (FlatOptional<V>)
    {
        if (member.has_value())
            walk(*member, f);
    }
    else if constexpr (FlatPair<V>)
    {
        walk(member.first, f);
        walk(member.second, f);
    }
    else if constexpr (Serializable<V>)
        for_each_cista_member(member, [&](auto& nested) { walk(nested, f); });
}

/// A value written into one of its own type, in place: a value of fields (a plain value, data<T> with no index or child)
/// field by field, a flat vector by set (its buffer kept), anything else assigned. So a plain value that holds buffers
/// is written without a copy of itself (it may delete its copy, as a record that is not trivially copyable does).
template<typename M>
void assign_value(const M& value, M& into)
{
    if constexpr (HasFields<M>)
        for_each_field<M>([&]<typename F>() { detail::assign_value(value.*F::member, into.*F::member); });
    else if constexpr (FlatVector<M>)
        into.set(value.begin(), value.end());
    else
        into = value;
}
}  // namespace urd::detail

#endif
