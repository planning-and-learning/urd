#pragma once

// A file generated from the code and committed: `<tool> --write <path>` writes it, `<tool> --check <path>` (a ctest)
// exits 1 and names the first line where the committed file differs from what the code generates.

#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace urd::test
{

/// The main of a generator: generate() throws std::runtime_error for a defect of the code it reads.
template<typename Generate>
int write_or_check(int argc, char** argv, std::string_view tool, Generate generate)
{
    const auto arguments = std::span(argv, static_cast<std::size_t>(argc));
    const auto mode = arguments.size() == 3 ? std::string_view(arguments[1]) : std::string_view();
    if (mode != "--write" && mode != "--check")
    {
        std::cerr << "usage: " << tool << " --write <path> | --check <path>\n";
        return 2;
    }
    const auto path = std::string(arguments[2]);
    auto generated = std::string();
    try
    {
        generated = generate();
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << tool << ": " << error.what() << "\n";
        return 1;
    }
    if (mode == "--write")
    {
        auto file = std::ofstream(path);
        file << generated;
        return file ? 0 : 1;
    }
    auto file = std::ifstream(path);
    if (!file)
    {
        std::cerr << tool << ": cannot read " << path << "\n";
        return 1;
    }
    const auto committed = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (committed == generated)
        return 0;
    auto expected = std::istringstream(generated);
    auto actual = std::istringstream(committed);
    auto expected_line = std::string();
    auto actual_line = std::string();
    for (int line = 1;; ++line)
    {
        const bool more_expected = static_cast<bool>(std::getline(expected, expected_line));
        const bool more_actual = static_cast<bool>(std::getline(actual, actual_line));
        if (!more_expected && !more_actual)
            break;
        if (expected_line != actual_line || more_expected != more_actual)
        {
            std::cerr << tool << ": " << path << " differs from the code at line " << line << ":\n  committed: " << actual_line
                      << "\n  generated: " << expected_line << "\nregenerate with: " << tool << " --write " << path << "\n";
            return 1;
        }
    }
    return 1;
}

}  // namespace urd::test
