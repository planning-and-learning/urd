#pragma once

#include "urd/props.hpp"

namespace urd
{

// One concept per property, to state what a function needs; they compose with &&:
//
//     template<typename R> requires urd::Sorted<R> && urd::Unique<R>
//     void apply(const R& changes);

template<typename R>
concept Sorted = has_props<R, props::sorted>;

template<typename R>
concept Unique = has_props<R, props::unique>;

template<typename R>
concept Nonempty = has_props<R, props::nonempty>;

template<typename R>
concept Contiguous = has_props<R, props::contiguous>;

template<typename R>
concept StableRefs = has_props<R, props::stable_refs>;

template<typename R>
concept Canonical = has_props<R, props::canonical>;

}  // namespace urd
