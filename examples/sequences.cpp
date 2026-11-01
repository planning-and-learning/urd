// Sequences: values identified by position. The properties choose the container, and decide what may change.

#include "show.hpp"
#include "urd/repr/maps/entry_ordered.hpp"
#include "urd/repr/maps/flat_map.hpp"
#include "urd/repr/sequences/jagged.hpp"
#include "urd/repr/sequences/row_pool.hpp"
#include "urd/repr/sequences/segmented.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/values/flat_set.hpp"
#include "urd/store.hpp"

#include <exception>
#include <iostream>
#include <thread>
#include <vector>

using urd::multiset;
using urd::sequence;
using urd::Store;

namespace
{

void run()
{
    std::cout << "Sequences of ints:\n";
    example::show<Store<sequence<int>>>("Store<sequence<int>>                        a plain sequence");
    example::show<Store<multiset<int>, urd::sorted>>("Store<multiset<int>, sorted>                kept in order, duplicates allowed");
    example::show<Store<sequence<int>, urd::monotone>>("Store<sequence<int>, monotone>              only grows");
    example::show<Store<sequence<int>, urd::stable_refs, urd::monotone>>("Store<sequence<int>, stable_refs, monotone> a log whose entries never move");
    example::show<Store<sequence<int>, urd::concurrent, urd::monotone>>("Store<sequence<int>, concurrent, monotone>  appended from several threads");

    // A plain sequence reads and changes like std::vector.
    auto numbers = Store<sequence<int>> { 4, 1, 3 };
    numbers[0] = 2;
    numbers.push_back(5);
    std::cout << "\nnumbers:";
    for (const int number : numbers)
        std::cout << ' ' << number;

    // A sorted sequence keeps its order itself: you insert, it places.
    auto sorted = Store<multiset<int>, urd::sorted> { 4, 1, 3 };
    sorted.insert(2);
    std::cout << "\nsorted:";
    for (const int number : sorted)
        std::cout << ' ' << number;
    // sorted[0] = 9;  // does not compile: it would break the order

    // A log that only grows; a reference to an entry stays valid however much is appended.
    auto log = Store<sequence<int>, urd::stable_refs, urd::monotone>();
    const int& first = log.emplace_back(42);
    for (int entry = 0; entry < 100000; ++entry)
        log.push_back(entry);
    std::cout << "\nfirst log entry after 100000 more: " << first;
    // log.pop_back();  // does not compile: monotone

    // Several threads append at once.
    auto shared = Store<sequence<int>, urd::concurrent, urd::monotone>();
    auto threads = std::vector<std::thread>();
    for (int thread = 0; thread < 4; ++thread)
        threads.emplace_back(
            [&shared]
            {
                for (int entry = 0; entry < 1000; ++entry)
                    shared.push_back(entry);
            });
    for (auto& thread : threads)
        thread.join();
    std::cout << "\nentries appended by 4 threads: " << shared.size() << '\n';
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
