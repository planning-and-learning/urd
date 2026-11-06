#pragma once

namespace urd
{

/// The mutable form of a T, filled in place before T is stored or read (a state while it is expanded, a denotation
/// while it is computed). Specialize it per type; views read it as urd::borrowed_view<T, C, builder<T>> (or
/// shared_view, owned_view), with the accessors of view_mixin<builder<T>, V>.
template<typename T>
struct builder;

/// Whether T is a builder<…>.
template<typename T>
inline constexpr bool is_builder = false;
template<typename T>
inline constexpr bool is_builder<builder<T>> = true;

}  // namespace urd
