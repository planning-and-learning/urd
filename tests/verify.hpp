#pragma once

// urd::verify: whether a container keeps its declared properties through random operations, by its kind. Each kind's
// check is in tests/verify/: sets and sequences, maps, queues, pools of rows and of slots, and records.

#include "urd/interface.hpp"
#include "urd/traits.hpp"
#include "verify/maps.hpp"
#include "verify/pools.hpp"
#include "verify/queues.hpp"
#include "verify/records.hpp"
#include "verify/sequences.hpp"

#include <concepts>
#include <optional>
#include <string>

namespace urd
{

/// Checks that `range` offers the operations container_traits<R> requires (interface.hpp; a compile-time
/// error otherwise) and keeps every declared property through them. It runs `steps` random operations on small
/// integers, each operation R's properties require, from inserts to erases of positions and ranges, and after
/// each one compares the contents and the lookups with a reference model and checks each declared property,
/// with the exceptions their definitions make. concurrent is not checked here: it needs several threads,
/// under ThreadSanitizer.
///
/// Returns the first violation, naming the step and what broke, or nothing.
template<typename R>
    requires detail::queue_traits<container_traits<R>> || has<container_traits<R>, handles> || std::constructible_from<detail::element_t<R>, int>
             || detail::map_container<R>
[[nodiscard]] std::optional<std::string> verify(R range, unsigned seed, int steps = 200)
{
    static_assert(conforming<R>, "urd::verify: R lacks an operation its declared properties require (include/urd/interface.hpp lists them)");
    if constexpr (!conforming<R>)
        return std::nullopt;
    else if constexpr (detail::queue_traits<container_traits<R>>)
        return detail::run_queue(range, seed, steps);
    else if constexpr (has<container_traits<R>, handles, unique, flat, layered>)
        return detail::run_layered_records(range, seed, steps);
    else if constexpr (has<container_traits<R>, handles, unique, flat>)
        return detail::run_records(range, seed, steps);
    else if constexpr (has<container_traits<R>, handles, unique>)
        return detail::run_pool(range, seed, steps);
    else if constexpr (has<container_traits<R>, handles>)
        return detail::run_slots(range, seed, steps);
    else if constexpr (detail::map_container<R>)
        return detail::run_map(range, seed, steps);
    else
        return detail::run(range, seed, steps);
}

}  // namespace urd
