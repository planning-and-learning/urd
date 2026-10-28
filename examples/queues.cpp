// Queues: elements reached by the order they leave in. The order is the property; push, pop and front or top
// follow the standard adaptors.

#include "show.hpp"
#include "urd/store.hpp"

#include <exception>
#include <iostream>
#include <utility>

using urd::Store;
using urd::value;

namespace
{

void run()
{
    std::cout << "Queues:\n";
    example::show<Store<value<int>, urd::fifo>>("Store<value<int>, fifo>          first in, first out (breadth-first search)");
    example::show<Store<value<int>, urd::lifo>>("Store<value<int>, lifo>          last in, first out (depth-first search)");
    example::show<Store<value<int>, urd::by_priority>>("Store<value<int>, by_priority>   least first (an open list)");

    // An open list of (cost, state), cheapest first, cleared per search without giving back its storage.
    auto open = Store<value<std::pair<double, int>>, urd::by_priority>();
    for (int search = 0; search < 2; ++search)
    {
        open.clear();
        open.push({ 4.0, 1 });
        open.push({ 2.5, 2 });
        open.emplace(3.0, 3);
        std::cout << "\nsearch " << search << " expands:";
        while (!open.empty())
        {
            std::cout << ' ' << open.top().second;
            open.pop();
        }
    }
    // for (const auto& entry : open)  // does not compile: a queue is not iterated
    // open.front();                    // does not compile: a priority queue has top()

    auto frontier = Store<value<int>, urd::fifo>();
    for (const int state : { 7, 8, 9 })
        frontier.push(state);
    std::cout << "\nbreadth-first, next: " << frontier.front() << '\n';
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
