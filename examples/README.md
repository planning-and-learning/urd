# urd examples

urd lets you say what your data is and how it is used, and picks a container for it:

```cpp
using Visited = urd::Store<urd::key<int>>;                       // a set           -> hash table
using Goals   = urd::Store<urd::key<int>, urd::sorted>;          // in order        -> balanced tree
using Log     = urd::Store<urd::value<Event>, urd::monotone>;    // only grows      -> vector
using Rows    = urd::Store<urd::value<urd::Store<urd::value<int>, urd::frozen>>>;  // rows -> CSR
```

The Store offers the standard library's interface (`insert`, `push_back`, `lower_bound`, …), limited to what
the properties allow: a `frozen` Store has no `insert`, a `monotone` one no `erase`, and a sorted sequence
cannot be assigned to in place. Every container urd may choose is checked by tests to keep the properties it
claims.

| Example | Shows |
|---|---|
| `sets.cpp` | sets: plain, sorted, frozen, small, stable references, retained memory |
| `sequences.cpp` | sequences: plain, sorted, append-only, stable references, concurrent appends |
| `maps.cpp` | maps: plain, sorted, frozen, multi; `operator[]`, `try_emplace`, `insert_or_assign`, `at` |
| `dense.cpp` | dense keys (indices): bitset and sparse sets, maps by position, O(1) clears, set algebra |
| `interning.cpp` | insertion order and positions: interners that give each element an index, `push_unique` lists |
| `queues.cpp` | queues: first in first out, last in first out, least first (an open list), reused without allocating |
| `scopes.cpp` | a scoped symbol table, a layer over a parent's translations, a memo reset by generations |
| `structures.cpp` | a partition (union-find), a family of Stores by type, a change log |
| `states.cpp` | pools: search states as interned rows shared between states, successors by delta, scratch slots |
| `rows.cpp` | Stores of Stores: flattened rows (CSR), a row pool that never frees, three levels; CSR by key and a graph |

Each prints the requests and the container chosen for each, then uses a few of them. Lines commented with
"does not compile" show what the properties rule out.

Build and run from the repository root:

```sh
cmake -S . -B build/examples -DCMAKE_BUILD_TYPE=Release
cmake --build build/examples -j8 --target example_sets example_sequences example_maps example_rows example_dense example_interning example_queues example_scopes example_structures example_states
./build/examples/example_sets
./build/examples/example_sequences
./build/examples/example_maps
./build/examples/example_rows
./build/examples/example_dense
./build/examples/example_interning
./build/examples/example_queues
./build/examples/example_scopes
./build/examples/example_structures
./build/examples/example_states
```
