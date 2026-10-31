// Ten Stores of different kinds: what a file holding them costs the compiler must not grow with the number of
// containers registered (tests/scaling.py compiles it with the registry as it is and grown, URD_REGISTRY_COPIES).

#include "urd/store.hpp"
#include "urd/use/contiguous.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/nodes.hpp"
#include "urd/use/queues.hpp"
#include "urd/use/sequences.hpp"

#include <string>

int main()
{
    auto sets = urd::Store<urd::key<int>>();
    auto sorted = urd::Store<urd::key<int>, urd::sorted>();
    auto frozen = urd::Store<urd::key<int>, urd::sorted, urd::frozen>();
    auto stable = urd::Store<urd::key<std::string>, urd::stable_refs>();
    auto maps = urd::Store<urd::key<int>, urd::value<int>>();
    auto multimaps = urd::Store<urd::key<int>, urd::value<int>, urd::multi>();
    auto dense = urd::Store<urd::key<int>, urd::dense>();
    auto sequence = urd::Store<urd::value<int>>();
    auto queue = urd::Store<urd::value<int>, urd::fifo>();
    auto heap = urd::Store<urd::value<int>, urd::by_priority>();
    sets.insert(1);
    sorted.insert(1);
    stable.insert("a");
    maps.emplace(1, 2);
    multimaps.emplace(1, 2);
    dense.insert(1);
    sequence.push_back(1);
    queue.push(1);
    heap.push(1);
    return static_cast<int>(frozen.size());
}
