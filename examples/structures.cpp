// A partition of indices, a family of Stores named by type, and a set that logs what changed,
// for semi-naive evaluation.

#include "urd/compose/family.hpp"
#include "urd/store.hpp"
#include "urd/use/all.hpp"

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
    auto terms = Store<key<int>, urd::partition>();
    terms.insert(4);  // terms 0..4, each in a class of its own
    terms.merge(0, 3);
    terms.merge(3, 4);
    std::cout << "classes: " << terms.classes() << ", 0 and 4 equal: " << std::boolalpha << terms.same(0, 4) << '\n';

    // One interner per kind of entity, reached by its type.
    auto repositories = urd::compose::Family<Interned, int, std::string>();
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
