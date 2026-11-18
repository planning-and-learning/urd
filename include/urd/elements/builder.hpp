#pragma once

#include "urd/elements/handle.hpp"
#include "urd/elements/index.hpp"

#include <variant>

namespace urd
{

/// The mutable form of a T read through a context C, filled in place before T is stored or read (a state while it is
/// expanded, a denotation while it is computed). It has the members of its record data<T>, under the same names and in
/// the same order: the same record in another memory organization. Where the record holds an index of a child record,
/// the builder holds the child (urd::child: interned, as the view C makes of it, or built, as a builder of its own);
/// the other members as the record holds them, or in a mutable representation of their own. Specialize
/// it per type; views read it as their record, urd::borrowed_view<T, C, builder<T, C>> (or owning_view, shared_view),
/// with the accessors of view_mixin<builder<T, C>, V>.
template<typename T, typename C>
struct builder;

/// A child record T of a builder, read through the context C: built, as a builder of its own held by its handle (none
/// until one is given: a child default-constructs empty), or interned, as the view V that C makes of its index (an
/// index_view, unless C makes another: a packed state's view of a state).
template<typename T, typename C, typename V = index_view<T, C>>
using child = std::variant<handle<builder<T, C>>, V>;

/// The builder a child holds, to fill: a new one from `pool` (a Store of slots of builder<T, C>) where it holds none,
/// or holds its interned form.
template<typename T, typename C, typename V, typename Pool>
builder<T, C>& builder_of(child<T, C, V>& held, Pool& pool)
{
    if (held.index() != 0 || std::get<0>(held) == nullptr)
        held = pool.make_handle();
    return *std::get<0>(held);
}

/// Whether R is a builder.
template<typename R>
inline constexpr bool is_builder = false;

template<typename T, typename C>
inline constexpr bool is_builder<builder<T, C>> = true;

}  // namespace urd
