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
{
    constexpr detail::property_set properties = container_traits<R>::urd_properties;  // a copy: GCC 13 fails on the member
    using of = urd::kind_of<properties, false, false>;
    if constexpr (kind_below<of, queue_kind>)
        return detail::run_queue(range, seed, steps);
    else if constexpr (std::same_as<of, records_kind> && has<container_traits<R>, layered>)
        return detail::run_layered_records(range, seed, steps);
    else if constexpr (std::same_as<of, records_kind>)
        return detail::run_records(range, seed, steps);
    else if constexpr (std::same_as<of, pool_kind>)
        return detail::run_pool(range, seed, steps);
    else if constexpr (std::same_as<of, slots_kind>)
        return detail::run_slots(range, seed, steps);
    else if constexpr (detail::map_container<R>)
        return detail::run_map(range, seed, steps);
    else
        return detail::run(range, seed, steps);
}

}  // namespace urd
