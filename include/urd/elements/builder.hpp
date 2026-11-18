#pragma once

#include "urd/elements/data.hpp"
#include "urd/elements/handle.hpp"
#include "urd/elements/index.hpp"

#include <variant>

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

/// A child record T of a builder, read through the context C, in one of three forms: interned (its index; the none
/// index until one is given: a child default-constructs empty), built (a builder of its own), or a record of its own not
/// interned yet; the last two owned by the child, each held by its handle from a pool. urd resolves it as any variant
/// member: the view the context makes of the index, the builder's borrowed view, the record's view.
template<typename T, typename C>
using child = std::variant<index<T>, handle<builder<T, C>>, handle<data<T>>>;

/// The builder a child holds, to fill: a new one from `pool` (a Store of slots of builder<T, C>) where it holds no
/// builder (it is interned, or a record).
template<typename T, typename C, typename Pool>
builder<T, C>& builder_of(child<T, C>& held, Pool& pool)
{
    if (held.index() != 1 || std::get<1>(held) == nullptr)
        held = pool.make_handle();
    return *std::get<1>(held);
}

/// Whether R is a builder.
template<typename R>
inline constexpr bool is_builder = false;

template<typename T, typename C>
inline constexpr bool is_builder<builder<T, C>> = true;

}  // namespace urd
