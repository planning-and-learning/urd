#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace urd
{

/// urd's word: the unsigned integer that bit-packed storage is made of (rows of a bit width or of column widths, sets
/// kept as bits), and that a packed value is read and written as.
using word_t = std::uint64_t;

/// Its bits, derived from it: the most a packed value takes. No code spells them as a number.
inline constexpr std::size_t word_bits = std::numeric_limits<word_t>::digits;

}  // namespace urd

// NOLINTBEGIN(cppcoreguidelines-macro-usage): each must expand at its call site
/// States an invariant of side-effect free `condition`: asserted in Debug, assumed in Release (undefined
/// behavior if false, at no cost), and visible to static analysis in both, unlike a plain assert under NDEBUG
/// (a function would hide the condition from the analysis of optionals).
#if defined(_MSC_VER)
#define URD_ASSUME(condition) (assert(condition), (condition) ? void(0) : __assume(0))
#else
#define URD_ASSUME(condition) (assert(condition), (condition) ? void(0) : __builtin_unreachable())
#endif

/// Keeps a rarely taken helper out of line, so its size does not count towards the inlined size of its caller.
#if defined(_MSC_VER)
#define URD_NOINLINE __declspec(noinline)
#else
#define URD_NOINLINE [[gnu::noinline]]
#endif

/// Marks a parameter that the function's result refers to, so the compiler can warn when the result outlives a
/// temporary argument (Clang); empty where the attribute is unknown.
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::lifetimebound)
#define URD_LIFETIMEBOUND [[clang::lifetimebound]]
#endif
#endif
#ifndef URD_LIFETIMEBOUND
#define URD_LIFETIMEBOUND
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)
