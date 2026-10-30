#pragma once

// The layers family: a set, map or pool over a parent it neither owns nor changes, answering lookups from both.
// Include it where a Store of one is held, with the family of what is layered. Pulls in nothing itself.

#include "urd/repr/layered.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/use/layers.hpp">
{
};

}  // namespace urd
