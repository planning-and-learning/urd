#pragma once

#include "urd/store.hpp"
#include "urd/traits.hpp"

namespace urd
{

// One concept per property, to state what a function needs of a Store; they compose with &&. Only Stores
// qualify: downstream code uses data through a Store, which offers what its properties allow.
//
//     template<typename R> requires urd::Sorted<R> && urd::Unique<R>
//     void apply(const R& changes);

template<typename R>
concept Sorted = is_store<R> && has<R, sorted>;

template<typename R>
concept Unique = is_store<R> && has<R, unique>;

template<typename R>
concept Nonempty = is_store<R> && has<R, nonempty>;

template<typename R>
concept Contiguous = is_store<R> && has<R, contiguous>;

template<typename R>
concept StableRefs = is_store<R> && has<R, stable_refs>;

template<typename R>
concept Canonical = is_store<R> && has<R, canonical>;

}  // namespace urd
