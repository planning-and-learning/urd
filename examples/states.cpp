// States of a search, as pools of rows: each state's facts and values stored once, a successor as its parent with a
// few changes, and a handle standing for a state in comparisons and as a key. With structural_sharing, states share
// everything they have in common.

#include "show.hpp"
#include "urd/store.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/pools.hpp"
#include "urd/use/sequences.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <vector>

using urd::key;
using urd::Store;
using urd::value;

namespace
{

using Facts = Store<key<std::uint32_t>, urd::dense, urd::frozen>;
using Values = Store<value<double>, urd::frozen>;
using FactStates = Store<value<Facts>, urd::handles, urd::unique, urd::structural_sharing>;
using ValueStates = Store<value<Values>, urd::handles, urd::unique, urd::structural_sharing>;
using Scratch = Store<value<std::vector<std::uint32_t>>, urd::handles>;

void run()
{
    std::cout << "States:\n";
    example::show<FactStates>("Store<value<Facts>, handles, unique, structural_sharing>    fact sets, shared");
    example::show<ValueStates>("Store<value<Values>, handles, unique, structural_sharing>   numeric values, shared");
    example::show<Store<value<Facts>, urd::handles, urd::unique>>("Store<value<Facts>, handles, unique>                        each stored in full");
    example::show<Scratch>("Store<value<vector<…>>, handles>                            slots, reused when freed");

    // A state and its successors: the parent with facts added and removed.
    auto states = FactStates();
    const auto initial = states.insert({ 1, 4, 9, 400 }).first;
    const auto added = std::vector<std::uint32_t> { 5 };
    const auto removed = std::vector<std::uint32_t> { 4 };
    const auto next = states.insert(initial, std::span<const std::uint32_t>(added), std::span<const std::uint32_t>(removed)).first;
    const auto [again, stored] = states.insert({ 1, 5, 9, 400 });
    std::cout << "\nthe successor and the same facts listed afresh are one state: " << std::boolalpha << (next == again) << " (stored again: " << stored
              << ")\n";
    std::cout << "the successor holds fact 5: " << states.contains(next, 5) << ", fact 4: " << states.contains(next, 4) << '\n';

    // Numeric values, a few written per successor.
    auto values = ValueStates();
    const auto start = values.insert(std::vector<double>(1000, 0.0)).first;
    const auto writes = std::vector<urd::Write<double>> { { 17, 2.5 } };
    const auto moved = values.insert(start, std::span<const urd::Write<double>>(writes)).first;
    std::cout << "two states of 1000 values take " << values.memory_usage() << " bytes; value 17 of the second: " << values.get(moved)[17] << '\n';

    // Scratch buffers handed out per successor and returned: acquire gives a freed one back with its storage.
    auto scratch = Scratch();
    const auto buffer = scratch.acquire();
    scratch[buffer].assign(64, 0);
    scratch.erase(buffer);
    std::cout << "a buffer acquired again keeps its storage: " << (scratch[scratch.acquire()].capacity() >= 64) << '\n';
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
