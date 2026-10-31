// Ten Stores of different kinds: what a file holding them costs the compiler must not grow with the number of
// containers registered (tests/scaling.py compiles it with the registry as it is and grown, URD_REGISTRY_COPIES).

#include "urd/repr/addressable_heap.hpp"
#include "urd/repr/bidirectional_csr.hpp"
#include "urd/repr/bucket_queue.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/csr_multimap.hpp"
#include "urd/repr/dense_multimap.hpp"
#include "urd/repr/dense_set.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/entry_ordered.hpp"
#include "urd/repr/flat_map.hpp"
#include "urd/repr/flat_set.hpp"
#include "urd/repr/index_map.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/jagged.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/queue.hpp"
#include "urd/repr/recycling.hpp"
#include "urd/repr/ring_fifo.hpp"
#include "urd/repr/row_pool.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/sparse_set.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/repr/unordered_flat_map.hpp"
#include "urd/repr/unordered_flat_set.hpp"
#include "urd/store.hpp"

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
