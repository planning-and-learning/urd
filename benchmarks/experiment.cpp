// One container's benchmarks: its experiments (benchmarks/cells.hpp), each the container's lifecycle on one shape of
// element at each size (benchmarks/lifecycle.hpp). CMake compiles this file once per container listed in
// benchmarks/requests.json, through a generated source that includes the container's family headers first (only
// those: a header costs its compile whether used or not), defines
//
//     URD_CONTAINER    its id (urd::test::name<R>::id)
//     URD_EXPERIMENTS  its experiments, each as the group and position in the grid of the request it is for, and
//                      its ordinal among the container's experiments on that shape (the second is "#2"): g, p, o, ...
//
// and calls register_experiments() from the function the generated main calls. So each translation unit instantiates
// the Stores of one container only, for at most a few of its experiments (a container measured on many element types
// is several objects), and adding a container adds its own. Compiled alone (by a linter, without those
// definitions), the hash set of `Store<key<int>>` stands in.

#if !defined(URD_CONTAINER)
#include "urd/use/hash.hpp"
#endif

#include "lifecycle.hpp"
#include "urd/registry.hpp"

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>

namespace
{

#if defined(URD_CONTAINER)
constexpr urd::detail::fixed_string container = URD_CONTAINER;
constexpr std::array<std::size_t, std::size(std::initializer_list<std::size_t> { URD_EXPERIMENTS })> positions { URD_EXPERIMENTS };
#else
constexpr urd::detail::fixed_string container = "unordered_flat_set";
constexpr std::array<std::size_t, 3> positions { 0, 0, 1 };
#endif

template<std::size_t Group, std::size_t Position, std::size_t Ordinal>
void register_one()
{
    using S = typename urd::test::request_at<Group, Position>::spec;
    using R = typename urd::test::serving_named<container, S>::type;
    static_assert(urd::test::offered<R, S>() == urd::test::offered<S>(),
                  "benchmarks/experiment.cpp: spec_switches (tests/complexity.hpp) no longer agrees with facet_switches");
    urd::bench::register_experiment<R, S>(Ordinal);
}

/// Registers this container's cells.
[[maybe_unused]] void register_experiments()
{
    [&]<std::size_t... I>(std::index_sequence<I...>)
    { (register_one<positions[3 * I], positions[(3 * I) + 1], positions[(3 * I) + 2]>(), ...); }(std::make_index_sequence<positions.size() / 3>());
}

}  // namespace
