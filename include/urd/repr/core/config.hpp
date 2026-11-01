#pragma once

#include <cassert>

/// States an invariant of side-effect free `condition`: asserted in Debug, assumed in Release (undefined
/// behavior if false, at no cost), and visible to static analysis in both, unlike a plain assert under NDEBUG.
#if defined(_MSC_VER)
#define URD_REPR_ASSUME(condition) (assert(condition), (condition) ? void(0) : __assume(0))
#else
#define URD_REPR_ASSUME(condition) (assert(condition), (condition) ? void(0) : __builtin_unreachable())
#endif
