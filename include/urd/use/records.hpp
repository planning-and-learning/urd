#pragma once

// The records family: pools of records, each one block of bytes read by index. The record interner keeps trivially
// copyable records in an indexed set; the arena interner serializes any other record with cista into a byte arena
// that never moves a block. Include it where a Store of such a pool is held (urd/flat.hpp gives the vocabulary flat
// records are written with). Pulls in Boost.Unordered and cista.

#include "urd/repr/arena.hpp"
#include "urd/repr/records.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/use/records.hpp">
{
};

}  // namespace urd
