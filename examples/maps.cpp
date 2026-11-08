// Maps: values by key. The same properties as for sets choose the container; values change by key or through the
// iterators, keys never.

#include "show.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/std_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/repr/wrappers/recycling.hpp"
#include "urd/store.hpp"

#include <exception>
#include <iostream>
#include <string>

using urd::map;
using urd::Store;

namespace
{

void run()
{
    std::cout << "Maps from strings to ints:\n";
    example::show<Store<map<std::string, int>>>("Store<map<string, int>>                  a map, nothing more asked");
    example::show<Store<map<std::string, int>, urd::sorted>>("Store<map<string, int>, sorted>          iterated in key order");
    example::show<Store<map<std::string, int>, urd::frozen>>("Store<map<string, int>, frozen>          built once, then only read");
    example::show<Store<map<std::string, int>, urd::multi>>("Store<map<string, int>, multi>           several values per key");

    // A symbol table: the standard's map operations.
    auto symbols = Store<map<std::string, int>>();
    symbols["x"] = 0;
    symbols.try_emplace("y", 1);       // inserts: no value for "y" yet
    symbols.try_emplace("x", 7);       // keeps 0: "x" has a value
    symbols.insert_or_assign("y", 2);  // overwrites
    std::cout << "\nx = " << symbols.at("x") << ", y = " << symbols.at("y") << '\n';

    // Sorted by key, with the bounds to search it.
    const auto costs = Store<map<int, std::string>, urd::sorted> { { 30, "far" }, { 10, "near" }, { 20, "middle" } };
    std::cout << "first cost >= 15: " << costs.lower_bound(15)->second << '\n';
    // costs.begin()->first = 5;  // does not compile: a key is never changed in place

    const auto constants = Store<map<std::string, double>, urd::frozen> { { "pi", 3.14159 }, { "e", 2.71828 } };
    std::cout << "e = " << constants.at("e") << '\n';
    // constants["tau"] = 6.28;  // does not compile: frozen

    auto achievers = Store<map<std::string, std::string>, urd::multi> { { "at-goal", "move-a" }, { "at-goal", "move-b" } };
    achievers.insert({ "holding", "pick-up" });
    std::cout << "achievers of at-goal: " << achievers.count("at-goal") << '\n';
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
