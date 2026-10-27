// Maps: values by key. The same properties as for sets choose the container; values change only by key.

#include "show.hpp"
#include "urd/store.hpp"

#include <iostream>
#include <string>

using urd::key;
using urd::Store;
using urd::value;

int main()
{
    std::cout << "Maps from strings to ints:\n";
    example::show<Store<key<std::string>, value<int>>>("Store<key<string>, value<int>>                  a map, nothing more asked");
    example::show<Store<key<std::string>, value<int>, urd::sorted>>("Store<key<string>, value<int>, sorted>          iterated in key order");
    example::show<Store<key<std::string>, value<int>, urd::frozen>>("Store<key<string>, value<int>, frozen>          built once, then only read");
    example::show<Store<key<std::string>, value<int>, urd::multi>>("Store<key<string>, value<int>, multi>           several values per key");

    // A symbol table: the standard's map operations.
    auto symbols = Store<key<std::string>, value<int>>();
    symbols["x"] = 0;
    symbols.try_emplace("y", 1);         // inserts: no value for "y" yet
    symbols.try_emplace("x", 7);         // keeps 0: "x" has a value
    symbols.insert_or_assign("y", 2);    // overwrites
    std::cout << "\nx = " << symbols.at("x") << ", y = " << symbols.at("y") << '\n';

    // Sorted by key, with the bounds to search it.
    const auto costs = Store<key<int>, value<std::string>, urd::sorted> { { 30, "far" }, { 10, "near" }, { 20, "middle" } };
    std::cout << "first cost >= 15: " << costs.lower_bound(15)->second << '\n';
    // costs.begin()->first = 5;  // does not compile: a key is never changed in place

    const auto constants = Store<key<std::string>, value<double>, urd::frozen> { { "pi", 3.14159 }, { "e", 2.71828 } };
    std::cout << "e = " << constants.at("e") << '\n';
    // constants["tau"] = 6.28;  // does not compile: frozen

    auto achievers = Store<key<std::string>, value<std::string>, urd::multi> { { "at-goal", "move-a" }, { "at-goal", "move-b" } };
    achievers.insert({ "holding", "pick-up" });
    std::cout << "achievers of at-goal: " << achievers.count("at-goal") << '\n';
}
