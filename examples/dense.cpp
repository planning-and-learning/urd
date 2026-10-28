// Dense keys: indices 0..n-1, stored by position instead of hashed. Facts, markings cleared every round, and costs
// per state.

#include "show.hpp"
#include "urd/store.hpp"

#include <cstdint>
#include <exception>
#include <iostream>

using urd::key;
using urd::Store;
using urd::value;

namespace
{

/// An index of a fact; it names its identity, so urd can use it as an index.
struct Fact
{
    std::uint32_t value = 0;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Fact::value> {}; }
};

void run()
{
    std::cout << "Dense keys:\n";
    example::show<Store<key<Fact>, urd::dense>>("Store<key<Fact>, dense>                          a set of indices");
    example::show<Store<key<Fact>, urd::dense, urd::constant_clear>>("Store<key<Fact>, dense, constant_clear>          cleared every round, in O(1)");
    example::show<Store<key<Fact>, value<double>, urd::dense>>("Store<key<Fact>, value<double>, dense>           a value per index");
    example::show<Store<key<Fact>, value<double>, urd::dense, urd::constant_clear>>(
        "Store<key<Fact>, value<double>, dense, constant_clear>  reset every round, in O(1)");

    // The facts of a state, and of a goal: set algebra, word by word on a bitset.
    auto state = Store<key<Fact>, urd::dense> { Fact { 1 }, Fact { 4 }, Fact { 9 } };
    const auto goal = Store<key<Fact>, urd::dense> { Fact { 4 }, Fact { 9 } };
    std::cout << "\nstate reaches the goal: " << std::boolalpha << state.includes(goal) << '\n';
    state |= Store<key<Fact>, urd::dense> { Fact { 12 } };
    std::cout << "facts after an action:";
    for (const auto fact : state)
        std::cout << ' ' << fact.value;

    // A marking per round: clearing does not depend on how many facts were ever marked.
    auto marked = Store<key<Fact>, urd::dense, urd::constant_clear>();
    for (std::uint32_t round = 0; round < 3; ++round)
    {
        marked.clear();
        for (std::uint32_t fact = round; fact < 1000; fact += 7)
            marked.insert(Fact { fact });
    }
    std::cout << "\nmarked in the last round: " << marked.size();

    // Costs per fact, reset every round by a new generation.
    auto costs = Store<key<Fact>, value<double>, urd::dense, urd::constant_clear>();
    costs[Fact { 3 }] = 2.5;
    costs.clear();
    costs[Fact { 5 }] = 1.0;
    std::cout << "\ncost of fact 3 after the reset: " << (costs.contains(Fact { 3 }) ? "kept" : "gone") << ", of fact 5: " << costs.at(Fact { 5 }) << '\n';
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
