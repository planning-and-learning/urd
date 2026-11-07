#pragma once

#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/index.hpp"

#include <boost/mp11/algorithm.hpp>
#include <concepts>
#include <tuple>
#include <type_traits>

/// The children of a record of element type T: every index<U> with U in the type list Types (a boost::mp11 list),
/// found anywhere in the record's cista members: directly, in (nested) flat vectors, as a variant's alternative, in an
/// optional or a pair (std's or cista's, as urd::resolution reads them), or inside a member that names cista members
/// itself (urd::Serializable). U may be T: an index<T> nested in any of these shapes is a child (an (and (and ...))).
/// Only a direct member of type index<T> is not: that is the record's own handle. A direct index<T> member that links
/// to another record of type T (a parent) is therefore not a child either; nest it (an optional, say) to make it one.
/// They are handed out as the views their context makes, context.view(index) (urd::resolves), so callers never handle
/// raw indices.
namespace urd
{
namespace detail
{
/// Calls f with each member of a record whose cista_members() is a Members list.
template<typename R, typename F>
void for_each_cista_member(R& record, F f)
{ [&]<auto... Ms>(Members<Ms...>) { (f(record.*Ms), ...); }(std::remove_const_t<R>::cista_members()); }

/// Calls f with each child index in member (const or not, as member is), in member order: an index, or one inside the
/// flat containers a stored record holds (flat_vector, flat_variant, flat_optional, flat_pair) or a nested record.
template<typename Types, typename M, typename F>
void for_each_child_index(M& member, F&& f)
{
    using V = std::remove_const_t<M>;
    if constexpr (is_index_type<V>)
    {
        if constexpr (boost::mp11::mp_contains<Types, typename V::element_type>::value)
            f(member);
    }
    else if constexpr (is_flat_vector<V>)
        for (auto& element : member)
            for_each_child_index<Types>(element, f);
    else if constexpr (is_flat_variant<V>)
        member.apply([&](auto& alternative) { for_each_child_index<Types>(alternative, f); });
    else if constexpr (is_flat_optional<V>)
    {
        if (member.has_value())
            for_each_child_index<Types>(*member, f);
    }
    else if constexpr (is_flat_pair<V>)
    {
        for_each_child_index<Types>(member.first, f);
        for_each_child_index<Types>(member.second, f);
    }
    else if constexpr (Serializable<V>)
        for_each_cista_member(member, [&](auto& nested) { for_each_child_index<Types>(nested, f); });
}

/// for_each_child_index over record's members, but for its own handle: a direct member of type index<T>.
template<typename Types, typename T, typename R, typename F>
void for_each_child_index_of(R& record, F&& f)
{
    for_each_cista_member(record,
                          [&]<typename M>(M& member)
                          {
                              if constexpr (!std::same_as<std::remove_const_t<M>, index<T>>)
                                  for_each_child_index<Types>(member, f);
                          });
}
}  // namespace detail

/// Calls f(context.view(child)) for every child of record, in member order.
template<typename Types, typename T, typename R, typename C, typename F>
void for_each_child(const R& record, const C& context, F f)
{ detail::for_each_child_index_of<Types, T>(record, [&](auto child) { f(context.view(child)); }); }

/// record with every child c replaced by f(context.view(c)), an index of the same type, in member order.
template<typename Types, typename T, typename R, typename C, typename F>
R map_children(R record, const C& context, F f)
{
    detail::for_each_child_index_of<Types, T>(record, [&](auto& child) { child = f(context.view(child)); });
    return record;
}

}  // namespace urd
