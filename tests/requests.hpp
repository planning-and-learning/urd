#pragma once

// What one group of the request grid contributes to benchmarks/requests.json: its requests, with their shape, the
// operations their Store offers and the containers serving them in the registry's order, and each serving container's
// family headers. tests/requests_group.cpp computes it, compiled once per group (URD_GROUP), so no translation unit
// walks the whole grid; tests/requests_table.cpp merges the groups.

#include "cells.hpp"
#include "grid.hpp"

#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

namespace urd::test
{

/// The number of groups in the grid: CMake compiles one object per group (URD_REQUEST_GROUPS, which
/// tests/requests_table.cpp checks against this).
inline constexpr std::size_t request_groups = std::tuple_size_v<groups>;

/// A container serving a request, with the family headers a Store of it for that request needs: its own (a wrapper's
/// part first), then those of the Stores among its elements.
struct server
{
    std::string container;
    std::vector<std::string> families;
    /// What a wrapper passes its costs on to for this request (one id may wrap different parts), and the memory in
    /// words per element with it (tests/complexity.hpp).
    std::string part;
    double words = 0;
    /// The costly guarantees it gives that the request did not ask for (urd::test::costly).
    std::vector<std::string> uninvited;
};

struct request_row
{
    std::string request;
    std::string shape;
    bool small = false;
    offers operations {};
    std::vector<server> serving;
    std::size_t position = 0;
    std::vector<std::size_t> sizes;
};

struct request_group
{
    std::vector<request_row> rows;
};

/// Group G's requests.
template<std::size_t G>
request_group requests_of();

}  // namespace urd::test
