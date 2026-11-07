#pragma once

namespace urd
{

/// The mutable form of a T, filled in place before T is stored or read (a state while it is expanded, a denotation
/// while it is computed). Specialize it per type; views read it as their record, urd::borrowed_view<T, C, builder<T>>
/// (or owning_view, shared_view), with the accessors of view_mixin<builder<T>, V>.
template<typename T>
struct builder;

}  // namespace urd
