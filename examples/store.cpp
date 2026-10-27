// Say what the data is and how it is used; urd picks the representation, and the Store offers what the
// properties allow.

#include "urd/store.hpp"

#include <iostream>

namespace
{

// Downstream names its data in its own words; urd::Store states the structure.
using VisitedIds = urd::Store<urd::key<int>>;                                // a set, nothing more asked
using GoalIds = urd::Store<urd::key<int>, urd::sorted>;                      // iterated in order
using ConstantIds = urd::Store<urd::key<int>, urd::sorted, urd::frozen>;     // built once, then read
using EventLog = urd::Store<urd::value<int>, urd::monotone>;                 // a sequence that only grows

}  // namespace

int main()
{
    auto goals = GoalIds { 7, 3, 9 };
    goals.insert(5);
    const auto constants = ConstantIds { 3, 5, 11 };

    // Sorted by request, whatever the representation.
    std::cout << "goals that are constants:";
    for (const int id : goals)
        if (constants.contains(id))
            std::cout << ' ' << id;
    std::cout << '\n';

    auto visited = VisitedIds();
    for (const int id : goals)
        visited.insert(id);
    std::cout << "visited " << visited.size() << " goals\n";

    auto log = EventLog();
    log.push_back(1);
    log.push_back(2);  // log.pop_back() would not compile: the log only grows
    std::cout << "log size: " << log.size() << '\n';
}
