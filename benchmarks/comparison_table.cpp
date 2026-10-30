// Writes benchmarks/comparisons.json from the registry's claims (benchmarks/comparisons.hpp), or checks the committed file
// against them:
//
//     comparison_table --write benchmarks/comparisons.json
//     comparison_table --check benchmarks/comparisons.json     (the `comparisons` test)
//
// One line per comparison, by id: the two representations, the outcome, the first request it stands for and how many, and the
// benchmark cells it reads; then every cell. benchmarks/comparisons.cpp registers exactly these cells, and
// benchmarks/record.py measures them and checks each comparison against them.

#include "comparisons.hpp"
#include "generated.hpp"

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>

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

std::string json_list(const std::vector<std::string>& items)
{
    auto result = std::string("[");
    for (const auto& each : items)
    {
        if (result.size() > 1)
            result += ", ";
        result += json_string(each);
    }
    return result + "]";
}

std::vector<std::string> ids(const std::vector<urd::test::cell>& cells)
{
    auto result = std::vector<std::string>();
    for (const auto& each : cells)
        result.push_back(each.id);
    return result;
}

std::string generate()
{
    auto out = std::string("{\"generated_by\": \"comparison_table --write benchmarks/comparisons.json, from benchmarks/comparisons.hpp\", "
                           "\"tolerance\": 0.1, \"comparisons\": [\n");
    auto cells = std::set<std::string>();
    bool first = true;
    for (const auto& [id, found] : urd::test::all_comparisons())
    {
        out += first ? "  " : ",\n  ";
        first = false;
        out += "{\"id\": " + json_string(id);
        out += ", \"chosen\": " + json_string(found.chosen);
        out += ", \"other\": " + json_string(found.other);
        out += ", \"outcome\": " + json_string(urd::test::outcome_names[static_cast<std::size_t>(found.result)]);
        out += ", \"request\": " + json_string(found.requests.front());
        out += ", \"requests\": " + std::to_string(found.requests.size());
        out += ", \"chosen_cells\": " + json_list(ids(found.chosen_cells));
        out += ", \"other_cells\": " + json_list(ids(found.other_cells)) + "}";
        for (const auto& each : found.chosen_cells)
            cells.insert(each.id);
        for (const auto& each : found.other_cells)
            cells.insert(each.id);
    }
    out += "\n], \"cells\": " + json_list(std::vector<std::string>(cells.begin(), cells.end())) + "}\n";
    return out;
}

}  // namespace

int main(int argc, char** argv) { return urd::test::write_or_check(argc, argv, "comparison_table", generate); }
