// States of a search, as pools of rows: each state's facts and values stored once, a successor as its parent with a
// few changes, and a handle standing for a state in comparisons and as a key. With structural_sharing, states share
// everything they have in common.

#include "show.hpp"
#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/pools/pools.hpp"
#include "urd/repr/pools/tree_bitset.hpp"
#include "urd/repr/pools/tree_vector.hpp"
#include "urd/repr/sequences/jagged.hpp"
#include "urd/repr/sequences/row_pool.hpp"
#include "urd/repr/sequences/segmented.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/store.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <vector>

using urd::pool;
using urd::sequence;
using urd::set;
using urd::slots;
using urd::Store;

namespace
{

using Facts = Store<set<std::uint32_t>, urd::dense, urd::frozen>;
using Values = Store<sequence<double>, urd::frozen>;
using FactStates = Store<pool<Facts>, urd::structural_sharing>;
using ValueStates = Store<pool<Values>, urd::structural_sharing>;
using Scratch = Store<slots<std::vector<std::uint32_t>>>;

void run()
{
    std::cout << "States:\n";
    example::show<FactStates>("Store<pool<Facts>, structural_sharing>    fact sets, shared");
    example::show<ValueStates>("Store<pool<Values>, structural_sharing>   numeric values, shared");
    example::show<Store<pool<Facts>>>("Store<pool<Facts>>                        each stored in full");
    example::show<Scratch>("Store<slots<vector<…>>>                            slots, reused when freed");

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
    std::cout << "two states of 1000 values take " << urd::memory_usage(values) << " bytes; value 17 of the second: " << values.get(moved)[17] << '\n';

    // Scratch buffers handed out per successor and returned: a freed one comes back with its storage.
    auto scratch = Scratch();
    scratch.make_unique()->assign(64, 0);  // used, and returned when its owner goes at the end of the statement
    std::cout << "a buffer handed out again keeps its storage: " << (scratch.make_unique()->capacity() >= 64) << '\n';
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
