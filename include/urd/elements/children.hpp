#pragma once

#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/view.hpp"
#include "urd/traits.hpp"

#include <boost/mp11/algorithm.hpp>
#include <concepts>
#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

/// The children of a record of element type T: every index<U> with U in the type list Types (a boost::mp11 list),
/// found anywhere in the record's cista members: directly, in (nested) flat vectors, as a variant's alternative, in an
/// optional or a pair (std's or cista's, as urd::resolution reads them), or inside a member that names cista members
/// itself (urd::Serializable). U may be T: an index<T> nested in any of these shapes is a child (an (and (and ...))).
/// Only a direct member of type index<T> is not: that is the record's own handle. A direct index<T> member that links
/// to another record of type T (a parent) is therefore not a child either; nest it (an optional, say) to make it one.
/// They are handed out as the views their context makes, context.view(index) (urd::resolves), so callers never handle
/// raw indices. urd::map_variant_children instead hands out each variant slot whole (urd::resolve of it), so that a
/// callback may replace it by another alternative.
///
/// A builder's children are its urd::child members (an index, a built builder, or a record not interned yet), found in the
/// same shapes as std's containers and builders held in place: urd::map_children of a builder writes its record with
/// each child replaced by an index, and urd::intern interns a builder so (its built children first).
namespace urd
{

/// The index an insert gives: of the pair (view or index, created) a repository's insert returns, of the interned
/// record's view (an index_view: its key), or the index itself.
template<typename U>
index<U> inserted_index(index<U> at) noexcept
{ return at; }

template<typename T, typename C, typename K>
K inserted_index(const index_view<T, C, K>& view) noexcept
{ return view.index(); }

/// An insert's result (std::set's convention): the interned record, as its view or its index (the overloads above),
/// and whether it was created.
template<typename A>
auto inserted_index(const std::pair<A, bool>& inserted) noexcept
{ return urd::inserted_index(inserted.first); }

namespace detail
{
/// Calls f with each member of a record whose cista_members() is a Members list.
template<typename R, typename F>
void for_each_cista_member(R& record, F f)
{
    [&]<auto... Ms>(Members<Ms...>) { (f(record.*Ms), ...); }(std::remove_const_t<R>::cista_members());
}

/// A slot of type S that is a child: an index of a type in the list Types.
template<typename S, typename Types>
concept child_of = is_index_type<std::remove_const_t<S>> && boost::mp11::mp_contains<Types, typename std::remove_const_t<S>::element_type>::value;
template<typename Types, typename S>
inline constexpr bool is_child = child_of<S, Types>;

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
void walk_members(R& record, F f)
{
    for_each_cista_member(record,
                          [&]<typename M>(M& member)
                          {
                              if constexpr (!std::same_as<std::remove_const_t<M>, index<T>>)
                                  walk(member, f);
                          });
}

/// Assigns source's members to scratch's, all but scratch's own handle (a direct member of type index<T>, which stays as
/// it is: none in a checked-out scratch). A flat vector keeps the memory it has (set), so a scratch reused across calls
/// allocates only where a member grows beyond what it held before; every other member is assigned.
template<typename T, typename R>
void assign_members(R& scratch, const R& source)
{
    [&]<auto... Ms>(Members<Ms...>)
    {
        (
            [&]
            {
                using M = std::remove_cvref_t<decltype(scratch.*Ms)>;
                if constexpr (std::same_as<M, index<T>>)
                    return;
                else if constexpr (is_flat_vector<M>)
                    (scratch.*Ms).set((source.*Ms).begin(), (source.*Ms).end());
                else
                    scratch.*Ms = source.*Ms;
            }(),
            ...);
    }(R::cista_members());
}
/// A builder's member written into its record's member of the same position, converted: a child (urd::child) as the
/// index f gives it, a builder held in place as its record, std's containers as the flat ones, a value as it is.
template<typename M, typename F>
void assign_member(M& target, const M& source, F& /*f*/)
{ target = source; }

/// What an insert gave, as the key K: the key itself, or the index an insert's result names (urd::inserted_index).
template<typename K, typename R>
K as_key(R&& inserted)
{
    if constexpr (std::same_as<std::remove_cvref_t<R>, K>)
        return inserted;
    else
        return urd::inserted_index(inserted);
}

template<typename K, typename U, typename C, typename F>
void assign_member(K& target, const child<U, C, K>& source, F& f)
{ target = detail::as_key<K>(f(source)); }

template<typename U, typename C, typename F>
void assign_built(data<U>& target, const builder<U, C>& source, F& f);

template<typename U, typename C, typename F>
void assign_member(data<U>& target, const builder<U, C>& source, F& f)
{ detail::assign_built(target, source, f); }

template<typename M, typename B, typename F>
void assign_member(flat_vector<M>& target, const std::vector<B>& source, F& f)
{
    target.resize(source.size());
    for (std::size_t i = 0; i < source.size(); ++i)
        detail::assign_member(target[i], source[i], f);
}

/// A Store of children (a builder's set of them, say) as the record's flat vector, in the Store's order.
template<typename M, typename Kind, typename... Tags, typename F>
void assign_member(flat_vector<M>& target, const Store<Kind, Tags...>& source, F& f)
{
    target.resize(source.size());
    auto at = std::size_t { 0 };
    for (const auto& element : source)
        detail::assign_member(target[at++], element, f);
}

template<typename M, typename B, typename F>
void assign_member(flat_optional<M>& target, const std::optional<B>& source, F& f)
{
    if (!source.has_value())
    {
        target.reset();
        return;
    }
    auto value = M();
    detail::assign_member(value, *source, f);
    target = std::move(value);
}

template<typename M, typename N, typename A, typename B, typename F>
void assign_member(flat_pair<M, N>& target, const std::pair<A, B>& source, F& f)
{
    detail::assign_member(target.first, source.first, f);
    detail::assign_member(target.second, source.second, f);
}

/// A variant member: the alternative it holds, at the same position.
template<std::size_t I, typename... Ms, typename... Bs, typename F>
void assign_alternative(flat_variant<Ms...>& target, const std::variant<Bs...>& source, F& f)
{
    if constexpr (I < sizeof...(Bs))
    {
        if (source.index() != I)
            return detail::assign_alternative<I + 1>(target, source, f);
        auto value = std::variant_alternative_t<I, std::variant<Ms...>>();
        detail::assign_member(value, std::get<I>(source), f);
        target = std::move(value);
    }
}

template<typename... Ms, typename... Bs, typename F>
void assign_member(flat_variant<Ms...>& target, const std::variant<Bs...>& source, F& f)
{
    static_assert(sizeof...(Ms) == sizeof...(Bs), "urd::map_children: a builder's variant member has its record's alternatives");
    detail::assign_alternative<0>(target, source, f);
}

/// A builder's fields written into its record's, in step (the same names, in the same order).
template<typename... Ds, typename... Bs, typename D, typename B, typename F>
void assign_fields(Fields<Ds...> /*record*/, Fields<Bs...> /*built*/, D& target, const B& source, F& f)
{
    static_assert(sizeof...(Ds) == sizeof...(Bs), "urd::map_children: a builder has its record's fields");
    static_assert(((Ds::name == Bs::name) && ...), "urd::map_children: a builder's fields are its record's, in order");
    (detail::assign_member(target.*Ds::member, source.*Bs::member, f), ...);
}

template<typename U, typename C, typename F>
void assign_built(data<U>& target, const builder<U, C>& source, F& f)
{ detail::assign_fields(data<U>::fields(), builder<U, C>::fields(), target, source, f); }
}  // namespace detail

/// Calls f(context.view(child)) for every child of record, in member order.
template<typename Types, typename T, typename R, typename C, typename F>
void for_each_child(const R& record, const C& context, F f)
{
    detail::walk_members<T>(record,
                            [&]<typename S>(S& child)
                                requires detail::is_child<Types, S> { f(context.view(child)); });
}

/// for_each_child of the record a view reads, through its own context: the way to visit a view's children without
/// handing its stored form out of urd (detail::view_access).
template<typename Types, typename T, typename V, typename F>
void for_each_child(const V& source, F f)
{ urd::for_each_child<Types, T>(detail::view_access::record(source), source.context(), std::move(f)); }

/// source with every child c replaced by what f(context.view(c)) reads, stored as c (urd::store), in member order, written into scratch, which
/// it returns: f returns an index of c's type, or a view of one. source is read, not copied (a record is move-only):
/// scratch's members are assigned from it (detail::assign_members), so a scratch reused across calls (a checked-out
/// builder) allocates nothing once its members have grown; scratch's own handle stays as it is.
template<typename Types, typename T, typename R, typename C, typename F>
R& map_children(const R& source, R& scratch, const C& context, F f)
{
    detail::assign_members<T>(scratch, source);
    detail::walk_members<T>(scratch,
                            [&]<typename S>(S& child)
                                requires detail::is_child<Types, S> { urd::store(child, f(context.view(child))); });
    return scratch;
}

/// The record of a builder, written into scratch (a reused record: its buffers kept, its own handle as it is): each
/// field in step with the builder's (urd::builder: the same names, in the same order), each child (urd::child: an index,
/// a built builder, or a record not interned yet) replaced by the index f gives it (f returns an index, a view of one, or
/// the pair a repository's insert returns), builders held in place as their records, std's containers as the flat ones.
template<typename T, typename C, typename F>
data<T>& map_children(const builder<T, C>& source, data<T>& scratch, F f)
{
    detail::assign_built(scratch, source, f);
    return scratch;
}

/// Interns a builder: its record written into scratch (map_children), each child as the key `insert` gives it, then the
/// record inserted. `insert` takes every form a child holds: an interned child's index_view (its key, where the
/// repository interned into holds it; else interned there from the view), a built one's builder (urd::intern again, with
/// a scratch record of its own) and a record not interned yet, of every kind the builder holds, and inserts it into the
/// repository that holds its kind; so built children first. Returns what insert(scratch) returns.
template<typename T, typename C, typename Insert>
decltype(auto) intern(const builder<T, C>& built, data<T>& scratch, Insert&& insert)
{
    urd::map_children(built,
                      scratch,
                      [&]<typename U, typename Cx, typename K>(const child<U, Cx, K>& held) -> K
                      {
                          if (const auto* interned = std::get_if<0>(&held))
                              return detail::as_key<K>(insert(*interned));
                          if (const auto* pooled = std::get_if<1>(&held))
                              return detail::as_key<K>(insert(std::as_const(pooled->held())));
                          return detail::as_key<K>(insert(std::get<2>(held).held()));
                      });
    return insert(scratch);
}

/// map_children of the record a view reads, through its own context: the way to rebuild a record from a view without
/// handing its stored form out of urd (detail::view_access).
template<typename Types, typename T, typename V, typename R, typename F>
R& map_children(const V& source, R& scratch, F f)
{ return urd::map_children<Types, T>(detail::view_access::record(source), scratch, source.context(), std::move(f)); }

/// map_children, but every flat_variant slot (one not inside another variant; any alternatives, children or not) is
/// replaced whole, in scratch: by f(urd::resolve(slot, context)) stored as the slot (urd::store), f called with the std::variant of the slot's
/// alternatives resolved (an empty slot throws std::bad_variant_access). f returns that variant, another of its
/// alternatives (a view or index of another alternative type), or anything else the slot's type stores or
/// one of its alternatives. The walk does not descend into a slot: the children (and nested variants) inside it are
/// f's to map. Children outside variants are replaced as map_children replaces them, so f is an overload set (or a
/// generic callback) taking both the views and the resolved variants.
template<typename Types, typename T, typename R, typename C, typename F>
R& map_variant_children(const R& source, R& scratch, const C& context, F f)
{
    detail::assign_members<T>(scratch, source);
    detail::walk_members<T>(scratch,
                            [&]<typename S>(S& slot)
                                requires(detail::is_child<Types, S> || is_flat_variant<S>)
                            { urd::store(slot, f(urd::resolve(std::as_const(slot), context))); });
    return scratch;
}

/// map_variant_children of the record a view reads, through its own context (detail::view_access).
template<typename Types, typename T, typename V, typename R, typename F>
R& map_variant_children(const V& source, R& scratch, F f)
{ return urd::map_variant_children<Types, T>(detail::view_access::record(source), scratch, source.context(), std::move(f)); }

}  // namespace urd
