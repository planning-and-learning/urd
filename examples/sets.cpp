// Sets: say what the data is and how it is used; urd picks the container.

#include "show.hpp"
#include "urd/repr/maps/flat_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/values/flat_set.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/std_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/repr/wrappers/change_logged.hpp"
#include "urd/repr/wrappers/recycling.hpp"
#include "urd/store.hpp"

#include <iostream>

using urd::set;
using urd::Store;

int main()
{
    std::cout << "Sets of ints:\n";
    example::show<Store<set<int>>>("Store<set<int>>                          a set, nothing more asked");
    example::show<Store<set<int>, urd::sorted>>("Store<set<int>, sorted>                  iterated in order");
    example::show<Store<set<int>, urd::sorted, urd::frozen>>("Store<set<int>, sorted, frozen>          built once, then only read");
    example::show<Store<set<int>, urd::small>>("Store<set<int>, small>                   a few elements");
    example::show<Store<set<int>, urd::stable_refs>>("Store<set<int>, stable_refs>             references to elements stay valid");
    example::show<Store<set<int>, urd::retains>>("Store<set<int>, retains>                 cleared and refilled, no allocation");

    // The same interface as the standard containers, limited to what was asked for.
    auto goals = Store<set<int>, urd::sorted> { 7, 3, 9 };
    goals.insert(5);
    std::cout << "\ngoals in order:";
    for (const int goal : goals)
        std::cout << ' ' << goal;
    std::cout << "\nfirst goal >= 6: " << *goals.lower_bound(6) << '\n';  // offered because sorted was asked

    const auto constants = Store<set<int>, urd::frozen> { 3, 5, 11 };
    std::cout << "5 is a constant: " << std::boolalpha << constants.contains(5) << '\n';
    // constants.insert(4);  // does not compile: frozen
}
