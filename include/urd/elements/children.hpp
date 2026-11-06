#pragma once

#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/index.hpp"

#include <boost/mp11/algorithm.hpp>
#include <concepts>
#include <tuple>
#include <type_traits>

/// The children of a record of element type T: every index<U> with U in the type list Types (a boost::mp11 list) and U
/// not T, found anywhere in the record's cista members: directly, in (nested) flat vectors, as a flat variant's
/// alternative, or inside a member that names cista members itself (urd::Serializable). They are handed out as the
/// views their context makes, context.view(index) (urd::resolves), so callers never handle raw indices.
namespace urd
{
namespace detail
{
/// Calls f with each member of a record whose cista_members() is a Members list.
template<typename R, typename F>
void for_each_cista_member(R& record, F f)
{ [&]<auto... Ms>(Members<Ms...>) { (f(record.*Ms), ...); }(std::remove_const_t<R>::cista_members()); }

/// Calls f with each child index in member (const or not, as member is), in member order.
template<typename Types, typename T, typename M, typename F>
void for_each_child_index(M& member, F&& f)
{
    using V = std::remove_const_t<M>;
    if constexpr (requires { requires std::same_as<V, index<typename V::element_type>>; })
    {
        if constexpr (!std::same_as<typename V::element_type, T> && boost::mp11::mp_contains<Types, typename V::element_type>::value)
            f(member);
    }
    else if constexpr (requires { requires std::same_as<V, flat_vector<typename V::value_type>>; })
        for (auto& element : member)
            for_each_child_index<Types, T>(element, f);
    else if constexpr (requires { member.apply([](auto&) {}); })
        member.apply([&](auto& alternative) { for_each_child_index<Types, T>(alternative, f); });
    else if constexpr (Serializable<V>)
        for_each_cista_member(member, [&](auto& nested) { for_each_child_index<Types, T>(nested, f); });
}
}  // namespace detail

/// Calls f(context.view(child)) for every child of record, in member order.
template<typename Types, typename T, typename R, typename C, typename F>
void for_each_child(const R& record, const C& context, F f)
{ detail::for_each_child_index<Types, T>(record, [&](auto child) { f(context.view(child)); }); }

/// record with every child c replaced by f(context.view(c)), an index of the same type, in member order.
template<typename Types, typename T, typename R, typename C, typename F>
R map_children(R record, const C& context, F f)
{
    detail::for_each_child_index<Types, T>(record, [&](auto& child) { child = f(context.view(child)); });
    return record;
}

}  // namespace urd
