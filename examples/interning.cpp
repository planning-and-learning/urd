// Insertion order and positions: interners, which give each distinct element the next index, and lists without
// repeats, in the order first seen.

#include "show.hpp"
#include "urd/store.hpp"

#include <exception>
#include <iostream>
#include <string>

using urd::key;
using urd::Store;

namespace
{

void run()
{
    std::cout << "Insertion order and positions:\n";
    example::show<Store<key<std::string>, urd::insertion_ordered>>("Store<key<string>, insertion_ordered>         each once, in the order first seen");
    example::show<Store<key<std::string>, urd::positional>>("Store<key<string>, positional>                each once, by index: an interner");
    example::show<Store<key<std::string>, urd::positional, urd::sorted>>("Store<key<string>, positional, sorted>        each once, by rank");

    // An interner: a symbol's index is its position, given on first insert and kept while nothing is erased.
    auto symbols = Store<key<std::string>, urd::positional, urd::monotone>();
    std::cout << '\n';
    for (const auto* symbol : { "on", "clear", "on", "holding", "clear" })
    {
        const auto [it, inserted] = symbols.insert(symbol);
        std::cout << symbol << " -> " << symbols.position(it) << (inserted ? " (new)" : "") << '\n';
    }
    std::cout << "symbol 2: " << symbols[2] << ", index of clear: " << symbols.position(symbols.find("clear")) << '\n';
    // symbols[2] = "empty";  // does not compile: an element is its index's, and changing it would break the lookup
    // symbols.erase("on");   // does not compile: monotone, so indices never shift

    // A list without repeats, in the order first seen, as push_unique keeps one.
    auto goals = Store<key<int>, urd::insertion_ordered>();
    for (const int goal : { 7, 3, 7, 9, 3 })
        goals.insert(goal);
    std::cout << "goals in the order first seen:";
    for (const int goal : goals)
        std::cout << ' ' << goal;
    std::cout << '\n';
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
