#pragma once

// The scopes family: a map with scopes, for symbol tables (pop_scope undoes every change since the matching
// push_scope). Include it where a Store of one is held, with the family of the map it scopes (hash, nodes or dense).
// Pulls in nothing itself.

#include "urd/repr/scoped.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/use/scopes.hpp">
{
};

}  // namespace urd
