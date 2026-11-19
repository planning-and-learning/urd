#pragma once

#include "urd/elements/data.hpp"
#include "urd/elements/handle.hpp"
#include "urd/elements/index.hpp"

#include <boost/mp11/algorithm.hpp>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace urd
{

/// The mutable form of a T read through a context C, filled in place before T is stored or read (a state while it is
/// expanded, a denotation while it is computed). It has the members of its record data<T>, under the same names and in
/// the same order: the same record in another memory organization. Where the record holds an index of a child record,
/// the builder holds the child (urd::child: interned, built, or a record not interned yet);
/// the other members as the record holds them, or in a mutable representation of their own. Specialize
/// it per type; views read it as their record, urd::borrowed_view<T, C, builder<T, C>> (or owning_view, shared_view),
/// with the accessors of view_mixin<builder<T, C>, V>.
template<typename T, typename C>
struct builder;

/// A child record T of a builder, as a view through its own context C, in one of three forms: interned (its index_view;
/// the none view until one is given: a child default-constructs empty), built (a builder of its own), or a record of
/// its own not interned yet; the last two owned by the child (pooled_view: held by its handle from a pool). Every form
/// carries its context, so that a child reads as it is, whoever reads it: urd resolves it as any variant member, the
/// index_view as it is, a pooled view as its record's borrowed view through its own context. K is the key of the
/// interned form, as index_view's: index<T>, or a key of its own (a row's: its pool and its id).
template<typename T, typename C, typename K = index<T>>
using interned_child = index_view<T, C, K>;  ///< A child's interned form.
template<typename T, typename C>
using built_child = pooled_view<T, C, builder<T, C>>;  ///< A child's built form.
template<typename T, typename C>
using held_child = pooled_view<T, C, data<T>>;  ///< A child's form of a record of its own, not interned yet.

template<typename T, typename C, typename K = index<T>>
using child = std::variant<interned_child<T, C, K>, built_child<T, C>, held_child<T, C>>;

namespace detail
{
template<typename C>
struct child_in
{
    template<typename T>
    using fn = child<T, C>;
};
}  // namespace detail

/// The built form of an index_variant<L> (a flat_variant of the indices of L's tags): the std::variant of their
/// children, read through C, in the same order.
template<typename L, typename C>
using child_variant = boost::mp11::mp_apply<std::variant, boost::mp11::mp_transform<detail::child_in<C>::template fn, L>>;

/// The builder a child holds, to fill: a new one from `pool` (a Store of slots of builder<T, C>), read through
/// `context`, where it holds no builder (it is interned, or a record).
template<typename T, typename C, typename K, typename Pool>
builder<T, C>& builder_of(child<T, C, K>& held, Pool& pool, const C& context)
{
    if (!std::holds_alternative<built_child<T, C>>(held))
        held.template emplace<built_child<T, C>>(pool.make_handle(), &context);
    return std::get<built_child<T, C>>(held).held();
}

/// Whether R is a builder.
template<typename R>
inline constexpr bool is_builder = false;

template<typename T, typename C>
inline constexpr bool is_builder<builder<T, C>> = true;

}  // namespace urd
