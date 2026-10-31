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
// `headers` the headers it needs: CMake compiles one benchmark source per container from them
// (benchmarks/experiment.cpp), which includes and instantiates that container only. `tested` lists, per container,
// the requests its property tests run for (group, position, ...): those over ints that are no queue, pool or
// partition; CMake compiles one test object per container from them (tests/container.cpp).
//
// The groups are computed in their own objects (tests/requests_group.cpp); this merges them.

#include "format.hpp"
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

using urd::test::joined;
using urd::test::json_text;
using urd::test::op;

std::string json_operations(const urd::test::offers& operations)
{
    auto names = std::vector<std::string_view>();
    for (std::size_t each = 0; each < operations.size(); ++each)
        if (operations[each])
            names.push_back(urd::test::op_names[each]);
    return "[" + joined(names, json_text) + "]";
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
    auto result = "{\"container\": " + json_text(each.container);
    if (!each.part.empty())
        result += ", \"part\": " + json_text(each.part);
    result += ", \"words\": " + std::to_string(each.words);
    if (!each.uninvited.empty())
        result += ", \"uninvited\": [" + joined(each.uninvited, json_text) + "]";
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
            out += "{\"request\": " + json_text(row.request) + ", \"shape\": " + json_text(row.shape) + ", \"small\": " + (row.small ? "true" : "false")
                   + ", \"operations\": " + json_operations(row.operations) + ", \"serving\": [" + joined(row.serving, json_server) + "]}";
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
    auto headers_of = std::map<std::string, std::vector<std::string>>();
    auto tested = std::map<std::string, std::string>();
    out += "\n], \"experiments\": {\n";
    first = true;
    for (const auto& [id, each] : experiments)
    {
        out += (first ? "  " : ",\n  ") + json_text(id) + ": {\"container\": " + json_text(each.container) + ", \"request\": " + json_text(each.row->request)
               + ", \"operations\": " + json_operations(each.row->operations) + ", \"cheap\": " + (each.row->cheap ? "true" : "false") + ", \"sizes\": ["
               + joined(each.row->sizes, [](std::size_t size) { return std::to_string(size); }) + "]}";
        first = false;
        auto& at = positions[each.container];
        at += (at.empty() ? "" : ", ") + std::to_string(each.group) + ", " + std::to_string(each.row->position) + ", " + std::to_string(each.ordinal);
        auto& headers = headers_of[each.container];
        for (const auto& header : each.server->headers)
            if (std::ranges::find(headers, header) == headers.end())
                headers.push_back(header);
        // The property suite (tests/properties.hpp) builds a container from ints: a set or sequence of them, or a map
        // of them; not a queue, a pool or a partition.
        const auto& offers = each.row->operations;
        const auto of = [&](op what) { return offers[static_cast<std::size_t>(what)]; };
        const bool ints = each.row->shape == "int;-" || each.row->shape == "-;int" || each.row->shape == "int;int";
        if (ints && !of(op::push) && !of(op::insert_row) && !of(op::merge))
        {
            auto& listed = tested[each.container];
            listed += (listed.empty() ? "" : ", ") + std::to_string(each.group) + ", " + std::to_string(each.row->position);
        }
    }
    out += "\n}, \"containers\": {\n";
    first = true;
    for (const auto& [container, at] : positions)
    {
        out += (first ? "  " : ",\n  ") + json_text(container) + ": [" + at + "]";
        first = false;
    }
    out += "\n}, \"headers\": {\n";
    first = true;
    for (const auto& [container, at] : positions)
    {
        out += (first ? "  " : ",\n  ") + json_text(container) + ": [" + joined(headers_of[container], json_text) + "]";
        first = false;
    }
    out += "\n}, \"tested\": {\n";
    first = true;
    for (const auto& [container, at] : tested)
    {
        out += (first ? "  " : ",\n  ") + json_text(container) + ": [" + at + "]";
        first = false;
    }
    return out + "\n}}\n";
}

}  // namespace

int main(int argc, char** argv) { return urd::test::write_or_check(argc, argv, "requests_table", generate); }
