#pragma once

#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/index.hpp"

#include <boost/mp11/algorithm.hpp>
#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

/// The children of a record of element type T: every index<U> with U in the type list Types (a boost::mp11 list),
/// found anywhere in the record's cista members: directly, in (nested) flat vectors, as a variant's alternative, in an
/// optional or a pair (std's or cista's, as urd::resolution reads them), or inside a member that names cista members
/// itself (urd::Serializable). U may be T: an index<T> nested in any of these shapes is a child (an (and (and ...))).
/// Only a direct member of type index<T> is not: that is the record's own handle. A direct index<T> member that links
/// to another record of type T (a parent) is therefore not a child either; nest it (an optional, say) to make it one.
/// They are handed out as the views their context makes, context.view(index) (urd::resolves), so callers never handle
/// raw indices. urd::map_variant_children instead hands out each variant slot whole (urd::resolve of it), so that a
/// callback may replace it by another alternative.
namespace urd
{
namespace detail
{
/// Calls f with each member of a record whose cista_members() is a Members list.
template<typename R, typename F>
void for_each_cista_member(R& record, F f)
{ [&]<auto... Ms>(Members<Ms...>) { (f(record.*Ms), ...); }(std::remove_const_t<R>::cista_members()); }

/// Whether a slot of type S is a child: an index of a type in the list Types.
template<typename Types, typename S>
inline constexpr bool is_child = []
{
    using V = std::remove_const_t<S>;
    if constexpr (is_index_type<V>)
        return boost::mp11::mp_contains<Types, typename V::element_type>::value;
    else
        return false;
}();

/// Walks member (const or not, as member is) in member order, calling f with each slot f takes (std::invocable) and
/// not descending into it; any other slot is descended into where it is one of the flat containers a stored record
/// holds (flat_vector, flat_variant, flat_optional, flat_pair) or a nested record, else skipped. An index f does not
/// take is skipped too (its own cista member, value, is no slot).
template<typename M, typename F>
void walk(M& member, F&& f)
{
    using V = std::remove_const_t<M>;
    if constexpr (std::invocable<F&, M&>)
        f(member);
    else if constexpr (is_index_type<V>)
        return;
    else if constexpr (is_flat_vector<V>)
        for (auto& element : member)
            walk(element, f);
    else if constexpr (is_flat_variant<V>)
        member.apply([&](auto& alternative) { walk(alternative, f); });
    else if constexpr (is_flat_optional<V>)
    {
        if (member.has_value())
            walk(*member, f);
    }
    else if constexpr (is_flat_pair<V>)
    {
        walk(member.first, f);
        walk(member.second, f);
    }
    else if constexpr (Serializable<V>)
        for_each_cista_member(member, [&](auto& nested) { walk(nested, f); });
}

/// walk over record's members, but for its own handle: a direct member of type index<T>.
template<typename T, typename R, typename F>
void walk_members(R& record, F&& f)
{
    for_each_cista_member(record,
                          [&]<typename M>(M& member)
                          {
                              if constexpr (!std::same_as<std::remove_const_t<M>, index<T>>)
                                  walk(member, f);
                          });
}
}  // namespace detail

/// Calls f(context.view(child)) for every child of record, in member order.
template<typename Types, typename T, typename R, typename C, typename F>
void for_each_child(const R& record, const C& context, F f)
{
    detail::walk_members<T>(record,
                            [&]<typename S>(S& child)
                                requires detail::is_child<Types, S>
                            { f(context.view(child)); });
}

/// record with every child c replaced by urd::to_stored(f(context.view(c))), in member order: f returns an index of
/// c's type, or a view of one.
template<typename Types, typename T, typename R, typename C, typename F>
R map_children(R record, const C& context, F f)
{
    detail::walk_members<T>(record,
                            [&]<typename S>(S& child)
                                requires detail::is_child<Types, S>
                            { child = urd::to_stored(f(context.view(child))); });
    return record;
}

/// map_children, but every flat_variant slot (one not inside another variant; any alternatives, children or not) is
/// replaced whole: by urd::to_stored(f(urd::resolve(slot, context))), f called with the std::variant of the slot's
/// alternatives resolved (an empty slot throws std::bad_variant_access). f returns that variant, another of its
/// alternatives (a view or index of another alternative type), or anything else to_stored makes into the slot's type or
/// one of its alternatives. The walk does not descend into a slot: the children (and nested variants) inside it are
/// f's to map. Children outside variants are replaced as map_children replaces them, so f is an overload set (or a
/// generic callback) taking both the views and the resolved variants.
template<typename Types, typename T, typename R, typename C, typename F>
R map_variant_children(R record, const C& context, F f)
{
    detail::walk_members<T>(record,
                            [&]<typename S>(S& slot)
                                requires(detail::is_child<Types, S> || is_flat_variant<S>)
                            { slot = urd::to_stored(f(urd::resolve(std::as_const(slot), context))); });
    return record;
}

}  // namespace urd
