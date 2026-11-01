#pragma once

// How the tests write what is theirs, with fmt: JSON as the generators write it (share/urd/*.json,
// benchmarks/requests.json). urd's own types are written by urd/format.hpp; the grid's element types are spelled beside
// them (tests/elements.hpp).

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <ranges>
#include <string>
#include <string_view>

namespace urd::test
{

/// A string in JSON: quoted, its quotes, backslashes and control characters escaped (fmt's escaped string). Not
/// `quoted`: a call with a std::string would find std::quoted by argument.
inline std::string json_text(std::string_view text) { return fmt::format("{:?}", text); }

/// Items in JSON, each written by `write`, separated by commas: the inside of an array.
template<typename Items, typename Write>
std::string joined(const Items& items, Write write)
{ return fmt::format("{}", fmt::join(items | std::views::transform(write), ", ")); }

}  // namespace urd::test
