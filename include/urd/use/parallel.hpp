#pragma once

// The parallel family: gtl's sharded hash tables, which threads insert into and look up in at once (flat, or with a
// node per element so that references hold). Include it where a Store of one is held. Pulls in gtl; a build without
// gtl (URD_WITH_GTL off) has no concurrent sets or maps by hashed keys.

#if !defined(URD_HAS_GTL)
#error "urd/use/parallel.hpp: this build has no gtl (URD_WITH_GTL is off), so no concurrent set or map by hashed keys"
#endif

#include "urd/repr/parallel.hpp"
#include "urd/traits.hpp"

#include <gtl/phmap.hpp>

namespace urd
{

template<>
struct include<"urd/use/parallel.hpp">
{
};

}  // namespace urd
