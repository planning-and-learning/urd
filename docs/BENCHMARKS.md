# Benchmarks

What `benchmarks/containers.cpp` measures, and the preference orders in `include/urd/registry/declarations.hpp` that cite it.
Each number is nanoseconds per element (lower is better); the fastest in a row is bold. Inserting a random
order into a sorted vector is O(n²), so its insert and clear-and-refill rows stop at 10⁴.

- Machine: 12th Gen Intel Core i9-12900KF; GCC 13.3, CMake Release (`-O3 -DNDEBUG`); Google Benchmark 1.9.4,
  `--benchmark_min_time=0.1s`.
- Containers: `std::unordered_set`/`_map`, `boost::unordered_flat_set`/`_map` (with `std::hash`),
  `std::set`/`map`, `boost::container::flat_set`/`_map`.
- Workloads: **insert** n distinct keys in random order into an empty container; **lookup** n probes, half hits
  and half misses; **clear and refill** with n keys; **iterate** over n elements.
- Keys: `int`, and a 32-byte key (four 64-bit ids) that names its identity (`identifying_members()`).
- Hash, equality and order are urd's (`urd::Hash`, `urd::EqualTo`, `urd::Less`), as the registry instantiates the
  containers: `int` hashes to itself (the flat tables mix it), other keys by xxHash64 over their bytes.

Reproduce:

```sh
cmake -S . -B build/bench -DCMAKE_BUILD_TYPE=Release -DURD_BUILD_BENCHMARKS=ON -DURD_BUILD_TESTS=OFF -DURD_BUILD_EXAMPLES=OFF
cmake --build build/bench -j8 && build/bench/benchmark_containers --benchmark_min_time=0.1s
```



## set<int>

| Workload | n | std::unordered | boost::unordered_flat | std tree | boost::container::flat |
|---|---|---|---|---|---|
| insert | 100 | 15.0 | 8.7 | 17.4 | **7.5** |
| insert | 10,000 | 24.9 | **8.5** | 74.3 | 102 |
| insert | 100,000 | 45.3 | **7.9** | 133 | – |
| lookup | 100 | 2.8 | **1.0** | 4.1 | 5.6 |
| lookup | 10,000 | 3.1 | **1.0** | 56.7 | 46.1 |
| lookup | 100,000 | 4.4 | **1.4** | 120 | 62.4 |
| clear and refill | 100 | 11.7 | **3.8** | 18.3 | 6.4 |
| clear and refill | 10,000 | 10.8 | **2.1** | 73.9 | 100 |
| clear and refill | 100,000 | 16.4 | **2.4** | 134 | – |
| iterate | 100 | 1.1 | 0.6 | 2.0 | **0.2** |
| iterate | 10,000 | 1.6 | 0.7 | 5.8 | **0.1** |
| iterate | 100,000 | 3.6 | 0.6 | 16.9 | **0.1** |

## set<Key32>

| Workload | n | std::unordered | boost::unordered_flat | std tree | boost::container::flat |
|---|---|---|---|---|---|
| insert | 100 | 22.8 | 15.8 | 21.3 | **14.2** |
| insert | 10,000 | 48.3 | **20.8** | 102 | 899 |
| insert | 100,000 | 82.5 | **21.7** | 182 | – |
| lookup | 100 | 8.5 | **7.3** | 12.2 | 9.2 |
| lookup | 10,000 | 18.2 | **8.3** | 66.9 | 54.6 |
| lookup | 100,000 | 26.2 | **11.2** | 136 | 81.9 |
| clear and refill | 100 | 19.2 | **8.2** | 21.9 | 11.5 |
| clear and refill | 10,000 | 29.7 | **9.4** | 101 | 937 |
| clear and refill | 100,000 | 50.2 | **11.4** | 190 | – |
| iterate | 100 | 1.0 | 0.6 | 2.2 | **0.2** |
| iterate | 10,000 | 3.3 | 0.7 | 6.8 | **0.1** |
| iterate | 100,000 | 9.7 | 1.0 | 17.8 | **0.1** |

## map<int, int>

| Workload | n | std::unordered | boost::unordered_flat | std tree | boost::container::flat |
|---|---|---|---|---|---|
| insert | 100 | 14.7 | **9.0** | 19.6 | 9.2 |
| insert | 10,000 | 25.2 | **9.3** | 93.3 | 182 |
| insert | 100,000 | 44.8 | **8.7** | 167 | – |
| lookup | 100 | 2.7 | **1.1** | 5.4 | 4.8 |
| lookup | 10,000 | 3.1 | **1.1** | 53.2 | 44.8 |
| lookup | 100,000 | 4.7 | **1.4** | 117 | 63.6 |
| clear and refill | 100 | 11.1 | **4.0** | 19.7 | 7.1 |
| clear and refill | 10,000 | 11.3 | **2.2** | 92.3 | 176 |
| clear and refill | 100,000 | 19.2 | **2.7** | 175 | – |
| iterate | 100 | 1.0 | 0.5 | 2.0 | **0.2** |
| iterate | 10,000 | 1.6 | 0.8 | 5.7 | **0.1** |
| iterate | 100,000 | 3.7 | 0.6 | 16.2 | **0.1** |

## Dense keys (int indices 0..2n)

Only a request for dense keys reaches these; the flat hash table, the default otherwise, for comparison.

| set workload | n | FlatHashSet | DenseSet | SparseSet |
|---|---|---|---|---|
| insert | 100 | 8.4 | **1.2** | 2.4 |
| insert | 10,000 | 8.8 | **1.2** | 1.6 |
| insert | 100,000 | 8.3 | **1.3** | 6.8 |
| lookup | 100 | 1.1 | 0.6 | **0.6** |
| lookup | 10,000 | 1.1 | 0.6 | **0.5** |
| lookup | 100,000 | 1.3 | **0.6** | 1.0 |
| clear and refill | 100 | 4.2 | 1.1 | **0.9** |
| clear and refill | 10,000 | 2.6 | 1.2 | **0.9** |
| clear and refill | 100,000 | 3.1 | **1.2** | 5.2 |
| iterate | 100 | 0.5 | 3.0 | **0.3** |
| iterate | 10,000 | 0.7 | 3.1 | **0.3** |
| iterate | 100,000 | 0.7 | 3.1 | **0.3** |

| map workload | n | FlatHashMap | DenseMap | GenerationMap |
|---|---|---|---|---|
| insert | 100 | 9.1 | **1.8** | 2.1 |
| insert | 10,000 | 9.7 | **1.2** | 1.9 |
| insert | 100,000 | 9.0 | **5.3** | 8.2 |
| lookup | 100 | 1.1 | **0.4** | 0.6 |
| lookup | 10,000 | 1.1 | **0.3** | 0.6 |
| lookup | 100,000 | 1.4 | **0.4** | 1.0 |
| clear and refill | 100 | 4.3 | **0.9** | 1.0 |
| clear and refill | 10,000 | 2.7 | 0.9 | **0.7** |
| clear and refill | 100,000 | **3.2** | 3.7 | 8.1 |
| iterate | 100 | **0.5** | 0.8 | 1.1 |
| iterate | 10,000 | **0.7** | 0.8 | 1.1 |
| iterate | 100,000 | **0.7** | 0.8 | 1.1 |

Positions beat hashing at insert and lookup, 2 to 7 times. A bitset iterates more slowly, scanning bits where half the
range is empty. Clear-and-refill favors the dense containers up to 10⁴ elements; at 10⁵ the flat hash table refills
faster than the generation map and the sparse set. Their O(1) clear pays off where few of many keys are present, as in
per-state markings.

## Insertion order (IndexedSet: a vector plus a flat hash table of positions)

ns per element, against the flat hash table.

| workload | n | int: FlatHashSet | int: IndexedSet | Key32: FlatHashSet | Key32: IndexedSet |
|---|---|---|---|---|---|
| insert | 10⁴ | 8.8 | 9.8 | 21.5 | 23.2 |
| insert | 10⁵ | 8.1 | 9.3 | 22.1 | 43.7 |
| lookup | 10⁵ | 1.3 | 1.7 | 11.3 | 11.5 |
| clear and refill | 10⁵ | 3.2 | 3.4 | 11.5 | 11.9 |
| iterate | 10⁵ | 0.7 | 0.1 | 1.0 | 0.1 |

Lookups cost what the hash table costs; iteration is a vector's. Inserting a large key costs twice, since the
table keeps a copy of each element: a table of positions hashed through the vector would avoid that, if
interning large keys shows up in a profile.

## Postings (multimaps: n values over n/8 keys, inserted, then each key's values read)

ns per value.

| keys | n | PostingMap | std::unordered_multimap | std::multimap |
|---|---|---|---|---|
| int | 10³ | **12.8** | 24.1 | 21.9 |
| int | 10⁵ | **13.2** | 32.2 | 113.4 |
| Key32 | 10³ | **24.7** | 37.5 | 52.6 |
| Key32 | 10⁵ | **27.2** | 80.5 | 191.4 |

A flat hash table of value vectors probes once per key and reads its values from one array; the node-based
multimap allocates per value. `PostingMap` comes first for multimaps; `std::unordered_multimap` remains for
`stable_refs`.

## Pools of states (benchmarks/pools.cpp)

20,000 states, each a random earlier one with 3 positions (values) or 3 facts added and 3 removed; inserted, then
read back (values) or asked one fact (facts). Bytes and time per state.

| rows | flat interner (`pool<V>`) | tree pool (`pool<V>, structural_sharing`) |
|---|---|---|
| 100 values | 447 B, 0.33 µs | 166 B, 0.63 µs |
| 1,000 values | 4,046 B, 2.5 µs | 271 B, 2.1 µs |
| 1,000 facts, about 10% true | 865 B, 1.5 µs | 136 B, 0.57 µs |
| 10,000 facts | 4,409 B, 8.6 µs | 271 B, 0.91 µs |

A tree pool stores a successor as the paths to what changed, so its bytes per state hardly grow with the row; the
flat interner copies the row. For short value rows the trees cost twice the time for a third of the memory; from a
thousand positions on, and for fact sets at every size, they are smaller and faster.

## Queues (fifo: a deque against a ring over one vector)

A frontier of `live` queued elements, then per step two pushes and two pops (`frontier` in `benchmarks/containers.cpp`),
and a growth from empty to n followed by n pops (`grow`). Nanoseconds per step and per element.

| live or n | `std::deque` (`Fifo`) | `RingFifo` |
|---|---|---|
| frontier, 16 live | 3.8 | **3.6** |
| frontier, 1,000 live | 3.8 | 3.8 |
| frontier, 100,000 live | **3.8** | 5.0 |
| frontier, 1,000,000 live | **3.9** | 5.4 |
| grow to 1,000 | **1.4** | 3.2 |
| grow to 1,000,000 | **2.2** | 6.7 |

The deque stays first for `fifo`: level with the ring on a small frontier, faster on a large one and at growing,
and a push never moves an element (`stable_refs`), where the ring copies every queued element when it doubles. The
ring serves `fifo, retains`, which the deque cannot (it frees its blocks as pops leave them), and `small`, where the
registry moves containers with stable storage last and the two are level.

## What the orders take from it

- **Unsorted sets and maps: flat hash tables first.** They are fastest at insert, lookup and clear-and-refill at
  every size and for both key types, 2–6 times faster than the standard's node-based tables, and much faster
  than trees or sorted vectors at lookup. This holds at 100 elements too, so it applies to `small` and
  `frozen` requests as well. The node-based tables remain for `stable_refs`, which flat tables do not keep.
- **Sorted requests: a tree where elements are inserted over time, a sorted vector where they are few or never
  change.** The sorted vector iterates 5–50 times faster and looks up faster than the tree, but inserts in O(n).
- **Deterministic hashing costs a little on composite keys.** With urd::Hash (xxHash64 over the bytes) the
  32-byte key is about twice as slow to hash and look up as with the minimal hash combine measured before, and
  ordering it member by member doubles a tree's lookups at 100 elements. The orders stay: the flat hash table is
  still fastest except at inserting 100 such keys, where a sorted vector is level with it.
- **Iteration alone favors sorted vectors**: no request states an iteration-heavy workload yet; one would put
  them first.

<!-- generated by benchmarks/record.py doc: edit outside this block -->

## The registry's choices, measured

Generated from `benchmarks/results/dominik-MS-7D31.json` (12th Gen Intel(R) Core(TM) i9-12900KF, g++). Each container is measured on its own (`benchmarks/requests.json`): its lifecycle on each shape of element at three sizes, the median of three runs, in ns per operation. For each request, the chosen container against every other that serves it: 532 not compared, as the other gives a costly guarantee the request did not ask for; 114 settled by their costs; 135 measured. Alike costs are decided asymptotically: a measured choice holds where, at the largest size measured, the chosen container's lifecycle (the sum of the compared operations' times per element) exceeds the other's by at most 10% or 5 ns, whichever is larger, unless it takes fewer words per element. The costs and measured times of every container are in the complexity tables of each kind of Store.

### Choices that fail

- set > recycling_tree_set / ordered_lookup,iterate,lookup,insert,erase,clear / std::string;- / small: at 64, 246.3 against 204.8 ns per element (Store<set<std::string>, small, sorted, stable_refs>)
- set > recycling_tree_set / ordered_lookup,iterate,lookup,insert,erase,clear / Id;-: at 100000, 740.1 against 649.6 ns per element (Store<set<Id>, sorted>)
- unordered_flat_set > flat_set / iterate,lookup,insert,erase,clear / Id;- / small: at 64, 26.7 against 20.6 ns per element (Store<set<Id>, small>)
- layered_bit_row_interner > layered_row_interner / access,insert_row,find_row / -;Store<sequence<index<Id>>, stable_refs, frozen, positional>: at 4096, 83100.9 against 62594.3 ns per element (Store<pool<Store<sequence<index<Id>>, stable_refs, frozen, positional>>, monotone, bit, layered>)
- layered_thread_safe_bit_row_interner > layered_thread_safe_row_interner / access,insert_row,find_row / -;Store<sequence<index<Id>>, stable_refs, frozen, positional>: at 4096, 79604.1 against 48331.7 ns per element (Store<pool<Store<sequence<index<Id>>, stable_refs, frozen, positional>>, monotone, bit, layered, concurrent>)
- unordered_map > recycling_hash_map / iterate,lookup,insert,erase,clear / int;int: at 100000, 120.8 against 97.3 ns per element (Store<map<int, int>, stable_refs>)
- unordered_flat_map > flat_map / iterate,lookup,insert,erase,clear / Id;Id / small: at 64, 26.6 against 21.4 ns per element (Store<map<Id, Id>, small>)

### Slower at a smaller size only

- unordered_flat_set > flat_set / iterate,lookup,insert,erase,clear / int;- / small: at 8, 47.2 against 27.0 ns per element (Store<set<int>, small>)
- unordered_flat_set > flat_set / iterate,lookup,insert,erase,clear / std::string;- / small: at 8, 114.1 against 102.4 ns per element (Store<set<std::string>, small>)
- unordered_flat_set > flat_set / iterate,lookup,insert,erase,clear / Id;- / small: at 8, 51.7 against 25.5 ns per element (Store<set<Id>, small>)
- layered_bit_row_interner > layered_row_interner / access,insert_row,find_row / -;Store<sequence<index<Id>>, stable_refs, frozen, positional>: at 16, 285.0 against 233.3 ns per element (Store<pool<Store<sequence<index<Id>>, stable_refs, frozen, positional>>, monotone, bit, layered>)
- layered_bit_row_interner > layered_row_interner / access,insert_row,find_row / -;Store<sequence<index<Id>>, stable_refs, frozen, positional>: at 256, 4095.6 against 3562.4 ns per element (Store<pool<Store<sequence<index<Id>>, stable_refs, frozen, positional>>, monotone, bit, layered>)
- layered_thread_safe_bit_row_interner > layered_thread_safe_row_interner / access,insert_row,find_row / -;Store<sequence<index<Id>>, stable_refs, frozen, positional>: at 16, 312.5 against 201.6 ns per element (Store<pool<Store<sequence<index<Id>>, stable_refs, frozen, positional>>, monotone, bit, layered, concurrent>)
- layered_thread_safe_bit_row_interner > layered_thread_safe_row_interner / access,insert_row,find_row / -;Store<sequence<index<Id>>, stable_refs, frozen, positional>: at 256, 4216.9 against 2632.0 ns per element (Store<pool<Store<sequence<index<Id>>, stable_refs, frozen, positional>>, monotone, bit, layered, concurrent>)
- unordered_flat_map > flat_map / iterate,lookup,insert,erase,clear / int;int / small: at 8, 58.9 against 26.1 ns per element (Store<map<int, int>, small>)
- flat_map > map / ordered_lookup,iterate,lookup / int;int: at 1000, 56.8 against 46.2 ns per element (Store<map<int, int>, sorted, frozen>)
- flat_map > recycling_map / ordered_lookup,iterate,lookup / int;int: at 1000, 56.8 against 51.5 ns per element (Store<map<int, int>, sorted, frozen>)
- posting_map > chained_posting_map / iterate,lookup,insert,erase,clear / int;int: at 1000, 41.4 against 24.3 ns per element (Store<map<int, int>, multi>)
- posting_map > chained_posting_map / iterate,lookup,insert,erase,clear / int;int: at 10000, 61.1 against 34.1 ns per element (Store<map<int, int>, multi>)
- flat_map > map / ordered_lookup,iterate,lookup / std::string;double: at 10000, 328.9 against 255.9 ns per element (Store<map<std::string, double>, sorted, frozen>)
- flat_map > recycling_map / ordered_lookup,iterate,lookup / std::string;double: at 10000, 328.9 against 241.4 ns per element (Store<map<std::string, double>, sorted, frozen>)
- posting_map > chained_posting_map / iterate,lookup,insert,erase,clear / std::string;double: at 1000, 173.0 against 144.6 ns per element (Store<map<std::string, double>, multi>)
- posting_map > chained_posting_map / iterate,lookup,insert,erase,clear / std::string;double: at 10000, 308.8 against 187.8 ns per element (Store<map<std::string, double>, multi>)
- posting_map > chained_posting_map / iterate,lookup / std::string;double: at 10000, 68.8 against 48.2 ns per element (Store<map<std::string, double>, multi, frozen>)
- posting_map > unordered_multimap / iterate,lookup / std::string;double: at 10000, 68.8 against 60.2 ns per element (Store<map<std::string, double>, multi, frozen>)
- unordered_flat_map > flat_map / iterate,lookup,insert,erase,clear / Id;Id / small: at 8, 47.5 against 24.3 ns per element (Store<map<Id, Id>, small>)
- posting_map > chained_posting_map / iterate,lookup,insert,erase,clear / Id;Id: at 1000, 47.2 against 27.7 ns per element (Store<map<Id, Id>, multi>)
- posting_map > chained_posting_map / iterate,lookup,insert,erase,clear / Id;Id: at 10000, 69.0 against 40.9 ns per element (Store<map<Id, Id>, multi>)
- stable_value_interner > arena_interner / access,insert_row,find_row / -;Text: at 16, 90.0 against 53.5 ns per element (Store<pool<Text>, stable_refs>)

### Slower for less memory

- unordered_set > set / iterate,lookup,insert,erase,clear / int;- / small: at 64, 48.5 against 42.3 ns per element (Store<set<int>, small, stable_refs>)
- unordered_set > recycling_tree_set / iterate,lookup,insert,erase,clear / int;- / small: at 64, 48.5 against 38.7 ns per element (Store<set<int>, small, stable_refs>)
- flat_set > recycling_tree_set / ordered_lookup,iterate,lookup / std::string;-: at 100000, 575.7 against 502.0 ns per element (Store<set<std::string>, sorted, frozen>)
- unordered_flat_set > stable_indexed_set / iterate,lookup / std::string;-: at 100000, 92.1 against 82.8 ns per element (Store<set<std::string>, frozen>)
- posting_map > chained_posting_map / iterate,lookup,insert,erase,clear / int;int: at 100000, 90.9 against 51.4 ns per element (Store<map<int, int>, multi>)
- posting_map > chained_posting_map / iterate,lookup,insert,erase,clear / Id;Id: at 100000, 95.2 against 61.6 ns per element (Store<map<Id, Id>, multi>)

## Memory, measured

The heap each container's Store holds once its lifecycle filled it, counted apart from the timing (benchmark_memory) and as `urd::memory_usage` tells it. Words are per element, each the size of an element and at least a machine word, at most over the sizes measured (where the elements own nothing more).

| Container | Declared | Measured words | memory_usage off by |
|---|---|---|---|
| `addressable_heap` | O(n), 3 words | 4.92 | 0.0% |
| `arena_interner` | O(n), 2.5 words | 0.90 | 0.1% |
| `bidirectional_csr` | O(U), 2.4 words | 4.75 | 0.0% |
| `bucket_queue` | O(U), 2 words | 6.13 | 0.0% |
| `chained_posting_map` | O(capacity), 8.5 words | 8.45 | 0.0% |
| `concurrent_indexed_set` | O(capacity), 2.6 words | 4.30 | 0.0% |
| `concurrent_segmented_vector` | O(n), 1.6 words | 1.01 | 0.0% |
| `csr_multimap` | O(U), 1.2 words | 2.38 | 0.0% |
| `dense_map` | O(U), 2 words | 1.64 | 0.0% |
| `dense_multimap` | O(U), 4 words | 6.13 | 0.0% |
| `dense_ordered_map` | O(U), 3 words | 3.51 | 0.0% |
| `dense_set` | O(U), 0.03 words | 0.03 | 0.0% |
| `disjoint_sets` | O(n), 2 words | 3.75 | 0.0% |
| `fifo` | O(n), 1.2 words | 0.53 | 1.5% |
| `fixed_jagged` | O(n), 1.2 words | — | 0.0% |
| `flat_map` | O(n), 1.5 words | 1.56 | 21.0% |
| `flat_multimap` | O(n), 1.5 words | 1.56 | 0.0% |
| `flat_multiset` | O(n), 1.5 words | 0.78 | 0.0% |
| `flat_set` | O(n), 1.5 words | 0.78 | 0.0% |
| `generation_map` | O(U), 1.6 words | 3.28 | 0.0% |
| `heap` | O(n), 1.5 words | 0.82 | 0.0% |
| `indexed_set` | O(capacity), 2.5 words | 1.84 | 0.1% |
| `jagged` | O(n), 1.2 words | — | 0.0% |
| `layered_arena_interner` | O(n), 2.5 words | 0.90 | 0.1% |
| `layered_bit_row_interner` | O(n), 0.5 words | 2.05 | 0.0% |
| `layered_dense_map` | O(U), 2 words | 1.64 | 0.0% |
| `layered_generation_map` | O(U), 1.6 words | 3.28 | 0.0% |
| `layered_row_interner` | O(n), 4.5 words | 4.01 | 0.0% |
| `layered_stable_record_interner` | O(n), 2.6 words | 0.15 | 0.3% |
| `layered_thread_safe_arena_interner` | O(n), 2.7 words | 1.49 | 0.0% |
| `layered_thread_safe_bit_row_interner` | O(n), 0.5 words | 2.58 | 0.0% |
| `layered_thread_safe_generation_map` | O(U), 1.8 words | 4.03 | 0.0% |
| `layered_thread_safe_record_interner` | O(n), 2.7 words | 0.69 | 0.0% |
| `layered_thread_safe_row_interner` | O(n), 2.6 words | 4.05 | 0.0% |
| `lifo` | O(n), 1.5 words | 0.82 | 0.0% |
| `map` | O(n), 5 words | 5.00 | 15.2% |
| `multimap` | O(n), 5 words | 5.00 | 0.0% |
| `ordered_map` | O(capacity), 3.5 words | 2.66 | 19.5% |
| `ordered_sparse_set` | O(U), 2 words | 2.69 | 0.0% |
| `parallel_hash_map` | O(capacity), 1.3 words | 2.10 | 17.7% |
| `parallel_hash_set` | O(capacity), 1.3 words | 1.18 | 0.0% |
| `parallel_node_hash_map` | O(n), 3 words | 3.10 | 18.5% |
| `parallel_node_hash_set` | O(n), 3 words | 2.60 | 0.0% |
| `posting_map` | O(capacity), 4 words | 8.95 | 0.0% |
| `record_interner` | O(n), 2.5 words | 0.11 | 0.5% |
| `recycling_hash_map` | O(n), 4.5 words | 4.73 | 17.7% |
| `recycling_hash_set` | O(n), 4.5 words | 4.73 | 0.0% |
| `recycling_map` | O(n), 5.5 words | 5.00 | 15.2% |
| `recycling_tree_set` | O(n), 5.5 words | 5.00 | 0.0% |
| `ring_fifo` | O(n), 1.5 words | 1.64 | 0.0% |
| `row_interner` | O(n), 4.5 words | 4.01 | 0.0% |
| `row_pool` | O(n), 4 words | — | 0.0% |
| `scoped_dense_map` | O(U), 2.2 words | 1.64 | 0.0% |
| `scoped_map` | O(capacity), 2.3 words | 2.31 | 17.7% |
| `scoped_tree_map` | O(n), 6 words | 5.00 | 15.2% |
| `segmented_vector` | O(n), 1.5 words | 1.01 | 0.0% |
| `set` | O(n), 5 words | 5.00 | 0.0% |
| `slot_vector` | O(n), 1.5 words | 0.45 | 0.0% |
| `sparse_set` | O(U), 2 words | 2.69 | 0.0% |
| `stable_dense_map` | O(U), 1.3 words | 2.02 | 0.0% |
| `stable_generation_map` | O(U), 1.7 words | 4.03 | 0.0% |
| `stable_indexed_set` | O(capacity), 2.5 words | 2.36 | 0.1% |
| `stable_record_interner` | O(n), 2.6 words | 0.15 | 0.3% |
| `stable_value_interner` | O(n), 2.6 words | 0.80 | 0.3% |
| `thread_safe_arena_interner` | O(n), 2.7 words | 1.49 | 0.0% |
| `thread_safe_generation_map` | O(U), 1.8 words | 4.03 | 0.0% |
| `thread_safe_record_interner` | O(n), 2.7 words | 0.69 | 0.0% |
| `thread_safe_row_interner` | O(n), 2.6 words | 4.05 | 0.0% |
| `thread_safe_slot_vector` | O(n), 1.6 words | 0.47 | 26.6% |
| `thread_safe_tree_bitset_set` | O(n), 0.05 words | 0.71 | 0.0% |
| `thread_safe_tree_vector_set` | O(n), 1 words | 1.79 | 0.0% |
| `thread_safe_value_interner` | O(n), 2.7 words | 1.33 | 0.0% |
| `tracked_dense_set` | O(U), 0.03 words | 0.08 | 0.0% |
| `tracked_jagged` | O(n), 1.3 words | — | 0.0% |
| `tree_bitset_set` | O(n), 0.05 words | 0.17 | 0.3% |
| `tree_vector_set` | O(n), 1 words | 0.82 | 0.1% |
| `unordered_flat_map` | O(capacity), 1.3 words | 2.31 | 17.7% |
| `unordered_flat_set` | O(capacity), 1.3 words | 1.28 | 0.2% |
| `unordered_map` | O(n), 5 words | 4.73 | 17.7% |
| `unordered_multimap` | O(n), 4 words | 4.73 | 0.0% |
| `unordered_set` | O(n), 4 words | 4.73 | 0.0% |
| `value_interner` | O(n), 2.5 words | 0.56 | 0.5% |
| `vector` | O(n), 1.5 words | 0.82 | 0.0% |

### Memory that fails

- addressable_heap/-;Id/10000: 39.3 bytes per element, declared 3.0 words (24.0 bytes)
- addressable_heap/-;Id/100000: 31.5 bytes per element, declared 3.0 words (24.0 bytes)
- addressable_heap/-;int/10000: 39.3 bytes per element, declared 3.0 words (24.0 bytes)
- addressable_heap/-;int/100000: 31.5 bytes per element, declared 3.0 words (24.0 bytes)
- bidirectional_csr/Id;Id/1000: 24.1 bytes per element, declared 2.4 words (19.2 bytes)
- bidirectional_csr/Id;Id/10000: 38.0 bytes per element, declared 2.4 words (19.2 bytes)
- bidirectional_csr/Id;Id/100000: 29.6 bytes per element, declared 2.4 words (19.2 bytes)
- bidirectional_csr/int;int/1000: 24.1 bytes per element, declared 2.4 words (19.2 bytes)
- bidirectional_csr/int;int/10000: 38.0 bytes per element, declared 2.4 words (19.2 bytes)
- bidirectional_csr/int;int/100000: 29.6 bytes per element, declared 2.4 words (19.2 bytes)
- bucket_queue/-;int/1000: 28.1 bytes per element, declared 2.0 words (16.0 bytes)
- bucket_queue/-;int/10000: 49.0 bytes per element, declared 2.0 words (16.0 bytes)
- bucket_queue/-;int/100000: 36.4 bytes per element, declared 2.0 words (16.0 bytes)
- concurrent_indexed_set/Id;-/1000: 34.3 bytes per element, declared 2.6 words (20.8 bytes)
- concurrent_indexed_set/int;-/1000: 34.4 bytes per element, declared 2.6 words (20.8 bytes)
- csr_multimap/Id;double/10000: 23.0 bytes per element, declared 1.2 words (19.2 bytes)
- csr_multimap/int;int/1000: 12.1 bytes per element, declared 1.2 words (9.6 bytes)
- csr_multimap/int;int/10000: 19.0 bytes per element, declared 1.2 words (9.6 bytes)
- csr_multimap/int;int/100000: 14.8 bytes per element, declared 1.2 words (9.6 bytes)
- dense_multimap/int;int/10000: 49.0 bytes per element, declared 4.0 words (32.0 bytes)
- dense_multimap/int;int/100000: 36.4 bytes per element, declared 4.0 words (32.0 bytes)
- dense_multimap/int;int#2/10000: 49.0 bytes per element, declared 4.0 words (32.0 bytes)
- dense_multimap/int;int#2/100000: 36.4 bytes per element, declared 4.0 words (32.0 bytes)
- dense_ordered_map/index<Id>;int/10000: 28.1 bytes per element, declared 3.0 words (24.0 bytes)
- dense_ordered_map/int;int/10000: 28.1 bytes per element, declared 3.0 words (24.0 bytes)
- disjoint_sets/Id;-/10000: 30.0 bytes per element, declared 2.0 words (16.0 bytes)
- disjoint_sets/Id;-/100000: 21.6 bytes per element, declared 2.0 words (16.0 bytes)
- disjoint_sets/int;-/10000: 30.0 bytes per element, declared 2.0 words (16.0 bytes)
- disjoint_sets/int;-/100000: 21.6 bytes per element, declared 2.0 words (16.0 bytes)
- flat_map/Id;Store<set<Id>, unique>/1000: memory_usage tells 86232 bytes, 103232 counted
- flat_map/Id;Store<set<Id>, unique>/10000: memory_usage tells 896544 bytes, 1066544 counted
- flat_map/Id;Store<set<Id>, unique>/100000: memory_usage tells 6392304 bytes, 8092304 counted
- flat_map/int;Store<set<int>, unique>/1000: memory_usage tells 86232 bytes, 103232 counted
- flat_map/int;Store<set<int>, unique>/10000: memory_usage tells 896544 bytes, 1066544 counted
- flat_map/int;Store<set<int>, unique>/100000: memory_usage tells 6392304 bytes, 8092304 counted
- flat_map/std::string;Store<set<double>, unique>/1000: memory_usage tells 164848 bytes, 185848 counted
- flat_map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1699816 bytes, 1909816 counted
- flat_map/std::string;Store<set<double>, unique>/100000: memory_usage tells 13228456 bytes, 15328456 counted
- generation_map/index<Id>;int/1000: 16.4 bytes per element, declared 1.6 words (12.8 bytes)
- generation_map/index<Id>;int/10000: 26.2 bytes per element, declared 1.6 words (12.8 bytes)
- generation_map/index<Id>;int/100000: 21.0 bytes per element, declared 1.6 words (12.8 bytes)
- generation_map/int;int/1000: 16.4 bytes per element, declared 1.6 words (12.8 bytes)
- generation_map/int;int/10000: 26.2 bytes per element, declared 1.6 words (12.8 bytes)
- generation_map/int;int/100000: 21.0 bytes per element, declared 1.6 words (12.8 bytes)
- layered_bit_row_interner/-;Store<sequence<index<Id>>, stable_refs, frozen, positional>/16: 16.4 bytes per element, declared 0.5 words (4.0 bytes)
- layered_bit_row_interner/-;Store<sequence<index<Id>>, stable_refs, frozen, positional>/256: 15.6 bytes per element, declared 0.5 words (4.0 bytes)
- layered_bit_row_interner/-;Store<sequence<index<Id>>, stable_refs, frozen, positional>/4096: 15.5 bytes per element, declared 0.5 words (4.0 bytes)
- layered_generation_map/index<Id>;int/1000: 16.4 bytes per element, declared 1.6 words (12.8 bytes)
- layered_generation_map/index<Id>;int/10000: 26.2 bytes per element, declared 1.6 words (12.8 bytes)
- layered_generation_map/index<Id>;int/100000: 21.0 bytes per element, declared 1.6 words (12.8 bytes)
- layered_generation_map/int;int/1000: 16.4 bytes per element, declared 1.6 words (12.8 bytes)
- layered_generation_map/int;int/10000: 26.2 bytes per element, declared 1.6 words (12.8 bytes)
- layered_generation_map/int;int/100000: 21.0 bytes per element, declared 1.6 words (12.8 bytes)
- layered_thread_safe_bit_row_interner/-;Store<sequence<index<Id>>, stable_refs, frozen, positional>/16: 20.7 bytes per element, declared 0.5 words (4.0 bytes)
- layered_thread_safe_bit_row_interner/-;Store<sequence<index<Id>>, stable_refs, frozen, positional>/256: 15.8 bytes per element, declared 0.5 words (4.0 bytes)
- layered_thread_safe_bit_row_interner/-;Store<sequence<index<Id>>, stable_refs, frozen, positional>/4096: 15.5 bytes per element, declared 0.5 words (4.0 bytes)
- layered_thread_safe_generation_map/Id;double/1000: 48.4 bytes per element, declared 1.8 words (28.8 bytes)
- layered_thread_safe_generation_map/Id;double/10000: 39.2 bytes per element, declared 1.8 words (28.8 bytes)
- layered_thread_safe_generation_map/index<Id>;int/1000: 32.3 bytes per element, declared 1.8 words (14.4 bytes)
- layered_thread_safe_generation_map/index<Id>;int/10000: 26.2 bytes per element, declared 1.8 words (14.4 bytes)
- layered_thread_safe_generation_map/index<Id>;int/100000: 21.0 bytes per element, declared 1.8 words (14.4 bytes)
- layered_thread_safe_generation_map/int;int/1000: 32.3 bytes per element, declared 1.8 words (14.4 bytes)
- layered_thread_safe_generation_map/int;int/10000: 26.2 bytes per element, declared 1.8 words (14.4 bytes)
- layered_thread_safe_generation_map/int;int/100000: 21.0 bytes per element, declared 1.8 words (14.4 bytes)
- layered_thread_safe_row_interner/-;Store<set<int>, unique, dense, stable_refs, frozen>/16: 24.1 bytes per element, declared 2.6 words (20.8 bytes)
- layered_thread_safe_row_interner/-;Store<set<int>, unique, dense, stable_refs, frozen>/256: 32.4 bytes per element, declared 2.6 words (20.8 bytes)
- layered_thread_safe_row_interner/-;Store<set<int>, unique, dense, stable_refs, frozen>/4096: 31.9 bytes per element, declared 2.6 words (20.8 bytes)
- map/Id;Store<set<Id>, unique>/1000: memory_usage tells 95000 bytes, 112000 counted
- map/Id;Store<set<Id>, unique>/10000: memory_usage tells 950000 bytes, 1120000 counted
- map/Id;Store<set<Id>, unique>/100000: memory_usage tells 9500000 bytes, 11200000 counted
- map/int;Store<set<int>, unique>/1000: memory_usage tells 95000 bytes, 112000 counted
- map/int;Store<set<int>, unique>/10000: memory_usage tells 950000 bytes, 1120000 counted
- map/int;Store<set<int>, unique>/100000: memory_usage tells 9500000 bytes, 11200000 counted
- map/std::string;Store<set<double>, unique>/1000: memory_usage tells 162000 bytes, 183000 counted
- map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1620000 bytes, 1830000 counted
- map/std::string;Store<set<double>, unique>/100000: memory_usage tells 16290000 bytes, 18390000 counted
- ordered_map/Id;Store<set<Id>, unique>/1000: memory_usage tells 70371 bytes, 87388 counted
- ordered_map/Id;Store<set<Id>, unique>/10000: memory_usage tells 952891 bytes, 1122908 counted
- ordered_map/Id;Store<set<Id>, unique>/100000: memory_usage tells 7922603 bytes, 9622620 counted
- ordered_map/int;Store<set<int>, unique>/1000: memory_usage tells 70371 bytes, 87388 counted
- ordered_map/int;Store<set<int>, unique>/10000: memory_usage tells 952891 bytes, 1122908 counted
- ordered_map/int;Store<set<int>, unique>/100000: memory_usage tells 7922603 bytes, 9622620 counted
- ordered_map/std::string;Store<set<double>, unique>/1000: memory_usage tells 140933 bytes, 161950 counted
- ordered_map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1840533 bytes, 2050550 counted
- ordered_map/std::string;Store<set<double>, unique>/100000: memory_usage tells 15981509 bytes, 18081526 counted
- ordered_sparse_set/Id;-/10000: 21.6 bytes per element, declared 2.0 words (16.0 bytes)
- ordered_sparse_set/index<Id>;-/10000: 21.6 bytes per element, declared 2.0 words (16.0 bytes)
- ordered_sparse_set/int;-/10000: 21.6 bytes per element, declared 2.0 words (16.0 bytes)
- parallel_hash_map/Id;Id/1000: 16.8 bytes per element, declared 1.3 words (10.4 bytes)
- parallel_hash_map/Id;Id/10000: 14.8 bytes per element, declared 1.3 words (10.4 bytes)
- parallel_hash_map/Id;Id/100000: 11.8 bytes per element, declared 1.3 words (10.4 bytes)
- parallel_hash_map/Id;Store<set<Id>, unique>/1000: memory_usage tells 105432 bytes, 122432 counted
- parallel_hash_map/Id;Store<set<Id>, unique>/10000: memory_usage tells 952304 bytes, 1122304 counted
- parallel_hash_map/Id;Store<set<Id>, unique>/100000: memory_usage tells 7922016 bytes, 9622016 counted
- parallel_hash_map/int;Store<set<int>, unique>/1000: memory_usage tells 96024 bytes, 113024 counted
- parallel_hash_map/int;Store<set<int>, unique>/10000: memory_usage tells 952304 bytes, 1122304 counted
- parallel_hash_map/int;Store<set<int>, unique>/100000: memory_usage tells 7922016 bytes, 9622016 counted
- parallel_hash_map/int;int/1000: 15.1 bytes per element, declared 1.3 words (10.4 bytes)
- parallel_hash_map/int;int/10000: 14.8 bytes per element, declared 1.3 words (10.4 bytes)
- parallel_hash_map/int;int/100000: 11.8 bytes per element, declared 1.3 words (10.4 bytes)
- parallel_hash_map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1775136 bytes, 1985136 counted
- parallel_hash_map/std::string;Store<set<double>, unique>/100000: memory_usage tells 15457360 bytes, 17557360 counted
- parallel_node_hash_map/Id;Store<set<Id>, unique>/1000: memory_usage tells 79832 bytes, 96832 counted
- parallel_node_hash_map/Id;Store<set<Id>, unique>/10000: memory_usage tells 777584 bytes, 947584 counted
- parallel_node_hash_map/Id;Store<set<Id>, unique>/100000: memory_usage tells 7479776 bytes, 9179776 counted
- parallel_node_hash_map/int;Store<set<int>, unique>/1000: memory_usage tells 78104 bytes, 95104 counted
- parallel_node_hash_map/int;Store<set<int>, unique>/10000: memory_usage tells 777584 bytes, 947584 counted
- parallel_node_hash_map/int;Store<set<int>, unique>/100000: memory_usage tells 7479776 bytes, 9179776 counted
- parallel_node_hash_map/std::string;Store<set<double>, unique>/1000: memory_usage tells 147408 bytes, 168408 counted
- parallel_node_hash_map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1447584 bytes, 1657584 counted
- parallel_node_hash_map/std::string;Store<set<double>, unique>/100000: memory_usage tells 14269776 bytes, 16369776 counted
- posting_map/Id;Id/1000: 71.6 bytes per element, declared 4.0 words (32.0 bytes)
- posting_map/Id;Id/10000: 58.1 bytes per element, declared 4.0 words (32.0 bytes)
- posting_map/Id;Id/100000: 47.3 bytes per element, declared 4.0 words (32.0 bytes)
- posting_map/int;int/1000: 71.6 bytes per element, declared 4.0 words (32.0 bytes)
- posting_map/int;int/10000: 58.1 bytes per element, declared 4.0 words (32.0 bytes)
- posting_map/int;int/100000: 47.3 bytes per element, declared 4.0 words (32.0 bytes)
- recycling_hash_map/Id;Store<set<Id>, unique>/1000: memory_usage tells 87872 bytes, 104872 counted
- recycling_hash_map/Id;Store<set<Id>, unique>/10000: memory_usage tells 872184 bytes, 1042184 counted
- recycling_hash_map/Id;Store<set<Id>, unique>/100000: memory_usage tells 9283464 bytes, 10983464 counted
- recycling_hash_map/int;Store<set<int>, unique>/1000: memory_usage tells 79872 bytes, 96872 counted
- recycling_hash_map/int;Store<set<int>, unique>/10000: memory_usage tells 792184 bytes, 962184 counted
- recycling_hash_map/int;Store<set<int>, unique>/100000: memory_usage tells 8483464 bytes, 10183464 counted
- recycling_hash_map/std::string;Store<set<double>, unique>/1000: memory_usage tells 145762 bytes, 166762 counted
- recycling_hash_map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1461074 bytes, 1671074 counted
- recycling_hash_map/std::string;Store<set<double>, unique>/100000: memory_usage tells 15272354 bytes, 17372354 counted
- recycling_map/Id;Store<set<Id>, unique>/1000: memory_usage tells 95000 bytes, 112000 counted
- recycling_map/Id;Store<set<Id>, unique>/10000: memory_usage tells 950000 bytes, 1120000 counted
- recycling_map/Id;Store<set<Id>, unique>/100000: memory_usage tells 9500000 bytes, 11200000 counted
- recycling_map/int;Store<set<int>, unique>/1000: memory_usage tells 95000 bytes, 112000 counted
- recycling_map/int;Store<set<int>, unique>/10000: memory_usage tells 950000 bytes, 1120000 counted
- recycling_map/int;Store<set<int>, unique>/100000: memory_usage tells 9500000 bytes, 11200000 counted
- recycling_map/std::string;Store<set<double>, unique>/1000: memory_usage tells 160890 bytes, 181890 counted
- recycling_map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1618890 bytes, 1828890 counted
- recycling_map/std::string;Store<set<double>, unique>/100000: memory_usage tells 16288890 bytes, 18388890 counted
- scoped_map/Id;Store<set<Id>, unique>/1000: memory_usage tells 115303 bytes, 132320 counted
- scoped_map/Id;Store<set<Id>, unique>/10000: memory_usage tells 952767 bytes, 1122784 counted
- scoped_map/Id;Store<set<Id>, unique>/100000: memory_usage tells 7922479 bytes, 9622496 counted
- scoped_map/int;Store<set<int>, unique>/1000: memory_usage tells 115303 bytes, 132320 counted
- scoped_map/int;Store<set<int>, unique>/10000: memory_usage tells 952767 bytes, 1122784 counted
- scoped_map/int;Store<set<int>, unique>/100000: memory_usage tells 7922479 bytes, 9622496 counted
- scoped_map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1774849 bytes, 1984866 counted
- scoped_map/std::string;Store<set<double>, unique>/100000: memory_usage tells 15457073 bytes, 17557090 counted
- scoped_tree_map/Id;Store<set<Id>, unique>/1000: memory_usage tells 95000 bytes, 112000 counted
- scoped_tree_map/Id;Store<set<Id>, unique>/10000: memory_usage tells 950000 bytes, 1120000 counted
- scoped_tree_map/Id;Store<set<Id>, unique>/100000: memory_usage tells 9500000 bytes, 11200000 counted
- scoped_tree_map/int;Store<set<int>, unique>/1000: memory_usage tells 95000 bytes, 112000 counted
- scoped_tree_map/int;Store<set<int>, unique>/10000: memory_usage tells 950000 bytes, 1120000 counted
- scoped_tree_map/int;Store<set<int>, unique>/100000: memory_usage tells 9500000 bytes, 11200000 counted
- scoped_tree_map/std::string;Store<set<double>, unique>/1000: memory_usage tells 160890 bytes, 181890 counted
- scoped_tree_map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1618890 bytes, 1828890 counted
- scoped_tree_map/std::string;Store<set<double>, unique>/100000: memory_usage tells 16288890 bytes, 18388890 counted
- sparse_set/Id;-/10000: 21.6 bytes per element, declared 2.0 words (16.0 bytes)
- sparse_set/index<Id>;-/10000: 21.6 bytes per element, declared 2.0 words (16.0 bytes)
- sparse_set/int;-/10000: 21.6 bytes per element, declared 2.0 words (16.0 bytes)
- stable_dense_map/Id;double/1000: 32.3 bytes per element, declared 1.3 words (20.8 bytes)
- stable_dense_map/Id;double/10000: 26.2 bytes per element, declared 1.3 words (20.8 bytes)
- stable_dense_map/index<Id>;int/1000: 16.1 bytes per element, declared 1.3 words (10.4 bytes)
- stable_dense_map/index<Id>;int/10000: 13.1 bytes per element, declared 1.3 words (10.4 bytes)
- stable_dense_map/int;int/1000: 16.1 bytes per element, declared 1.3 words (10.4 bytes)
- stable_dense_map/int;int/10000: 13.1 bytes per element, declared 1.3 words (10.4 bytes)
- stable_generation_map/Id;double/1000: 32.3 bytes per element, declared 1.7 words (27.2 bytes)
- stable_generation_map/index<Id>;int/1000: 32.3 bytes per element, declared 1.7 words (13.6 bytes)
- stable_generation_map/index<Id>;int/10000: 26.2 bytes per element, declared 1.7 words (13.6 bytes)
- stable_generation_map/index<Id>;int/100000: 21.0 bytes per element, declared 1.7 words (13.6 bytes)
- stable_generation_map/int;int/1000: 32.3 bytes per element, declared 1.7 words (13.6 bytes)
- stable_generation_map/int;int/10000: 26.2 bytes per element, declared 1.7 words (13.6 bytes)
- stable_generation_map/int;int/100000: 21.0 bytes per element, declared 1.7 words (13.6 bytes)
- thread_safe_generation_map/Id;double/1000: 48.4 bytes per element, declared 1.8 words (28.8 bytes)
- thread_safe_generation_map/Id;double/10000: 39.2 bytes per element, declared 1.8 words (28.8 bytes)
- thread_safe_generation_map/index<Id>;int/1000: 32.3 bytes per element, declared 1.8 words (14.4 bytes)
- thread_safe_generation_map/index<Id>;int/10000: 26.2 bytes per element, declared 1.8 words (14.4 bytes)
- thread_safe_generation_map/index<Id>;int/100000: 21.0 bytes per element, declared 1.8 words (14.4 bytes)
- thread_safe_generation_map/int;int/1000: 32.3 bytes per element, declared 1.8 words (14.4 bytes)
- thread_safe_generation_map/int;int/10000: 26.2 bytes per element, declared 1.8 words (14.4 bytes)
- thread_safe_generation_map/int;int/100000: 21.0 bytes per element, declared 1.8 words (14.4 bytes)
- thread_safe_row_interner/-;Store<set<int>, unique, dense, stable_refs, frozen>/16: 24.1 bytes per element, declared 2.6 words (20.8 bytes)
- thread_safe_row_interner/-;Store<set<int>, unique, dense, stable_refs, frozen>/256: 32.4 bytes per element, declared 2.6 words (20.8 bytes)
- thread_safe_row_interner/-;Store<set<int>, unique, dense, stable_refs, frozen>/4096: 31.9 bytes per element, declared 2.6 words (20.8 bytes)
- thread_safe_slot_vector/-;Id/16: memory_usage tells 1920 bytes, 2616 counted
- thread_safe_slot_vector/-;Id/256: memory_usage tells 1920 bytes, 2616 counted
- thread_safe_slot_vector/-;Id/4096: memory_usage tells 1920 bytes, 2616 counted
- thread_safe_slot_vector/-;int/16: memory_usage tells 1920 bytes, 2616 counted
- thread_safe_slot_vector/-;int/256: memory_usage tells 1920 bytes, 2616 counted
- thread_safe_slot_vector/-;int/4096: memory_usage tells 1920 bytes, 2616 counted
- thread_safe_tree_bitset_set/-;Store<set<int>, unique, dense, stable_refs, frozen>/16: 5.7 bytes per element, declared 0.05 words (0.4 bytes)
- thread_safe_tree_vector_set/-;Store<sequence<int>, stable_refs, frozen, positional>/16: 14.3 bytes per element, declared 1.0 words (8.0 bytes)
- thread_safe_tree_vector_set/-;Store<sequence<int>, stable_refs, frozen, positional>/256: 10.6 bytes per element, declared 1.0 words (8.0 bytes)
- thread_safe_tree_vector_set/-;Store<sequence<int>, stable_refs, frozen, positional>/4096: 10.5 bytes per element, declared 1.0 words (8.0 bytes)
- tracked_dense_set/Id;-/1000: 0.4 bytes per element, declared 0.03 words (0.2 bytes)
- tracked_dense_set/Id;-/10000: 0.7 bytes per element, declared 0.03 words (0.2 bytes)
- tracked_dense_set/Id;-/100000: 0.5 bytes per element, declared 0.03 words (0.2 bytes)
- tracked_dense_set/index<Id>;-/1000: 0.4 bytes per element, declared 0.03 words (0.2 bytes)
- tracked_dense_set/index<Id>;-/10000: 0.7 bytes per element, declared 0.03 words (0.2 bytes)
- tracked_dense_set/index<Id>;-/100000: 0.5 bytes per element, declared 0.03 words (0.2 bytes)
- tracked_dense_set/int;-/1000: 0.4 bytes per element, declared 0.03 words (0.2 bytes)
- tracked_dense_set/int;-/10000: 0.7 bytes per element, declared 0.03 words (0.2 bytes)
- tracked_dense_set/int;-/100000: 0.5 bytes per element, declared 0.03 words (0.2 bytes)
- tree_bitset_set/-;Store<set<int>, unique, dense, stable_refs, frozen>/16: 1.3 bytes per element, declared 0.05 words (0.4 bytes)
- unordered_flat_map/Id;Id/1000: 18.4 bytes per element, declared 1.3 words (10.4 bytes)
- unordered_flat_map/Id;Id/10000: 14.7 bytes per element, declared 1.3 words (10.4 bytes)
- unordered_flat_map/Id;Id/100000: 11.8 bytes per element, declared 1.3 words (10.4 bytes)
- unordered_flat_map/Id;Store<set<Id>, unique>/1000: memory_usage tells 115303 bytes, 132320 counted
- unordered_flat_map/Id;Store<set<Id>, unique>/10000: memory_usage tells 952767 bytes, 1122784 counted
- unordered_flat_map/Id;Store<set<Id>, unique>/100000: memory_usage tells 7922479 bytes, 9622496 counted
- unordered_flat_map/int;Store<set<int>, unique>/1000: memory_usage tells 115303 bytes, 132320 counted
- unordered_flat_map/int;Store<set<int>, unique>/10000: memory_usage tells 952767 bytes, 1122784 counted
- unordered_flat_map/int;Store<set<int>, unique>/100000: memory_usage tells 7922479 bytes, 9622496 counted
- unordered_flat_map/int;int/1000: 18.4 bytes per element, declared 1.3 words (10.4 bytes)
- unordered_flat_map/int;int/10000: 14.7 bytes per element, declared 1.3 words (10.4 bytes)
- unordered_flat_map/int;int/100000: 11.8 bytes per element, declared 1.3 words (10.4 bytes)
- unordered_flat_map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1775959 bytes, 1985976 counted
- unordered_flat_map/std::string;Store<set<double>, unique>/100000: memory_usage tells 15458183 bytes, 17558200 counted
- unordered_map/Id;Store<set<Id>, unique>/1000: memory_usage tells 87872 bytes, 104872 counted
- unordered_map/Id;Store<set<Id>, unique>/10000: memory_usage tells 872184 bytes, 1042184 counted
- unordered_map/Id;Store<set<Id>, unique>/100000: memory_usage tells 9283464 bytes, 10983464 counted
- unordered_map/int;Store<set<int>, unique>/1000: memory_usage tells 79872 bytes, 96872 counted
- unordered_map/int;Store<set<int>, unique>/10000: memory_usage tells 792184 bytes, 962184 counted
- unordered_map/int;Store<set<int>, unique>/100000: memory_usage tells 8483464 bytes, 10183464 counted
- unordered_map/std::string;Store<set<double>, unique>/1000: memory_usage tells 146872 bytes, 167872 counted
- unordered_map/std::string;Store<set<double>, unique>/10000: memory_usage tells 1462184 bytes, 1672184 counted
- unordered_map/std::string;Store<set<double>, unique>/100000: memory_usage tells 15273464 bytes, 17373464 counted
- unordered_multimap/Id;Id/100000: 37.8 bytes per element, declared 4.0 words (32.0 bytes)
- unordered_set/Id;-/100000: 37.8 bytes per element, declared 4.0 words (32.0 bytes)
- unordered_set/index<Id>;-/100000: 37.8 bytes per element, declared 4.0 words (32.0 bytes)

<!-- end of the generated block -->
