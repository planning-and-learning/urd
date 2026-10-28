# Survey: data structures in yggdrasil, loki, tyr and runir

Which data structures the four libraries use, explicitly or implicitly, and which of them could become
`urd::Store`s. This survey is about coverage: it guides which properties and representations urd adds next.
It does not integrate urd into the libraries.

Method: an AST scan (libclang) of every container declaration, text counts of common idioms, and a reading
of the hand-written structures in each library. Counts are per declaration: a container passed through five
functions counts five times, so the "as fields" column is closer to the number of distinct structures.

## Containers

| Container | yggdrasil | loki | tyr | runir | total | as fields |
|---|---|---|---|---|---|---|
| `std::vector` | 327 | 251 | 908 | 776 | 2,262 | 751 |
| cista vector | 55 | 97 | 244 | 207 | 603 | 251 |
| `flat_hash_map` | 19 | 71 | 423 | 47 | 560 | 267 |
| `flat_hash_set` | 10 | 117 | 115 | 15 | 257 | 41 |
| `dynamic_bitset` | 16 | – | 50 | 2 | 68 | 26 |
| `std::unordered_map` | 2 | – | – | 61 | 63 | 3 |

All four libraries together hold 4,274 container declarations. Of them:

- 224 hash containers are keyed by dense indices, where a vector or a bitset would do;
- about 330 more are keyed by views (183 view → view maps, 141 view sets); a view wraps a dense index, so
  most of these are dense too;
- 581 vectors hold dense indices, many of them sets or lists in disguise;
- 56 maps from strings to dense indices are symbol tables.

## Patterns that could become Stores

The last column says what urd covers now; [ROADMAP.md](ROADMAP.md) orders the rest, and `tests/survey.cpp`
restates declarations from the libraries as Store requests.

| # | Pattern | Where (examples) | Properties | urd today |
|---|---|---|---|---|
| 1 | **Interner**: value ↔ dense, stable ID | yggdrasil repositories, `IndexedHashSet`, `RawArraySet`; runir `PropertyMap`, `SignatureInterner`, get-or-create vertex maps (3×), `EquivalenceCoordinator`; tyr state storages; loki union types | key, `interned`, dense and stable IDs, monotone, canonical; optionally concurrent and `layered` over a frozen parent | `Store<key<T>, positional, monotone>`: `IndexedSet`, an element's position is its index; concurrent and `layered` still to come |
| 2 | **Dense ID map** | search-node storages (3×), `PatternDatabase`, `vector<optional<T>>` memos, tyr `FDRContext` (concurrent, write once), unpacked state storages, the 224 dense-keyed hash maps | dense key, value, `auto_resize` or `fixed_size`, `partial` values; optionally stable refs or concurrent | `IndexMap` for `dense` keys (`urd::index_of`); `constant_clear` gives generations |
| 3 | **Dense ID set** (bitset, sparse set) | tyr `PredicateFactSets`, `PredicateHeadUpdates`, FF and LM-cut markings; runir `SetOperationWorkspace`, flag vectors (19) | dense key, bounded universe, clear strategy | `DenseSet` (bitset) and `SparseSet` (O(1) clear) for `dense` keys; set algebra on every set |
| 4 | **Two-level dense map**: (relation, row) → value | tyr `DenseRelationMap` (generation clear, concurrent) and its users: `PredicateAnnotationMap`, `NumericIntervalAnnotations`, `IndexedAchievers`, `GroundRuleDependencies` | composite dense key, `auto_resize`, generation clear | nested dense Stores, the outer with `constant_clear`; no new representation |
| 5 | **Frozen CSR multimap** (jagged array) | runir `StaticGraph` (the CSR build is written twice, plus an O(V·E) build for nauty); tyr `EffectAtomIndex`, consistency-graph indexes, listeners; yggdrasil `MustPass` sources | dense key, multi, frozen, built through a builder | `Store<key<K>, value<V>, multi, dense, frozen>`: `CsrMultiMap`, `values(key)` a span; frozen rows by position were already `Jagged` |
| 6 | **Hash postings** (inverted index) | yggdrasil `JoinIndex`, `KeyIndex`, join and semijoin evaluators (maintained under swap-erase); tyr invariant postings (2× identical) | multi, sparse keys, kept in sync when rows move | `Store<key<K>, value<V>, multi>`: `PostingMap`, `values(key)` a span; sync under swap-erase is the caller's |
| 7 | **Graph** | runir static, bidirectional and dynamic graphs; yggdrasil `DistanceGraph`; tyr kckp graphs | mirrored in/out edges, dense or sparse vertices, `id_recycling`, frozen | `Store<key<V>, value<V>, multi, dense, frozen, bidirectional>`: `BidirectionalCsr`, successors `values(v)`, predecessors `keys(v)`; edge properties and dynamic graphs still to come |
| 8 | **Priority queue** | yggdrasil `OpenList`, addressable Fibonacci heaps, bucket histogram; tyr scheduler (lazy-deletion indexed heap), `CostBuckets` and `PendingPredicateAchievers` (duplicates), `PriorityQueue`, BrFS deque | `by_priority` (`addressable`, `monotone_priority`), `fifo`, `lifo` | `Store<value<T>, by_priority>`: `Heap`; `fifo`/`lifo` for breadth- and depth-first; bucket queues and decrease-key still to come |
| 9 | **Memo table** | runir `DenotationCaches` (two generations), optimizer memos; loki `TranslationStorage` (about 40 maps, layered over a parent); yggdrasil `JoinIndexCache`, memos keyed by bit vectors and 64-bit subsets | map, monotone within a generation, reset policy, tuple or bitset keys | maps, flat hash tables by default; `constant_clear` (M1) or `layered` over a parent; two generations are two maps swapped (examples/scopes.cpp) |
| 10 | **Scoped symbol table** | loki parse and copy contexts; tyr `ParameterIndexMapping`; runir parser maps (about 40 sites), `ReferenceTable` | `scoped` (push and pop, innermost first), string keys | `Store<key<K>, value<V>, scoped>`: `ScopedMap`, push and pop undo a scope's changes |
| 11 | **Insertion-ordered unique set** | loki `push_unique`; tyr goal atoms, `IndexedAchievers`; runir statistics leaves; yggdrasil linear-dedup deques | unique, `insertion_ordered`, fast lookup | `Store<key<T>, insertion_ordered>`: `IndexedSet` |
| 12 | **Sorted unique vector** (canonical set) | yggdrasil `canonicalize` (43 calls), interned row sets; tyr `StateDelta`, domains; runir colours and marks | sorted, unique, canonical | exists (`flat_set`, also as frozen rows) |
| 13 | **Partition** (union-find, refinement) | tyr `DisjointSet`; yggdrasil estimation union-find; runir `SccRefinementForest`, component buckets, colour classes | `partition`: disjoint, covering, mergeable or refinable | `Store<key<K>, partition>`: `DisjointSets`, merge, find, same; refinement still to come |
| 14 | **Bit matrix** | tyr adjacency matrices, `BinaryCompatibility`, variable dependency graph; runir role denotations (n×n), k-WL tables, Warshall closure | two dense dimensions, fixed size; symmetric or triangular variants | planned: a `uniform` property on nested dense Stores, `repr::BitTensor`, any depth |
| 15 | **Persistent structure** | yggdrasil tree sets, parent-layered repositories; runir `SearchPath` (pooled cons list), interned `CallStack`; loki `TranslationStorage` copies on inherit | `persistent`, structural sharing, layered overlay | `persistent` exists without a representation |
| 16 | **Delta tracking** | runir `DenotationState`, `Delta`; tyr `StateDelta`, full + delta pairs (semi-naive evaluation); yggdrasil incremental relations | `change_log`: exact added and removed elements | `Store<key<T>, change_log>`: `ChangeLogged`, exact `added()` and `removed()` |
| 17 | **Workspace** (any store, reused) | over 600 `clear()` calls with different strategies: generation counter (`DenseRelationMap`), touched list, full fill, epoch stamps, validity flags, swap with an empty container (loses capacity) | `reusable`, with the clear strategy chosen by workload | `retains` keeps storage; `proportional_clear`/`constant_clear` choose the clear strategy |
| 18 | **Type-indexed family** | loki `TranslationStorage`; tyr static/fluent twins (26 accessor sites); yggdrasil `BuilderStorage` | one Store per type of a tag list | `urd::compose::Family<F, Ts...>`: `get<T>()`, `for_each` |

Object pools are pools of slots: `Store<value<T>, handles>` (`repr::SlotVector`, a segmented vector plus the
indices of freed slots), thread-safe with `concurrent`. yggdrasil's `UniqueObjectPool` and `SharedObjectPool`,
with their owning, reference-counted handles, are not copied.

## Auto-resize

A dense map either has a size fixed at construction, or grows when an index past its end is written. The
libraries grow it in four ways: tyr's `DenseRelationMap` per relation lane, the `SegmentedVector` search-node
storages, `ygg::set` and `resize(i + 1)` before a write, and a loop of `push_back` until the index exists.
Lifted state storage auto-resizes; ground state storage has a fixed size. As properties:

- `auto_resize`: writing at any index is allowed and fills the gap with default values;
- `fixed_size`: the size is set once and an index past it is an error (checked in Debug).

Reads past the end are the open question: a default value (what `DenseRelationMap` returns) or an error.

## Duplications

| What | Where |
|---|---|
| variable domains (types, policies, conversions) | tyr `program_domains.cpp`, `task_domains.cpp` |
| bucket queue with node recycling | tyr `CostBuckets`, `PendingPredicateAchievers` |
| relation/row markings, three clear strategies | tyr fact sets, head updates, FF markings |
| postings index | tyr invariant matcher, conjunctive-condition matcher |
| stratification | tyr `stratification.cpp`, `axiom_stratification.cpp` |
| union-find | tyr `DisjointSet`, yggdrasil estimation |
| plan extraction | tyr sequential and parallel search spaces |
| CSR build | runir `static_graph.hpp`, `backward_static_graph_view.hpp`, nauty conversion |
| interner | runir `SignatureInterner` re-implements yggdrasil's `IndexedHashSet` |
| sort vertices and remap to dense IDs | runir, 4× |
| memo rewrite and visited collector | runir `dl/optimization.hpp`, `ps/optimization.hpp` |
| search-node storage and proof graphs | runir base, ext and icp search, 3× |
| key → vertex, edge dedup, builder | runir state, object and equivalence graphs, 3× |
| symbol tables | runir parsers, 5× |

## New properties

In order of how often the patterns above need them:

1. `auto_resize` / `fixed_size` (patterns 2, 3, 4)
2. `interned` (1, 7)
3. `reusable` with a clear strategy: generation, touched list or full (3, 4, 17)
4. `insertion_ordered` (11)
5. priority orders: `by_priority`, `addressable`, `monotone_priority`, `fifo`, `lifo` (8)
6. `scoped` (10)
7. `layered` over a frozen parent (1, 9, 15)
8. `id_recycling` (7)
9. composite dense keys (4, 14)
10. `partition`, `change_log`, `partial` (13, 16, 2)

## Order for urd

By coverage per effort:

1. Dense ID map and set with `auto_resize`/`fixed_size` and clear strategies (2, 3, 17): covers the 224
   dense-keyed hash containers and most markings.
2. Interner (1): register yggdrasil's `IndexedHashSet` and `RawArraySet`, with `interned` and `layered`.
3. Frozen CSR multimap with a builder (5), then graphs on top (7).
4. Postings (6): `UnorderedMultiMap`, already planned for tier 2.
5. Priority queues (8): one kind, three orders.
6. Insertion-ordered set (11) and scoped symbol table (10).
7. Later: two-level dense maps (4), bit matrices (14), partitions (13), persistent structures (15), delta
   tracking (16), type-indexed families (18).
