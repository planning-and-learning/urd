#pragma once

#include "urd/interface.hpp"
#include "urd/traits.hpp"

namespace urd
{

// One concept per property, to state what a function needs: R declares the property and offers the operations
// its properties require, so the function can use them. They compose with &&:
//
//     template<typename R> requires urd::Sorted<R> && urd::Unique<R>
//     void apply(const R& changes);

template<typename R>
concept Sorted = has<R, sorted> && conforming<R>;

template<typename R>
concept Unique = has<R, unique> && conforming<R>;

template<typename R>
concept Nonempty = has<R, nonempty> && conforming<R>;

template<typename R>
concept Contiguous = has<R, contiguous> && conforming<R>;

template<typename R>
concept StableRefs = has<R, stable_refs> && conforming<R>;

template<typename R>
concept Canonical = has<R, canonical> && conforming<R>;

}  // namespace urd
