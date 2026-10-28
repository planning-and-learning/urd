// Rows: Stores of Stores, instead of std::vector<std::vector<…>>. The rows' properties choose the layout.

#include "show.hpp"
#include "urd/store.hpp"

#include <iostream>
#include <utility>
#include <vector>

using urd::key;
using urd::Store;
using urd::value;

using Row = Store<value<int>, urd::frozen>;                   // written once
using GrowingRow = Store<value<int>, urd::retains>;           // grows, keeps its storage when cleared
using Rows = Store<value<Row>>;                               // rows written once
using Scratch = Store<value<GrowingRow>, urd::retains>;       // a buffer of buffers, reset every round
using Blocks = Store<value<Store<value<Row>, urd::frozen>>>;  // three levels

int main()
{
    std::cout << "Rows:\n";
    example::show<Rows>("Store<value<Store<value<int>, frozen>>>               rows written once");
    example::show<Scratch>("Store<value<Store<value<int>, retains>>, retains>    rows that grow, reset every round");
    example::show<Store<value<Store<value<int>>>>>("Store<value<Store<value<int>>>>                       anything else");
    example::show<Blocks>("three levels, all frozen                              (one offsets array per level)");

    // Frozen rows lie in one array; a row is a read-only slice of it (a std::span).
    auto successors = Rows { { 1, 2 }, { 0 }, { 0, 1, 3 } };
    successors.push_back({ 2 });
    std::cout << "\nsuccessors of 2:";
    for (const int state : successors[2])
        std::cout << ' ' << state;
    // successors[2].push_back(4);  // does not compile: the row is frozen

    // Rows that grow, cleared every round: after the first round nothing is allocated.
    auto scratch = Scratch();
    for (int round = 0; round < 3; ++round)
    {
        scratch.clear();
        for (int row = 0; row < 4; ++row)
        {
            auto& buffer = scratch.emplace_back();  // a kept row, cleared
            for (int element = 0; element <= row; ++element)
                buffer.push_back(round * 10 + element);
        }
    }
    std::cout << "\nlast row of the last round:";
    for (const int element : scratch[3])
        std::cout << ' ' << element;

    // Three levels, flattened: a slice of slices.
    const auto blocks = Blocks { { { 1 }, { 2, 3 } }, { { 4, 5, 6 } } };
    std::cout << "\nblock 0, row 1, element 1: " << blocks[0][1][1] << '\n';

    // By key instead of position: a frozen multimap with dense keys is CSR too, built from pairs in any order.
    using Edges = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen>;
    example::show<Edges>("Store<key<int>, value<int>, multi, dense, frozen>     each key's values together");
    const auto pairs = std::vector<std::pair<int, int>> { { 2, 3 }, { 0, 1 }, { 2, 0 }, { 1, 2 } };
    const auto edges = Edges(pairs.begin(), pairs.end());
    std::cout << "values of 2:";
    for (const int target : edges.values(2))
        std::cout << ' ' << target;

    // A graph: with bidirectional, the keys of a value too, so a vertex's predecessors as well as its successors.
    const auto graph = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>(pairs.begin(), pairs.end());
    std::cout << "\npredecessors of 0:";
    for (const int source : graph.keys(0))
        std::cout << ' ' << source;
    std::cout << '\n';
}
