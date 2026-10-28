# Benchmarks

What `benchmarks/containers.cpp` measures, and the preference orders in `include/urd/registry.hpp` that cite it.
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

