#pragma once

// urd::verify: whether a container keeps its declared properties through random operations, by its kind. Each kind's
// check is in tests/verify/: sets and sequences, maps, queues, pools of rows and of slots, and records.

#include "urd/traits.hpp"
#include "verify/maps.hpp"
#include "verify/pools.hpp"
#include "verify/queues.hpp"
#include "verify/records.hpp"
#include "verify/sequences.hpp"

#include <optional>
#include <string>

namespace urd
{

/// Checks that `range` keeps every declared property through random operations: `steps` of them on small integers,
/// each operation R's properties allow, from inserts to erases of positions and ranges, and after each one compares
/// the contents and the lookups with a reference model and checks each declared property, with the exceptions their
/// definitions make. concurrent is not checked here: it needs several threads, under ThreadSanitizer.
///
/// Returns the first violation, naming the step and what broke, or nothing.
template<typename R>
[[nodiscard]] std::optional<std::string> verify(R range, unsigned seed, int steps = 200)
{ return detail::verifier<typename detail::kind_of_range<R>::type>::run(range, seed, steps); }

}  // namespace urd
