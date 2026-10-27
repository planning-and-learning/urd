#pragma once

#include "urd/traits.hpp"

namespace urd
{

// One concept per property, to state what a function needs; they compose with &&:
//
//     template<typename R> requires urd::Sorted<R> && urd::Unique<R>
//     void apply(const R& changes);

template<typename R>
concept Sorted = has<R, sorted>;

template<typename R>
concept Unique = has<R, unique>;

template<typename R>
concept Nonempty = has<R, nonempty>;

template<typename R>
concept Contiguous = has<R, contiguous>;

template<typename R>
concept StableRefs = has<R, stable_refs>;

template<typename R>
concept Canonical = has<R, canonical>;

}  // namespace urd
