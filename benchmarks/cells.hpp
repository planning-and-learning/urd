#pragma once

// What is measured, named: an experiment is one container's lifecycle on one shape of element (and small or not),
// "<container>/<key type>;<value type>[/small][#k]"; a cell is one experiment at one size, "<experiment>/<n>". The
// list of experiments (tests/requests_table.cpp) and each container's benchmark (benchmarks/experiment.cpp) both name
// them here, so they cannot disagree. Every container is measured on its own; comparing two is reading their results
// (benchmarks/record.py), never measuring them together.

#include "../tests/complexity.hpp"
#include "../tests/elements.hpp"
#include "../tests/grid.hpp"
#include "urd/format.hpp"
#include "urd/store.hpp"

#include <array>
#include <cstddef>
#include <fmt/format.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace urd::test
{

/// The sizes measured: elements; for a small request, its sizes; for a pool, a row's length.
inline constexpr std::array<std::size_t, 3> sizes { 1000, 10000, 100000 };
inline constexpr std::array<std::size_t, 2> small_sizes { 8, 64 };
inline constexpr std::array<std::size_t, 3> row_lengths { 16, 256, 4096 };

/// The shape of a request's elements: its key and value types as the request writes them, "<key>;<value>" ("-" for
/// none). Only containers measured on the same types are compared.
template<typename S>
std::string shape_of_spec()
{ return fmt::format("{};{}", urd::spelling<typename S::key_type>(), urd::spelling<typename S::value_type>()); }

/// The experiment of container R for the request spec S.
template<typename S>
std::string experiment_of(std::string_view container)
{ return std::string(container) + "/" + shape_of_spec<S>() + (urd::has<S, urd::small> ? "/small" : ""); }

template<typename S>
std::vector<std::size_t> sizes_of()
{
    if constexpr (urd::has<S, urd::small>)
        return { small_sizes.begin(), small_sizes.end() };
    else if constexpr (S::is_pool)
        return { row_lengths.begin(), row_lengths.end() };
    else
        return { sizes.begin(), sizes.end() };
}

/// One experiment at one size: every operation its Store offers, timed in turn on the same Store.
struct cell
{
    std::string id;
    offers operations {};
    std::size_t size = 0;
};

}  // namespace urd::test
