// Say what the data is and how it is used; urd picks the representation.

#include "urd/algorithms.hpp"
#include "urd/store.hpp"

#include <iostream>
#include <string_view>

namespace
{

// Downstream names its data in its own words; urd::Store states the structure.
using VisitedIds = urd::Store<urd::key<int>>;                                // a set, nothing more asked
using GoalIds = urd::Store<urd::key<int>, urd::sorted>;                      // iterated in order
using ConstantIds = urd::Store<urd::key<int>, urd::sorted, urd::frozen>;     // built once, then read
using EventLog = urd::Store<urd::value<int>, urd::monotone>;                 // a sequence that only grows

template<typename S>
void report(std::string_view name)
{
    std::cout << name << " -> " << S::representation_name << '\n';
}

}  // namespace

int main()
{
    report<VisitedIds>("VisitedIds   (key<int>)");
    report<GoalIds>("GoalIds      (key<int>, sorted)");
    report<ConstantIds>("ConstantIds  (key<int>, sorted, frozen)");
    report<EventLog>("EventLog     (value<int>, monotone)");

    auto goals = GoalIds::make({ 7, 3, 9 });
    goals.insert(5);
    const auto constants = ConstantIds::make({ 3, 5, 11 });

    // Both are sorted sets by request, whatever their representations, so they merge in linear time.
    std::cout << "goals that are constants:";
    for (const int id : urd::intersect(goals, constants))
        std::cout << ' ' << id;
    std::cout << '\n';

    auto log = EventLog();
    log.insert(1);
    log.insert(2);  // log.erase(1) would not compile: the log only grows
    std::cout << "log size: " << log.size() << '\n';
}
