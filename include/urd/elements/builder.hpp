#pragma once

#include "urd/elements/data.hpp"
#include "urd/elements/handle.hpp"
#include "urd/elements/index.hpp"

#include <boost/mp11/algorithm.hpp>
#include <concepts>
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

/// A child record T of a builder, in one of three forms: interned (its key K: key<T>, or a key of its own such as a
/// row's; the none key until one is given: a child default-constructs empty), built (a builder of its own), or a record
/// of its own not interned yet; the last two owned by the child through their handles (urd::handle: from a pool, or
/// make_owned). No form carries a context: a child is read through the context of the view that reads its builder,
/// which urd resolves as any variant member (a key as the view its context makes, a handle as its record).
/// C is the context type of the builder that holds it: what a built child's builder is read through.
template<typename T, typename C, typename K = key_t<T>>
using interned_child = K;  ///< A child's interned form: its key.
template<typename T, typename C>
using built_child = handle<builder<T, C>>;  ///< A child's built form; alone, a field a builder holds only built (never null).
template<typename T, typename C>
using held_child = handle<data<T>>;  ///< A child's form of a record of its own, not interned yet.

template<typename T, typename C, typename K = key_t<T>>
using child = std::variant<interned_child<T, C, K>, built_child<T, C>, held_child<T, C>>;

namespace detail
{
template<typename B>
inline constexpr bool child_type = false;
template<typename U, typename C, typename K>
inline constexpr bool child_type<child<U, C, K>> = true;

template<typename B>
inline constexpr bool built_child_type = false;
template<typename U, typename C>
inline constexpr bool built_child_type<built_child<U, C>> = true;
}  // namespace detail

/// Whether B is a child (urd::child): a record's key as a builder holds it, in one of its three forms.
template<typename B>
concept Child = detail::child_type<B>;

/// Whether B is a child held only built (urd::built_child): a builder's handle, never null.
template<typename B>
concept BuiltChild = detail::built_child_type<B>;

/// The builder a child holds, to fill: where it holds none (it is interned or a record), the one `make` gives (a
/// function of no argument that returns a built_child: from a pool, a storage's checkout, the heap).
template<typename T, typename C, typename K, std::invocable Make>
builder<T, C>& builder_of(child<T, C, K>& held, Make make)
{
    if (!std::holds_alternative<built_child<T, C>>(held))
        held.template emplace<built_child<T, C>>(make());
    return *std::get<built_child<T, C>>(held);
}

/// Whether Pool hands out handles of its slots (a Store of slots: make_handle()).
template<typename Pool>
concept HandsOutHandles = requires(Pool& pool) { pool.make_handle(); };

/// The builder a child holds, to fill: a new one from `pool` (a Store of slots of builder<T, C>) where it holds none.
template<typename T, typename C, typename K, HandsOutHandles Pool>
builder<T, C>& builder_of(child<T, C, K>& held, Pool& pool)
{
    return urd::builder_of(held, [&pool]() -> built_child<T, C> { return pool.make_handle(); });
}

/// The builder a child holds, to replace whole (clear, assign, unpack into): a new one of its own on the heap
/// (urd::make_owned) where it holds none.
template<typename T, typename C, typename K>
builder<T, C>& builder_of(child<T, C, K>& held)
{
    return urd::builder_of(held, []() -> built_child<T, C> { return urd::make_owned<builder<T, C>>(); });
}

/// The builder a child holds built, to write in place: std::bad_variant_access where it holds its key or a record (its
/// context unpacks it first).
template<typename T, typename C, typename K>
builder<T, C>& built(child<T, C, K>& held)
{ return *std::get<built_child<T, C>>(held); }

template<typename T, typename C, typename K>
const builder<T, C>& built(const child<T, C, K>& held)
{ return *std::get<built_child<T, C>>(held); }

/// The contents of a builder, taken out of it: `built` is left as a new one (its handles made anew, never null), fit to
/// be filled again. What keeps a builder that its owner reuses (a pooled one, a generator's): moving it out would leave
/// its handles null.
template<Builder B>
[[nodiscard]] B take(B& built)
{
    auto taken = B();
    std::swap(taken, built);
    return taken;
}

}  // namespace urd
