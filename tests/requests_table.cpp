// Writes benchmarks/requests.json from the request grid, or checks the committed file against it:
//
//     requests_table --write benchmarks/requests.json
//     requests_table --check benchmarks/requests.json     (the `requests` test)
//
// `requests`: each request, its element shape, the operations its Store offers, and the containers serving it in the
// registry's order (the first is the one chosen). benchmarks/record.py judges the registry's choices from these,
// share/urd/complexity.json and the measured results.
//
// `experiments`: what is measured. Each container on each shape of element, for the fewest requests whose operations
// cover those of every request it serves on that shape (usually one; a second is "#2"), at the sizes of
// benchmarks/cells.hpp. `containers` lists each one's experiments as (group, position, ordinal) in the grid and
// `families` its family headers: CMake compiles one benchmark source per container from them
// (benchmarks/experiment.cpp), which includes and instantiates that container only.
//
// The groups are computed in their own objects (tests/requests_group.cpp); this merges them.

#include "generated.hpp"
#include "grid.hpp"
#include "requests.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#if defined(URD_GROUPS)
static_assert(URD_GROUPS == urd::test::request_groups, "requests_table: the grid's groups changed; set URD_REQUEST_GROUPS in CMakeLists.txt to their number");
#endif

namespace
{

std::string json_string(std::string_view text)
{
    auto result = std::string("\"");
    for (const char each : text)
    {
        if (each == '"' || each == '\\')
            result += '\\';
        result += each;
    }
    return result + "\"";
}

std::string json_operations(const urd::test::offers& operations)
{
    auto result = std::string("[");
    for (std::size_t each = 0; each < operations.size(); ++each)
        if (operations[each])
            result += (result.size() > 1 ? ", " : "") + json_string(urd::test::op_names[each]);
    return result + "]";
}

template<typename Items, typename Write>
std::string json_list(const Items& items, Write write)
{
    auto result = std::string();
    for (const auto& each : items)
        result += (result.empty() ? "" : ", ") + write(each);
    return result;
}

std::size_t count(const urd::test::offers& operations) { return static_cast<std::size_t>(std::ranges::count(operations, true)); }

/// Where a container is served on one shape of element: the request rows, in the grid's order.
struct served
{
    std::string container;
    std::vector<std::pair<const urd::test::request_row*, const urd::test::server*>> rows;
    std::vector<std::size_t> groups;
};

struct experiment
{
    std::string container;
    const urd::test::request_row* row = nullptr;
    const urd::test::server* server = nullptr;
    std::size_t group = 0;
    std::size_t ordinal = 1;
};

std::string json_server(const urd::test::server& each)
{
    auto result = "{\"container\": " + json_string(each.container);
    if (!each.part.empty())
        result += ", \"part\": " + json_string(each.part);
    result += ", \"words\": " + std::to_string(each.words);
    if (!each.uninvited.empty())
        result += ", \"uninvited\": [" + json_list(each.uninvited, json_string) + "]";
    return result + "}";
}

std::string generate()
{
    const auto all = []<std::size_t... G>(std::index_sequence<G...>)
    { return std::vector<urd::test::request_group> { urd::test::requests_of<G>()... }; }(std::make_index_sequence<urd::test::request_groups>());
    auto where = std::map<std::string, served>();
    auto out = std::string("{\"generated_by\": \"requests_table --write benchmarks/requests.json, from tests/grid.hpp\", \"requests\": [\n");
    bool first = true;
    for (std::size_t group = 0; group < all.size(); ++group)
        for (const auto& row : all[group].rows)
        {
            out += first ? "  " : ",\n  ";
            first = false;
            out += "{\"request\": " + json_string(row.request) + ", \"shape\": " + json_string(row.shape) + ", \"small\": " + (row.small ? "true" : "false")
                   + ", \"operations\": " + json_operations(row.operations) + ", \"serving\": [" + json_list(row.serving, json_server) + "]}";
            for (const auto& each : row.serving)
            {
                auto& at = where[each.container + "/" + row.shape + (row.small ? "/small" : "")];
                at.container = each.container;
                at.rows.emplace_back(&row, &each);
                at.groups.push_back(group);
            }
        }
    // Each container on each shape: the fewest requests whose operations cover all its requests' (greedily, the one
    // adding the most first, the earliest of equals); a second one is "#2", and so on.
    auto experiments = std::map<std::string, experiment>();
    for (const auto& [id, at] : where)
    {
        auto needed = urd::test::offers {};
        for (const auto& [row, server] : at.rows)
            for (std::size_t each = 0; each < needed.size(); ++each)
                needed[each] = needed[each] || row->operations[each];
        for (std::size_t ordinal = 1; count(needed) > 0; ++ordinal)
        {
            std::size_t best = 0;
            std::size_t adds = 0;
            for (std::size_t each = 0; each < at.rows.size(); ++each)
            {
                std::size_t added = 0;
                for (std::size_t what = 0; what < needed.size(); ++what)
                    added += needed[what] && at.rows[each].first->operations[what] ? 1U : 0U;
                if (added > adds)
                {
                    best = each;
                    adds = added;
                }
            }
            const auto* row = at.rows[best].first;
            for (std::size_t what = 0; what < needed.size(); ++what)
                needed[what] = needed[what] && !row->operations[what];
            experiments[id + (ordinal == 1 ? "" : "#" + std::to_string(ordinal))] =
                experiment { at.container, row, at.rows[best].second, at.groups[best], ordinal };
        }
    }
    auto positions = std::map<std::string, std::string>();
    auto families = std::map<std::string, std::vector<std::string>>();
    out += "\n], \"experiments\": {\n";
    first = true;
    for (const auto& [id, each] : experiments)
    {
        out += (first ? "  " : ",\n  ") + json_string(id) + ": {\"container\": " + json_string(each.container)
               + ", \"request\": " + json_string(each.row->request) + ", \"operations\": " + json_operations(each.row->operations) + ", \"sizes\": ["
               + json_list(each.row->sizes, [](std::size_t size) { return std::to_string(size); }) + "]}";
        first = false;
        auto& at = positions[each.container];
        at += (at.empty() ? "" : ", ") + std::to_string(each.group) + ", " + std::to_string(each.row->position) + ", " + std::to_string(each.ordinal);
        auto& headers = families[each.container];
        for (const auto& family : each.server->families)
            if (std::ranges::find(headers, family) == headers.end())
                headers.push_back(family);
    }
    out += "\n}, \"containers\": {\n";
    first = true;
    for (const auto& [container, at] : positions)
    {
        out += (first ? "  " : ",\n  ") + json_string(container) + ": [" + at + "]";
        first = false;
    }
    out += "\n}, \"families\": {\n";
    first = true;
    for (const auto& [container, at] : positions)
    {
        out += (first ? "  " : ",\n  ") + json_string(container) + ": [" + json_list(families[container], json_string) + "]";
        first = false;
    }
    return out + "\n}}\n";
}

}  // namespace

int main(int argc, char** argv) { return urd::test::write_or_check(argc, argv, "requests_table", generate); }
