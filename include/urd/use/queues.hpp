#pragma once

// The queues family: first in (a deque, or a ring that retains its storage), last in, and the least first (a binary
// heap, an addressable heap, a bucket queue). Include it where a Store of one is held. Pulls in nothing beyond the
// standard library.

#include "urd/repr/queues.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/use/queues.hpp">
{
};

}  // namespace urd
