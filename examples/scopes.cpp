// Scopes, layers and memo tables: a symbol table whose inner bindings end with their scope, a map that reuses a
// parent's entries, and a memo kept for two generations.

#include "urd/use/scopes.hpp"

#include "show.hpp"
#include "urd/store.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/layers.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <utility>

using urd::key;
using urd::Store;
using urd::value;

namespace
{

using Symbols = Store<key<std::string>, value<int>, urd::scoped>;
using Translations = Store<key<std::string>, value<std::string>, urd::layered>;
using Memo = Store<key<int>, value<std::int64_t>>;

void run()
{
    std::cout << "Scopes and layers:\n";
    example::show<Symbols>("Store<key<string>, value<int>, scoped>      bindings that end with their scope");
    example::show<Translations>("Store<key<string>, value<string>, layered>  a layer over a parent's entries");

    // A parser's symbol table: x is bound outside, rebound inside an action, and back once the action ends.
    auto symbols = Symbols();
    symbols["x"] = 1;
    symbols.push_scope();
    symbols["x"] = 2;
    symbols["y"] = 3;
    std::cout << "\ninside: x = " << symbols.at("x") << ", y bound: " << std::boolalpha << symbols.contains("y");
    symbols.pop_scope();
    std::cout << "\noutside: x = " << symbols.at("x") << ", y bound: " << symbols.contains("y") << '\n';

    // A problem's translations over its domain's: found from the parent, added to its own.
    const auto domain = Translations { { "on", "on'" }, { "clear", "clear'" } };
    auto problem = Translations();
    problem.layer_over(&domain);
    problem.try_emplace("goal", "goal'");
    std::cout << "problem translates on to " << std::as_const(problem).at("on") << " and holds " << problem.size() << " of its own\n";

    // A memo kept for two generations: the current one, and the last, swapped and cleared per generation.
    auto current = Memo();
    auto last = Memo();
    for (int generation = 0; generation < 3; ++generation)
    {
        std::swap(current, last);
        current.clear();
        for (int input = generation; input < generation + 4; ++input)
            current.try_emplace(input, last.contains(input) ? last.at(input) : static_cast<std::int64_t>(input) * input);
    }
    std::cout << "memo of generation 2 holds " << current.size() << ", the last " << last.size() << "; inputs in both were computed once\n";
}

}  // namespace

int main()
{
    try
    {
        run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
