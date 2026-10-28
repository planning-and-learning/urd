# urd

**Say what your data is. urd picks the container and checks that it keeps its promises.**

```cpp
#include <urd/store.hpp>
using namespace urd;

using Facts  = Store<key<Fact>, dense>;                                    // a bitset
using Open   = Store<value<std::pair<double, Node>>, by_priority>;         // a binary heap, cheapest first
using Costs  = Store<key<Node>, value<double>, dense, constant_clear>;     // a slot per node, cleared in O(1)
using States = Store<value<Store<key<Fact>, dense, frozen>>,
                     handles, unique, structural_sharing>;                 // every state once, sharing subtrees
using Names  = Store<key<std::string>, positional, monotone>;              // an interner: each name gets an index

auto facts = Facts { on_a_b, clear_c };
facts.insert(holding_c);                            // the interface its properties allow, in the standard's names
auto next = states.insert(parent, added, removed);  // a successor costs only the paths to what changed
// costs.lower_bound(n);                            // does not compile: nothing asked for sorted
```

`Fact` and `Node` are indices, as `dense` asks: integers, enums, or types identified by one integral member
(`urd::index_of`). A fact numbered 17 is bit 17 of the bitset.

You write *what* the data is (keys or values, ordered or not, dense indices, frozen, cleared every round, shared
between threads) instead of *which* container holds it. urd resolves the request at compile time to a container
from the standard library, Boost or its own `repr`, and gives the Store exactly the interface those properties
allow: a `frozen` Store has no `insert`, an unordered one no `lower_bound`, a queue no iteration.

Every container urd can choose is checked to keep the properties it declares: typed tests compare each one with a
reference model through thousands of random operations, and deliberately broken versions of the containers are
caught by them. The vocabulary is small enough for people and code-writing agents alike to state instead of
hand-rolling a structure, and errors say what is wrong:

```
error: static assertion failed: urd::Store: two properties answer the same question (dense and sparse, ...)
error: no matching function for call to 'Store<std::set<int, ...>, spec<int, void, urd::tag::sorted,
       urd::tag::unique, urd::tag::shrinkable>>::push_back(int)'
```

Header-only C++20, GPL-3.0. Built for the planning and learning libraries yggdrasil, loki, tyr and runir, and
general enough for others.

## What a request resolves to

A selection, as the current registry resolves them (`include/urd/registry.hpp`; the order of candidates there is
backed by [docs/BENCHMARKS.md](docs/BENCHMARKS.md)):

| Request | Container |
|---|---|
| `Store<key<int>>` | `boost::unordered_flat_set` (open addressing) |
| `Store<key<int>, sorted>` | `std::set` |
| `Store<key<int>, sorted, frozen>` | `boost::container::flat_set` (sorted vector) |
| `Store<key<int>, stable_refs>` | `std::unordered_set` |
| `Store<key<int>, dense>` | `DenseSet` (bitset) |
| `Store<key<int>, dense, constant_clear>` | `SparseSet` (Briggs–Torczon) |
| `Store<key<int>, insertion_ordered>` | `IndexedSet` (vector plus a hash table of positions) |
| `Store<key<std::string>, positional, monotone>` | `IndexedSet`: an interner |
| `Store<key<std::string>, positional, monotone, concurrent>` | `ConcurrentIndexedSet` |
| `Store<value<int>>` | `std::vector` |
| `Store<value<int>, sorted>` | `boost::container::flat_multiset` |
| `Store<value<int>, monotone, stable_refs>` | `SegmentedVector` (thread-safe with `concurrent`) |
| `Store<key<int>, value<int>>` | `boost::unordered_flat_map` |
| `Store<key<int>, value<int>, sorted>` | `std::map` |
| `Store<key<int>, value<int>, dense>` | `IndexMap` (a slot per index) |
| `Store<key<int>, value<int>, dense, constant_clear>` | `IndexMap` with generations |
| `Store<key<int>, value<int>, insertion_ordered>` | `OrderedMap` (with `dense`: over a `SparseSet`) |
| `Store<key<int>, value<int>, multi>` | `PostingMap` (flat hash table of value vectors) |
| `Store<key<int>, value<int>, multi, sorted>` | `std::multimap` |
| `Store<key<int>, value<int>, multi, dense, frozen>` | `CsrMultiMap` (compressed sparse rows) |
| `… multi, dense, frozen, bidirectional` | `BidirectionalCsr` (a frozen graph with its in-edges) |
| `Store<key<int>, value<int>, scoped>` | `ScopedMap` (scopes undone by a log) |
| `Store<key<int>, value<int>, layered>` | `Layered` flat hash map over a parent |
| `Store<key<int>, change_log>` | `ChangeLogged` flat hash set |
| `Store<key<int>, partition>` | `DisjointSets` (union-find) |
| `Store<value<int>, fifo>` / `lifo` / `by_priority` | `Fifo` (deque) / `Lifo` (vector) / `Heap` (binary heap) |
| `Store<value<Store<value<int>, frozen>>>` | `Jagged` (CSR rows, any depth) |
| `Store<value<Row>, handles, unique>` | `RowInterner` (each row once, in full) |
| `Store<value<Row>, handles, unique, structural_sharing>` | `TreeVectorSet` (value rows) / `TreeBitsetSet` (rows of indices) |
| `Store<value<T>, handles>` | `SlotVector` (stable slots, freed ones reused) |

`Store<…>::representation` names the choice, for tests that pin it; code that uses a Store never needs it.

## Properties

Each property answers one question (a category) with a tag; a tag derived from another promises at least as much,
and the root of a category promises nothing. List them in any order; equal requests are the same type. Containers
declare the guarantees in `container_traits`; the rest only a request states.

| Category | Tags (later ones promise more) | Guarantees |
|---|---|---|
| order | `sorted`, `insertion_ordered` | iteration ascending by `urd::Less`, or in the order elements first came |
| uniqueness | `unique` | no two elements equal (implied for sets, and for maps without `multi`) |
| keys | `dense`, `sparse` | keys are indices 0..n-1 (`urd::index_of`), or few in their range |
| references | `stable_refs` | a reference stays valid until its element is removed |
| layout | `contiguous` | elements next to each other in memory |
| representation | `canonical` | equal contents, equal representations |
| threads | `concurrent` | inserts (and, for pools, the operations they offer) from several threads at once |
| memory | `retains`, `reclaims` | storage freed by erase and clear is kept for reuse, or returned |
| clear | `proportional_clear` → `constant_clear` | clear costs O(elements present), or O(1) |
| access | `fifo`, `lifo`, `by_priority` | a queue: reached by front or top, not iterated |
| handles | `handles` | a pool: insert gives a handle; with `unique`, equal rows get equal handles |
| sharing | `structural_sharing` | equal parts of different rows are stored once |
| scopes, layering | `scoped`, `layered` | changes undone by `pop_scope`; lookups falling through to a parent |
| tracking | `change_log` | `added()` and `removed()` since `clear_changes()` |
| grouping, direction | `partition`, `bidirectional` | union-find classes; a map answering `keys(value)` too |

| Requested only | Tags | Effect |
|---|---|---|
| lifecycle | `monotone` → `frozen` | only grows (no erase), or never changes after construction |
| emptiness | `nonempty` | at least one element |
| multiplicity | `functional`, `multi` | one value per key, or many (a multimap) |
| identity | `positional` | elements identified by position (sequences, and sets with positions) |
| workload | `small` | a few elements: picks compact containers, never changes a guarantee |

The kind of a Store follows from its arguments: `key<K>` is a set, `key<K>, value<V>` a map, `value<V>` a sequence;
an access tag makes a value Store a queue, `handles` a pool, `partition` a key Store a partition.

## Cost per operation

n elements; U the key range (largest index + 1); r a row's length; k the changes in a delta; A the tree arity (4).
\* amortized (growing the storage); ~ expected (hashing).

**Sets and maps**

| Container | insert | lookup | erase | clear | iterate | memory |
|---|---|---|---|---|---|---|
| `boost::unordered_flat_set/map` | O(1)\*~ | O(1)~ | O(1)~ | O(capacity) | O(capacity) | ~n/0.875 slots + 1 byte each; retains |
| `std::unordered_set/map` | O(1)\*~ | O(1)~ | O(1)~ | O(n + buckets) | O(n) | a node per element; stable references |
| `std::set/map`, multi | O(log n) | O(log n) | O(log n) | O(n) | O(n), in order | a node per element; reclaims |
| `boost::container::flat_set/map`, multi | O(n) | O(log n) | O(n) | O(n) | O(n), contiguous | n elements; retains |
| `DenseSet` (bitset) | O(1)\* | O(1) | O(1) | O(U/64) | O(U/64 + n), in order | U bits; set algebra by words |
| `SparseSet` | O(1)\* | O(1) | O(1); O(n) keeping order | O(1) for trivial keys | O(n), contiguous | n keys + U positions |
| `IndexMap` (dense map) | O(1)\* | O(1) | O(1) | O(U); O(1) with generations | O(U), in key order | a slot per index |
| `IndexedSet` | O(1)\*~ | O(1)~ | O(n) (keeps order) | O(capacity) | O(n), contiguous | n elements + a table of positions |
| `OrderedMap` | as its key set | as its key set | O(n) (keeps order) | as its key set | O(n); keys and values as spans | key set + n values |
| `PostingMap` (multi) | O(1)\*~ | O(1)~; `values(k)` a span | O(r) per value, O(1)~ per key | O(capacity + n) | O(n) | one vector of values per key |
| `CsrMultiMap` (frozen) | build O(n + U) | O(1); `values(k)` a span | — | — | O(n + U), in key order | n values + U + 1 offsets |
| `ScopedMap` | O(1)\*~ | O(1)~ | O(1)~ | O(capacity) | O(capacity) | + a log entry per change in a scope; `pop_scope` O(changes) |
| `Layered` | O(1)\*~ | O(1)~ own + parent | O(1)~ own | O(capacity) | own elements | the parent is not owned |
| `ChangeLogged` | O(1)\*~ | O(1)~ | O(1)~; O(added) for one added since the reset | O(n) | O(capacity) | + the elements added and removed |
| `ConcurrentIndexedSet` | O(1)\*~ (new ones under a lock) | O(1)~ without a lock | — | O(capacity) | O(n) | segments that never move |

**Sequences, queues, partitions**

| Container | push / insert | access | erase | clear | memory |
|---|---|---|---|---|---|
| `std::vector` | O(1)\* at the end, O(n) inside | O(1) | O(n) | O(n) | contiguous; retains |
| `SegmentedVector` | O(1)\* (no element moves) | O(1) | — | O(n) | doubling segments; stable references |
| `Jagged` (frozen rows) | O(r)\* per row | a row: O(1), a span | — | O(n) | one offsets array per level |
| `Fifo` / `Lifo` | O(1)\* | front / top O(1) | `pop` O(1) | O(n) | deque / vector |
| `Heap` | O(log n) | `top` O(1) | `pop` O(log n) | O(n) | vector; retains |
| `DisjointSets` | `insert` O(1)\* | `find`, `same`, `merge` O(α(n))\* | — | O(1) | n parents + n sizes |

**Pools** (rows by handle)

| Container | insert a row | insert a delta | read a row | `contains` | memory |
|---|---|---|---|---|---|
| `RowInterner` | O(r)~ | O(r)~ (copies the row) | O(r) | O(r) | each distinct row once, in full |
| `TreeVectorSet` | O(r)~ | O(k log_A r)~ | O(r) | — | nodes shared across rows: a delta adds about k·log_A r nodes |
| `TreeBitsetSet` | O(r/32)~ | O(k log_A (r/32))~ | O(r/32 + set bits) | O(log_A (r/32)) | as above; all-zero subtrees take nothing |
| `SlotVector` | O(1)\* (a freed slot first) | — | O(1) by handle | — | stable slots; erased elements keep their storage |

Measured, not only bounded: for 20,000 search states that each differ from a parent in 3 positions, a tree pool
takes 136–271 bytes per state against 447–4,409 for rows stored in full, and is faster from a thousand positions on
([docs/BENCHMARKS.md](docs/BENCHMARKS.md)).

## Using it

```sh
cmake -S src -B dependencies-build -DCMAKE_INSTALL_PREFIX="$PWD/dependencies-install" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build dependencies-build && cmake --install dependencies-build    # Boost, GoogleTest, Google Benchmark
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8 && ctest --test-dir build
```

Link the `urd::urd` target and include `urd/store.hpp`. Requests use only the vocabulary of `urd/traits.hpp` and
`urd/properties.hpp`; what `urd/repr` holds is the representations' business, not code that uses a Store (a check
keeps containers out of the include root).

- [examples/](examples/): sets, sequences, maps, rows, dense keys, interning, queues, scopes, states, and more.
- [docs/INTEGRATION.md](docs/INTEGRATION.md): adding a container (declare its traits, offer what they require,
  register it, test it), and every property and facet in detail.
- [docs/BENCHMARKS.md](docs/BENCHMARKS.md): the measurements behind the choices.
- [docs/SURVEY.md](docs/SURVEY.md): the data structures of yggdrasil, loki, tyr and runir, and how each is written
  as a request.
- [docs/ROADMAP.md](docs/ROADMAP.md): what is done and what is next.
