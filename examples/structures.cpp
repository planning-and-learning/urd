// Beyond one Store: a partition of indices, a matrix of bits, a family of Stores named by type, and a set that
// logs what changed, for semi-naive evaluation.

#include "urd/bit_matrix.hpp"
#include "urd/family.hpp"
#include "urd/partition.hpp"
#include "urd/store.hpp"

#include <exception>
#include <iostream>
#include <string>

using urd::key;
using urd::Store;

namespace
{

template<typename T>
using Interned = Store<key<T>, urd::positional, urd::monotone>;

void run()
{
    // Classes of equal terms: merge what is equal, ask which class each term is in.
    auto terms = urd::Partition<int>(5);
    terms.merge(0, 3);
    terms.merge(3, 4);
    std::cout << "classes: " << terms.classes() << ", 0 and 4 equal: " << std::boolalpha << terms.same(0, 4) << '\n';

    // Reachability by closing a bit matrix: unite each row with the rows it reaches until nothing changes.
    auto reach = urd::BitMatrix<int>(4, 4);
    reach.set(0, 1);
    reach.set(1, 2);
    reach.set(2, 3);
    for (bool changed = true; changed;)
    {
        changed = false;
        for (int from = 0; from < 4; ++from)
            for (int to = 0; to < 4; ++to)
                if (reach.test(from, to))
                    changed = reach.unite_rows(from, to) || changed;
    }
    std::cout << "0 reaches " << reach.count(0) << " vertices\n";

    // One interner per kind of entity, reached by its type.
    auto repositories = urd::Family<Interned, int, std::string>();
    repositories.get<std::string>().insert("on");
    repositories.get<int>().insert(42);
    std::cout << "strings interned: " << repositories.get<std::string>().size() << '\n';

    // Semi-naive evaluation: each round works on the facts new since the last.
    auto facts = Store<key<int>, urd::change_log>();
    facts.insert(1);
    facts.clear_changes();
    for (const int derived : { 1, 2, 3 })
        facts.insert(derived);
    std::cout << "new this round:";
    for (const int fact : facts.added())
        std::cout << ' ' << fact;
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
