// Sets: say what the data is and how it is used; urd picks the container.

#include "show.hpp"
#include "urd/store.hpp"

#include <iostream>

using urd::key;
using urd::Store;

int main()
{
    std::cout << "Sets of ints:\n";
    example::show<Store<key<int>>>("Store<key<int>>                          a set, nothing more asked");
    example::show<Store<key<int>, urd::sorted>>("Store<key<int>, sorted>                  iterated in order");
    example::show<Store<key<int>, urd::sorted, urd::frozen>>("Store<key<int>, sorted, frozen>          built once, then only read");
    example::show<Store<key<int>, urd::small>>("Store<key<int>, small>                   a few elements");
    example::show<Store<key<int>, urd::stable_refs>>("Store<key<int>, stable_refs>             references to elements stay valid");
    example::show<Store<key<int>, urd::retains>>("Store<key<int>, retains>                 cleared and refilled, no allocation");

    // The same interface as the standard containers, limited to what was asked for.
    auto goals = Store<key<int>, urd::sorted> { 7, 3, 9 };
    goals.insert(5);
    std::cout << "\ngoals in order:";
    for (const int goal : goals)
        std::cout << ' ' << goal;
    std::cout << "\nfirst goal >= 6: " << *goals.lower_bound(6) << '\n';  // offered because sorted was asked

    const auto constants = Store<key<int>, urd::frozen> { 3, 5, 11 };
    std::cout << "5 is a constant: " << std::boolalpha << constants.contains(5) << '\n';
    // constants.insert(4);  // does not compile: frozen
}
