#pragma once

// Containers for keys that are indices 0..n-1 (dense keys): stored by position instead of hashed. They grow to fit
// any key inserted. The registry names them for any element type, so they are unconstrained templates; their
// container_traits declare properties only for indexable keys. One header per container.

#include "urd/repr/dense_set.hpp"
#include "urd/repr/index_map.hpp"
#include "urd/repr/sparse_set.hpp"
