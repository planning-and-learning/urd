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
- Keys: `int`, and a 32-byte key (four 64-bit ids) with a combined `std::hash`.

Reproduce:

```sh
cmake -S . -B build/bench -DCMAKE_BUILD_TYPE=Release -DURD_BUILD_BENCHMARKS=ON -DURD_BUILD_TESTS=OFF -DURD_BUILD_EXAMPLES=OFF
cmake --build build/bench -j8 && build/bench/benchmark_containers --benchmark_min_time=0.1s
```


## set<int>

| Workload | n | std::unordered | boost::unordered_flat | std tree | boost::container::flat |
|---|---|---|---|---|---|
| insert | 100 | 16.8 | 8.4 | 19.5 | **8.2** |
| insert | 10,000 | 28.2 | **8.4** | 75.3 | 101 |
| insert | 100,000 | 45.9 | **7.7** | 137 | – |
| lookup | 100 | 2.8 | **1.0** | 5.8 | 5.3 |
| lookup | 10,000 | 3.2 | **1.1** | 51.9 | 46.8 |
| lookup | 100,000 | 4.3 | **1.4** | 114 | 62.6 |
| clear and refill | 100 | 15.1 | **3.9** | 19.8 | 6.4 |
| clear and refill | 10,000 | 14.4 | **1.9** | 76.2 | 100 |
| clear and refill | 100,000 | 18.1 | **2.4** | 134 | – |
| iterate | 100 | 1.0 | 0.6 | 2.1 | **0.2** |
| iterate | 10,000 | 1.6 | 0.7 | 5.6 | **0.1** |
| iterate | 100,000 | 3.9 | 0.7 | 15.9 | **0.1** |

## set<Key32>

| Workload | n | std::unordered | boost::unordered_flat | std tree | boost::container::flat |
|---|---|---|---|---|---|
| insert | 100 | 20.9 | **9.7** | 21.7 | 11.9 |
| insert | 10,000 | 46.8 | **11.9** | 106 | 904 |
| insert | 100,000 | 77.7 | **13.4** | 191 | – |
| lookup | 100 | 4.8 | **2.7** | 6.0 | 4.9 |
| lookup | 10,000 | 9.8 | **3.1** | 62.4 | 50.0 |
| lookup | 100,000 | 14.9 | **5.1** | 140 | 73.1 |
| clear and refill | 100 | 17.7 | **5.1** | 22.1 | 8.2 |
| clear and refill | 10,000 | 26.4 | **4.0** | 105 | 901 |
| clear and refill | 100,000 | 43.9 | **5.1** | 187 | – |
| iterate | 100 | 1.1 | 0.6 | 2.2 | **0.3** |
| iterate | 10,000 | 2.9 | 0.7 | 6.8 | **0.2** |
| iterate | 100,000 | 8.7 | 1.1 | 17.5 | **0.4** |

## map<int, int>

| Workload | n | std::unordered | boost::unordered_flat | std tree | boost::container::flat |
|---|---|---|---|---|---|
| insert | 100 | 18.2 | **8.8** | 21.5 | 9.4 |
| insert | 10,000 | 29.8 | **9.2** | 96.5 | 183 |
| insert | 100,000 | 48.2 | **8.6** | 172 | – |
| lookup | 100 | 2.8 | **1.0** | 4.0 | 5.2 |
| lookup | 10,000 | 3.2 | **1.1** | 54.0 | 45.9 |
| lookup | 100,000 | 4.6 | **1.5** | 117 | 64.2 |
| clear and refill | 100 | 15.0 | **3.8** | 22.5 | 7.8 |
| clear and refill | 10,000 | 15.1 | **2.1** | 98.2 | 176 |
| clear and refill | 100,000 | 20.4 | **2.8** | 176 | – |
| iterate | 100 | 1.1 | 0.6 | 2.1 | **0.2** |
| iterate | 10,000 | 1.6 | 0.7 | 5.6 | **0.1** |
| iterate | 100,000 | 3.7 | 0.6 | 16.1 | **0.1** |

## What the orders take from it

- **Unsorted sets and maps: flat hash tables first.** They are fastest at insert, lookup and clear-and-refill at
  every size and for both key types, 2–6 times faster than the standard's node-based tables, and much faster
  than trees or sorted vectors at lookup. This holds at 100 elements too, so it applies to `small` and
  `frozen` requests as well. The node-based tables remain for `stable_refs`, which flat tables do not keep.
- **Sorted requests: a tree where elements are inserted over time, a sorted vector where they are few or never
  change.** The sorted vector iterates 5–50 times faster and looks up faster than the tree, but inserts in O(n).
- **Iteration alone favors sorted vectors**: no request states an iteration-heavy workload yet; one would put
  them first.

