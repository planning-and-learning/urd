#pragma once

// The memory a Store holds: urd::memory_usage(store), as for any value (urd/memory.hpp).

#include "urd/memory.hpp"
#include "urd/store/types.hpp"

#include <cstddef>

namespace urd
{

/// A Store holds what its container holds.
template<typename R, typename S>
struct memory_of<detail::Store<R, S>>
{
    static std::size_t bytes(const detail::Store<R, S>& store) { return memory_usage(detail::store_access::representation(store)); }
};

}  // namespace urd
