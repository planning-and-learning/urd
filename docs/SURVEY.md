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
| 1 | **Interner**: value ↔ dense, stable ID | yggdrasil repositories, `IndexedHashSet`, `RawArraySet`; runir `PropertyMap`, `SignatureInterner`, get-or-create vertex maps (3×), `EquivalenceCoordinator`; tyr state storages; loki union types | key, `interned`, dense and stable IDs, monotone, canonical; optionally concurrent and `layered` over a frozen parent | `Store<key<T>, positional, monotone>`: `IndexedSet`; with `concurrent`, `ConcurrentIndexedSet`; rows interned by `handles, unique`; `layered` still to come |
| 2 | **Dense ID map** | search-node storages (3×), `PatternDatabase`, `vector<optional<T>>` memos, tyr `FDRContext` (concurrent, write once), unpacked state storages, the 224 dense-keyed hash maps | dense key, value, `auto_resize` or `fixed_size`, `partial` values; optionally stable refs or concurrent | `IndexMap` for `dense` keys (`urd::index_of`); `constant_clear` gives generations |
| 3 | **Dense ID set** (bitset, sparse set) | tyr `PredicateFactSets`, `PredicateHeadUpdates`, FF and LM-cut markings; runir `SetOperationWorkspace`, flag vectors (19) | dense key, bounded universe, clear strategy | `DenseSet` (bitset) and `SparseSet` (O(1) clear) for `dense` keys; set algebra on every set |
| 4 | **Two-level dense map**: (relation, row) → value | tyr `DenseRelationMap` (generation clear, concurrent) and its users: `PredicateAnnotationMap`, `NumericIntervalAnnotations`, `IndexedAchievers`, `GroundRuleDependencies` | composite dense key, `auto_resize`, generation clear | nested dense Stores, the outer with `constant_clear`; no new representation |
| 5 | **Frozen CSR multimap** (jagged array) | runir `StaticGraph` (the CSR build is written twice, plus an O(V·E) build for nauty); tyr `EffectAtomIndex`, consistency-graph indexes, listeners; yggdrasil `MustPass` sources | dense key, multi, frozen, built through a builder | `Store<key<K>, value<V>, multi, dense, frozen>`: `CsrMultiMap`, `values(key)` a span; frozen rows by position were already `Jagged` |
| 6 | **Hash postings** (inverted index) | yggdrasil `JoinIndex`, `KeyIndex`, join and semijoin evaluators (maintained under swap-erase); tyr invariant postings (2× identical) | multi, sparse keys, kept in sync when rows move | `Store<key<K>, value<V>, multi>`: `PostingMap`, `values(key)` a span; sync under swap-erase is the caller's |
| 7 | **Graph** | runir static, bidirectional and dynamic graphs; yggdrasil `DistanceGraph`; tyr kckp graphs | mirrored in/out edges, dense or sparse vertices, `id_recycling`, frozen | `Store<key<V>, value<V>, multi, dense, frozen, bidirectional>`: `BidirectionalCsr`, successors `values(v)`, predecessors `keys(v)`; edge properties and dynamic graphs still to come |
| 8 | **Priority queue** | yggdrasil `OpenList`, addressable Fibonacci heaps, bucket histogram; tyr scheduler (lazy-deletion indexed heap), `CostBuckets` and `PendingPredicateAchievers` (duplicates), `PriorityQueue`, BrFS deque | `by_priority` (`addressable`, `monotone_priority`), `fifo`, `lifo` | `Store<value<T>, by_priority>`: `Heap`, with `order_by<P>` for tie-breaks such as larger g first; `fifo`/`lifo` for breadth- and depth-first; `handles` gives `update(h, e)` and `erase(h)` (no lazy deletion), `monotone_priority` a bucket queue; decrease-key by key rather than handle still to come |
| 9 | **Memo table** | runir `DenotationCaches` (two generations), optimizer memos; loki `TranslationStorage` (about 40 maps, layered over a parent); yggdrasil `JoinIndexCache`, memos keyed by bit vectors and 64-bit subsets | map, monotone within a generation, reset policy, tuple or bitset keys | maps, flat hash tables by default; `constant_clear` (M1) or `layered` over a parent; two generations are two maps swapped (examples/scopes.cpp) |
| 10 | **Scoped symbol table** | loki parse and copy contexts; tyr `ParameterIndexMapping`; runir parser maps (about 40 sites), `ReferenceTable` | `scoped` (push and pop, innermost first), string keys | `Store<key<K>, value<V>, scoped>`: `ScopedMap`, push and pop undo a scope's changes |
| 11 | **Insertion-ordered unique set** | loki `push_unique`; tyr goal atoms, `IndexedAchievers`; runir statistics leaves; yggdrasil linear-dedup deques | unique, `insertion_ordered`, fast lookup | `Store<key<T>, insertion_ordered>`: `IndexedSet` |
| 12 | **Sorted unique vector** (canonical set) | yggdrasil `canonicalize` (43 calls), interned row sets; tyr `StateDelta`, domains; runir colours and marks | sorted, unique, canonical | exists (`flat_set`, also as frozen rows) |
| 13 | **Partition** (union-find, refinement) | tyr `DisjointSet`; yggdrasil estimation union-find; runir `SccRefinementForest`, component buckets, colour classes | `partition`: disjoint, covering, mergeable or refinable | `Store<key<K>, partition>`: `DisjointSets`, merge, find, same; refinement still to come |
| 14 | **Bit matrix** | tyr adjacency matrices, `BinaryCompatibility`, variable dependency graph; runir role denotations (n×n), k-WL tables, Warshall closure | two dense dimensions, fixed size; symmetric or triangular variants | `Store<value<Store<key<I>, dense>>, fixed_shape>`: `Jagged` over one word array, a row a `BitsetSpan`; a uniform stride (no offsets) still to come |
| 15 | **Persistent structure** | yggdrasil tree sets, parent-layered repositories; runir `SearchPath` (pooled cons list), interned `CallStack`; loki `TranslationStorage` copies on inherit | `persistent`, structural sharing, layered overlay | `Store<value<Row>, handles, unique, structural_sharing>`: `TreeVectorSet` (values) and `TreeBitsetSet` (sets of indices), successors by delta; thread-safe with `concurrent` |
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

## tyr's container classes as requests

An audit (2026-10-28) of tyr's bespoke container classes: can each class, not only its members, be one or a few
Store requests? A: expressible now. B: expressible once the named gap is closed (M9). C: domain logic that holds
Stores. Updated as M9 closes gaps.

| Class | Verdict | Request, or the gap |
|---|---|---|
| `PredicateFactSet(s)` | A | `Store<key<Row>, dense>` (DenseSet), one per predicate; `insert(other)` is `unite` |
| `FunctionFactSet(s)` | A (M9.1, M9.2) | `Store<key<Row>, value<Interval>, dense, insertion_ordered>`; `merge(row, interval, hull)` |
| `SubstitutionFunction` | A (M9.1) | `Store<key<Parameter>, value<optional<Object>>, dense, insertion_ordered>` |
| `IndexedAchievers` | A (M9.1) | `Store<key<RuleBinding>, value<Witness>, insertion_ordered>` |
| `PredicateHeadUpdates`, `FunctionHeadUpdates` | A | `Store<key<Binding>, insertion_ordered>` |
| `FunctionAssignmentSet(s)` | A | `Store<value<Interval>>` of a fixed length |
| `RuleCostOverrideStorage` | A | maps to `Cost`, default on a miss |
| Search nodes | A | `Store<value<SearchNode>, monotone, stable_refs>` |
| State storages | A | M8 pools |
| `EffectAtomIndex` | A | frozen CSR per predicate; rebuilding in place: M9 later |
| `PriorityQueue` | A / B | `by_priority` where the element's order puts the key first; else order by projection |
| `PredicateAssignmentSet(s)` | A (M9.3) | `Store<key<Rank>, dense, proportional_clear>`; its `BitsetSpan` view stays tyr's |
| `DenseRelationMap`, `PredicateAnnotationMap` | A (M9.4) | `Store<key<Rel>, value<Store<key<Row>, value<V>, dense, constant_clear[, concurrent]>>, dense>`; `update`, `read` |
| `NumericIntervalAnnotations` | A (M11) | the dense maps above, each row's annotations `Store<key<Interval>, value<Annotation>, sorted, small, order_by<P(interval, annotation)>>`: one per interval, cheapest first; insert-if-cheaper is `merge` |
| `VertexPartitions` | A (M9.5, M10.5) | `Store<value<Store<key<Vertex>, dense>>, fixed_shape>`; `reset()` |
| `AdjacencyMatrix` | A (M10.5) | `Store<value<Store<value<Store<key<Vertex>, dense>>, fixed_shape>>, fixed_shape>`: `rows[v]` is all partitions of v |
| `PartitionedAdjacencyMatrix` | A / B (M10.5) | as above with each source partition's own row widths, `proportional_clear` for the sparse reset (per vertex, where tyr tracks per vertex and partition); the implicit and static-only pairs stay tyr's |
| `DeduplicatedAdjacencyMatrix` | A (M10.6), by composition | specific to k-partite graphs, so no container of its own: a pool of dense-set rows (each partition bitset once, `view(h)` its words), a pool of rows of those handles (each vertex row once), a row handle per vertex (pinned in tests/survey.cpp) |
| `CostBuckets`, `PendingPredicateAchievers` | A (M10.7) | `Store<key<Cost>, value<Bucket>, sorted>` with a `retains` Bucket (`RecyclingMap`); `min_cost` is `begin()`; the stale-entry rule of pending achievers stays tyr's |
| Scheduler, state builder, `PerfectAssignmentHash`, k-CKP workspace, statistics, static/fluent pairs | C | hold Stores |

## Second pass (2026-10-29)

A second reading of the four libraries on their newest branches: yggdrasil `dev-fix-point` (d5fa32f, 2026-10-28), loki
`main` (89fdd8d1, 2026-10-25), tyr `main` (a31ef5fd, 2026-10-25), runir `dev-fix-point` (6e13f139, 2026-10-28; 158
commits past the branch the first pass saw). Every hand-written class was read and given a verdict as in the tyr audit
above (A expressible now, B once a named gap is closed, C domain logic holding Stores); the pins it produced are at the
end of `tests/survey.cpp`. Counts are text counts over `include/` and `src/` (`grep -c` of the type's name), not the
AST counts of the first table, so they are comparable with each other, not with it.

| | yggdrasil | loki | tyr | runir |
|---|---|---|---|---|
| files | 263 | 154 | 526 | 530 |
| `std::vector` | 523 | 208 | 629 | 697 |
| cista vectors (`IndexList`) | 66 | 97 | 224 | 210 |
| hash maps (`UnorderedMap`, `flat_hash_map`, `std::unordered_map`) | 29 | 17 (+40 `ViewMap`) | 224 | 109 |
| hash sets | 11 | 113 | 110 | 27 |
| `std::map` / `std::set` | 16 / 16 | 2 / 5 | 4 / 0 | 12 / 12 |
| bitsets | 43 | 0 | 88 | 25 |
| `std::deque` / priority queues | 2 / 3 hand-rolled heaps | 0 / 0 | 23 / 4 | 5 / 0 |
| keyed by indices or views | ~70 dense maps and sets in disguise | 99 of 132 | 284 of 328 | ~46 of 164 (71 by string) |
| `.clear()` sites | 226 | 28 | 223 | 98 |

### Closed since the first pass

What the first pass or the tyr audit left open and a later milestone answered, now pinned: yggdrasil's `OpenList`
tie-break (f ascending, larger g, later push) is `order_by` with a tuple (M10.2); tyr's `PriorityQueue` is `by_priority,
order_by<&E::get_key>, retains` (its clear by swapping with an empty queue lost capacity); the three heaps with lazy
deletion (yggdrasil `WeightedStratum`, `TreeRepair`; tyr's ground scheduler) are `by_priority, handles`; tyr's
`EffectAtomIndex`, now a CSR per applicability row rebuilt into kept storage, and yggdrasil's `MustPass` groups and
`DistanceGraph` are frozen multimaps rebuilt by `assign` (M10.4); tyr's `TypedRuleSchedulerStratum`, which copies a hash
set into a vector and sorts it every iteration, is `Store<key<Rule>, dense, sorted>` (a bitset iterates ascending);
yggdrasil's active-source array compacted by swap-erase with a position map is `Store<key<V>, dense, contiguous>` (the
sparse set, no new tag); yggdrasil's six result-plus-delta evaluators and datalog `Table` are `change_log`; bit matrices
(tyr `BinaryCompatibility`, `AdjacencyMatrix` and the kckp workspace, three strided accessors; runir's Weisfeiler-Leman
table and Warshall closure) are `fixed_shape` rows of dense sets; loki's union types keyed by a sorted name list are a
sorted map keyed by a frozen sorted Store, and its type table collected in index order at build time is an M11 map
ordered by its values.

### yggdrasil

154 commits since 2026-10-15, mostly in `database/datalog/goal_search`, `search/a_star.hpp`, the tree sets (an `Arity`
parameter, inline leaves, all-zero subtrees as a sentinel: urd's copies should be diffed against them), `SegmentedVector`
and the repositories.

| Class | Verdict | Request, or the gap |
|---|---|---|
| `SegmentedVector` (`containers/segmented_vector.hpp:50`), 14 uses | A | `Store<value<T>, monotone, stable_refs[, concurrent]>`; `emplace_back_with_index(factory)` (a factory run with the reserved index inside the lock) is the one operation without a counterpart |
| `IndexedHashSet` (`containers/indexed_hash_set.hpp:52`), 39 uses | A | `Store<key<Data<T>>, positional, monotone[, concurrent]>` |
| `buffer::IndexedHashSet`, `SegmentedBuffer` (`buffer/`) | A (M12 step 2) | an interner whose elements live serialized in a byte arena: a storage policy outside urd's vocabulary; leave |
| `RawArraySet`, `RawVectorSet`, `BlockArraySet` (37 + 14 `VertexSet` uses; `Builder<Relation>` is one) | A / B | `Store<value<Store<value<T>, frozen>>, handles, unique[, concurrent]>`, `view(h)`; **B for `erase(h)`**: the swap-in that reports which handle moved, so that postings over the rows can follow (gap 3) |
| `RawArrayPool`, `BlockArrayPool`, `BitPackedArrayPool` | B | rows of one length (uniform stride, gap 10), swap-erase, an element width in bits (`BitsetSpan` is width 1) |
| `UnorderedMultiMap` (`containers/unordered_multi_map.hpp:36`), 14 uses | A | `Store<key<K>, value<V>, multi, retains>`; its `replace(key, old, new)` is part of gap 3 |
| `FibonacciHeap` (1 use), `UniqueObjectPool` (13), `SharedObjectPool` (0) | A storage, C handles | `by_priority, handles`; `Store<value<T>, handles>` (the storage is a `SlotVector`); the owning handles stay |
| `BasicSymbolRepository` (`formalism/basic_symbol_repository.hpp:47`), 22 instantiations; `BasicRelationRepository` slots | A (M12: a family of layered flat pools; the relation slots a layered dense map of layered row pools) | the layered interner (gap 2): positions continuing after a frozen parent's, `find` falling through |
| `ConcurrentSlotDirectory` (`basic_relation_repository.hpp:172`) | B (the single-threaded directory is `Store<key<index<P>>, value<Rows>, dense, layered>` since M12) | a dense concurrent map with get-or-create and stable values, no generations (gap 5) |
| `Delta` and the six result-plus-delta evaluators, datalog `Table` | A | `Store<key<Row>, change_log>` |
| `JoinEvaluator`, `SemijoinEvaluator` postings kept in sync under swap-erase (`incremental/join.hpp:22-58`) | B | gap 3 |
| `TupleIndex`, `IndexedTuples` (`datalog/detail/tuples.hpp:25,130`) | B | secondary indexes on a row Store with O(1) per-posting erase (intrusive doubly linked chains); gap 3 and gap 13 |
| `WeightedStratum` (`strata.hpp:160`): settled, queued + best, candidates heap, done flags | A | `Store<key<Row>>`, `Store<key<Row>, value<uint_t>, insertion_ordered>`, `by_priority, handles, order_by<&Candidate::priority>`, `Store<key<size_t>, dense>` |
| `AggregateStratum` (`strata.hpp:244`): `std::map<uint_t, size_t>` as a min/max multiset | A | `Store<value<uint_t>, sorted>` |
| `OpenList`, `AStarStorage`, `SearchTree` (three generation stamps in one node), `BackwardFrontier` | A | the queue above; interner + dense maps; three `dense, constant_clear` maps over the handle; `resize(id + 1)` maps |
| `DistanceGraph` + `m_reverse` in the incremental evaluator | A full, B incremental | frozen multimap by `assign`; a growable dense multimap with `bidirectional` (gap 6) |
| `DistanceEvaluator` sources, flags, touched lists; `DistanceRepairWorkspace` | A | `dense, contiguous`; `DenseSet`; `SparseSet`; `by_priority, handles` composed with dense maps of handles |
| `GenericJoinTrie` (`generic_join.hpp:44`) | B minor | a map whose value holds the map itself (incomplete through indirection), transparent `find(span)` |
| `GoalHeuristic` memos keyed by `vector<uint_t>` (`goal_search.hpp:116,133`) | A / B minor | interners and dense maps; the state interner whose ids keep counting across a wholesale clear is the layered-interner knob (gap 2); the memo keyed by a sorted row is a map keyed by a Store (gap 1) |
| `bound::QueryEvaluator` deques for stable references (`bound.hpp:207-208`) | A | `Store<value<KeyIndex>, monotone, stable_refs>` |
| `incremental::bound::QueryEvaluator` plans keyed by `vector<bool>` (`incremental_bound.hpp:156`) | B | a map keyed by a frozen dense Store (gap 1) |
| estimation and planner memos: `vector<optional<T>>` by query index (4×), subset masks | A | `Store<key<Index<Query<>>>, value<T>, dense>`, `Store<key<uint64_t>, value<Estimate>, monotone>` |

Duplications: the interning layer over a pool (4×: `basic_raw_set`, `basic_array_set`, `indexed_hash_set_impl`,
`buffer/…`); swap-erase keeping parallel structures in sync (6×); heaps with lazy deletion (3×); postings `hash_t ->
position` (8 sites, 2 implementations); the parent-chain fall-through loop (7 copies); the incremental search bundle
(`incremental/min_distance.hpp` vs `goal_search.cpp:601-636`).

### loki

27 commits since 2026-10-15, mostly lint; the 40 `BasicSymbolRepository<T>` are now instantiated once
(`formalism/repository.hpp:46-86`), `(either …)` types became union types, and records list `identifying_members()` as
member pointers, which is what `urd::Members` consumes.

| Class | Verdict | Request, or the gap |
|---|---|---|
| `formalism::Repository`: 40 interners, each layered over the previous phase's (`translator.cpp:139,169`) | A (M12 step 4: `Family(&parent)` over layered pools) | the layered interner (gap 2); the sorted-unique `IndexList` members are a `canonical` promise on the rows |
| `TranslationStorage`: 40 `ViewMap<T>` (`translator/common.hpp:101,116-155`) | A maps, C composition | `Store<key<View<T>>, value<View<T>>, monotone>` each, as a `compose::Family` with `for_each`, which removes the two 40-line call lists (`translation_storage.cpp:95-133,148-187`); the first phase's copy of six domain maps is `layered`; `compose_map` is an algorithm |
| `CopyContext` fields (`copy_context.hpp:36-46`) | A | `variable_bindings` is `scoped` (push and pop, innermost first, `contains` across scopes, iteration of bindings in force: all match `ScopedMap`); `num_quantifications` a map that `retains`; `generated_predicates/axioms` `insertion_ordered` (the merge with fresh seen-sets at `copy_translator_facade.hpp:192-204` becomes one insertion loop); the memos `monotone` maps; `used_predicate_names()` copies a set and unions the domain's names on every generated name: `layered` over a frozen set |
| `push_unique` (6 sites) and its hand-written variants (`compile_typing_translator.hpp:69-80,186-208,249-253`) | A | `Store<key<View<T>>, insertion_ordered>`, then `assign` into the `IndexList` |
| `DomainContext` (`context.hpp:48-79`), nine `clear()` then refill | C holding Stores | string tables that `retain`; `requirement_kinds` a `DenseSet`; `union_type_names` keyed by a sorted name list (pinned); `types` collected in index order at build time: M11 `order_by` on the value's index (pinned) |
| `ParseContext` (`context.hpp:83-98`): `task_objects` looked up before `domain_context.objects` (`checks.hpp:56-59`) | A | `Store<key<std::string>, value<ObjectView>, layered>` over the domain table; `union_types` a sorted set of sorted sets, `merge` is `unite` |
| `variable_scopes` with the RAII `VariableScope` (`builder.hpp:61-74`, 7 uses) | A / B | `Store<key<std::string>, value<VariableView>, scoped>`; **B**: `builder_declarations.cpp:350` asks whether the innermost scope binds the name, which `ScopedMap::contains` answers across all scopes (gap 12) |
| `RequirementCollector` (`infer_requirements.cpp:38`), `NumericGroup` list (`to_effect_normal_form_translator.hpp:89-106`) | A | `Store<key<RequirementKind>, dense, sorted>`; `Store<key<GroupKey>, value<Store<value<Index>>>, insertion_ordered, small>` |
| `declarations` + `declaration_order` (`builder_declarations.cpp:101-109`); `union_bases` (`:113`); `resolving` (`:132`) | A | `insertion_ordered` map; `multi, sorted`; a set |
| `ygg::Data<T>` records: 67 `IndexList` members | B | rows whose storage must be a cista offset vector: out of scope unless urd registers a cista-backed representation |

Duplications: the 40-line per-type call lists (4×); the `find_mapped … remember` memo prologue (60+ sites); the
axiom head-variable set (`formatter.cpp:203-212`, `infer_requirements.cpp:207-215`); innermost-first scope lookup
(parser and translator); the fresh-name search against a taken set (4×); the `=` predicate built twice.

### tyr

The audit above holds: every class is where it was, with three changes of shape. `PredicateHeadUpdates`
(`rule_evaluation.hpp:107`) now keeps a bitset per relation cleared by walking its insertion list (gap 8);
`EffectAtomIndex` (`effect_atom_index.hpp:40`) is a CSR by applicability row rebuilt into kept storage (A, by `assign`);
`PriorityQueue` is A by `order_by`. The 2026-10-23 reverts (forward checking, numeric constraint cost caches, static
variable elimination) put the first table's counts above today's. Classes not in the audit:

| Class | Verdict | Request, or the gap |
|---|---|---|
| `TypedRuleSchedulerStratum` (`lifted/scheduler.hpp:44-84`) | A | `Store<key<Rule>, dense, sorted>` (pinned), `dense, frozen` for the full-enumeration rules |
| ground `Scheduler` queues with lazy deletion (`ground/scheduler.hpp:139-173`) | A / B | `by_priority, retains`; with decrease-key by key (gap 7) the stale-pop protocol and `num_stale_queue_pops` go |
| `GroundRuleDependencies` (`ground/workspaces/program.hpp:43-49`): two generation maps never bumped | A | a frozen multimap per relation in a frozen dense map (pinned) |
| `StagedPredicateAnnotation` (`annotation_types.hpp:612`): a few entries scanned, insert-if-cheaper | A | `small` map, `merge(key, v, cheaper)` |
| `RuleWorkspace<Lifted>` (`lifted/workspaces/rule.hpp:57-141`): `std::deque<Worker>` for address stability; debug `seen_bindings` keyed by an `IndexList` | C / B | `Store<value<Worker>, monotone, stable_refs>`; a set keyed by a Store (gap 1) |
| `ProgramWorkspace<Lifted>` (`workspaces/program.hpp:70-71,125-126`): `vector<unique_ptr<T>>` and `vector<optional<T>>` by rule | B / A | `dense, stable_refs` (gap 5); `dense, frozen` |
| `FactsWorkspace` (`lifted/workspaces/facts.hpp:32-36`): fact sets plus delta fact sets | B | `Store<key<Row>, dense, change_log>` (gap 4) |
| `StaticConsistencyGraph` mappings, `RuleStrata`, `GroundAxiomStrata`, `VariableDomains` | A | `Jagged` at two levels; `Store<key<Object>, sorted, frozen>` per domain |
| `BinaryCompatibility` (`static_literal_compatibility.hpp:36-64`), `AdjacencyMatrix`, kckp `Workspace` rows | A | `fixed_shape` bit rows (pinned); the tri-state "unconstrained" flag stays; uniform stride is gap 10 |
| `NumericSupportSelectorWorkspace` (`numeric_support.hpp:482-510`): entries found by scan, `stable_sort`ed by cost with ties by arrival | B | ties by arrival under `order_by` (gap 11), also the counter tyr puts in the key tuple of its open lists |
| `DynamicNoveltyTable` (`iw/novelty_table.hpp:48-53`): segmented bit vectors under one mutex | B | a dense concurrent set (gap 5); the test-and-insert across arities stays |
| `FDRContext` (`fdr_context.hpp:61-78`): write-once segmented slots with a ready flag | A | `dense, constant_clear, concurrent` (presence as the generation), though the clear is never used (gap 5) |
| BrFS deque, `StateDelta` sorted by key, `TmpVariableDomain` grown to a fixpoint, `ParameterIndexMapping`, `MutableInvariant` | A | `fifo`; `sorted, frozen` flat maps; `dense, monotone`; `scoped`; sets |
| `LMCutImplementation` (`lmcut.cpp:127-141`): 14 hash containers cleared per state, a deque of buffers | C holding Stores | each `retains`; the goal zones over (relation, row) keys are gap 8 |
| `FFRPGHeuristic` markings `vector<dynamic_bitset>` (`rpg_ff.cpp:169`) | A | nested dense, `constant_clear` |
| `ApplicableActionProgram::SchemaPrograms` (`programs/action.hpp:51-58`) | A | `dense, frozen` maps by action and by (action, effect) |
| Match tree node data (cista), `GroundSchedulerScratch`, statistics, search policies | C | — |

Duplications beyond the first list: the binary heap written twice; five relation-row clear strategies, not three (full
reset, inserted-list walk, touched blocks, touched (vertex, partition) bits, generation); the strided bit-matrix row
accessor (4×); the interval hull merge (2×) and "replace only if cheaper" (4×), both `merge`; the predicate/function
twins with their `get(Tag)` accessors (8×, a second `Family`); sort-and-unique canonicalisation (7×).

### runir

The October work is the incremental description-logic engine (`kr/dl/semantics/incremental/`), distance features
with pattern databases, Datalog modules, and search storage policies; graphs and datasets are as the first pass saw them.

| Class | Verdict | Request, or the gap |
|---|---|---|
| `StaticGraph` (two CSR builds), `BackwardStaticGraphView` (a third), `BidirectionalStaticGraph`; all dataset graphs | A by composition, B as one request | two `CsrMultiMap<Vertex, Edge>` from `(source, e)` and `(target, e)`; `bidirectional` as one request needs in-edges as edge ids (gap 9) |
| `StaticGraphBuilder` adjacency lists, `DynamicGraph` (free lists + maps; no site instantiates it) | A by nesting, B as one request | `Store<value<Store<value<Edge>>>>`; `Store<value<Vertex>, handles>` is its id recycling; a growable dense multimap (gap 6) |
| `bgl` storages chosen by `conditional_t<IsDense, vector, UnorderedMap>` (`bgl/algorithms.hpp:79-96`), `Coloring<G>` | A | exactly `dense` or not |
| `SignatureInterner` (stores each signature twice), sort-and-remap (4×), key-to-vertex builders (3× + 2 lambdas) | A | `Store<key<T>, positional, monotone>`: the position is the dense id |
| `nauty::SparseGraph` build: `std::set<pair>` scanned once per vertex (O(V·E)) | A build, C class | `Store<key<Edge>, sorted, frozen>` then a frozen dense multimap whose offsets and values are nauty's arrays |
| `EquivalencePolicy<GI>` (`gi.hpp:35`): a map under a mutex, find-or-allocate | A | `concurrent` map, the allocation in `update` (pinned; see the registry note below) |
| `ColorRepository` (`object_graph_color.hpp:183`) | A | a `Family` of concurrent interners (pinned) |
| `DenotationCaches` (`denotation_caches.hpp:21-44`): static and dynamic partitions, a new `ProgramCache` | A | two `Family`s of maps that `retain`; `reset_dynamic` clears one (not two swapped generations, as pattern 9 says) |
| `DenotationRepository` block pool, `Builder<Denotation<Role>>` rows of words | A | `RowInterner` with `view(h)`; `fixed_shape` bit rows |
| `DenotationState<C>` (`incremental/detail/denotation_state.hpp:16,110`): a builder plus exact added/removed, role rows diffed by XOR of words; held by about 12 evaluators | B | `change_log` on a dense set and on `fixed_shape` bit rows (gap 4) |
| `SetOperationWorkspace`, `ClosureWorkspace` (Tarjan arrays and stacks) | A / C | `SparseSet`; dense maps and `lifo` Stores |
| `EvaluationGraph` (`incremental/evaluation.hpp:33,80-112`): nodes interned by linear scan (three scans) | A | `Store<key<NodeKey>, value<Node>, insertion_ordered>`, the position the evaluation index |
| `EvaluationWorkspace` (`evaluation_workspace.hpp:37-80`): three deques found by linear scan | A | `Store<key<FeatureKey>, value<F>, stable_refs>` once the key has a hashable surrogate for its `weak_ptr` |
| `PatternDatabase`, `ProjectionHeuristic` (`heuristics/projection.hpp`): `vector<bool>` targets compared by `!=`, BFS queue, byte-string keys | A | `DenseSet` (equal by words), `dense, frozen` map, `fifo`, sorted maps by byte rows |
| `QueryOptimizer`, `RecordOptimizer` memos over `RecordKey` (shared since d9d2e216) | A | two-level dense maps |
| `GeneratedSentences` (`generate.cpp:52`): `std::map` keyed by a nonterminal index; `SeenDenotationsFor` rows; deque of storages | A | nested dense maps; `RowInterner`; `monotone, stable_refs` |
| `FeatureResolver`, `DatalogSection`, `ProgramBuilder` (`parser/datalog.*`), `ReferenceTable` (6×), the per-kind feature tables passed in pairs through five signatures | A | string maps, `sorted` where iterated by name, `positional, monotone` where the value is `size()` at insertion, `insertion_ordered` for `ReferenceTable` (pinned), a `Family` of the per-kind tables, `scoped` where modules nest |
| search nodes in `SegmentedVector` grown by `while (index >= size) push_back` (ext and base, byte-identical) | B | `Store<key<State>, value<SearchNode>, dense, stable_refs>` (gap 5) |
| icp search nodes with a compaction map, proof-graph builders with sentinel maps (3×), `RuleEvaluationWorkspace` | A | `insertion_ordered` map; `dense` maps (contains = not the sentinel); `Jagged`, `DenseSet` |
| `SccRefinementForest` (`scc_refinement_forest.hpp:32`) | B | a refinable partition, as pattern 13 planned; its parts are dense maps, small sorted witness sets and bitsets |

Duplications: all seven of the first list remain (symbol tables now 6+); new: component buckets `vector<vector>(count)`
filled from `component_of` (5×, a `CsrMultiMap` from pairs), sentinel-valued dense maps `vector<I>(n, max)` (5×),
dense-or-hash `conditional_t` storage (4× plus three adapter pairs), find-by-key-or-append linear scans (6×), bit
matrices three ways.

### Gaps after the second pass, ranked by the sites they unlock

| # | Gap | Sites | Smallest addition |
|---|---|---|---|
| 1 | **A Store as a hashed key.** Done (2026-10-30): `Hash<Store>` for Stores whose contents fix their order (sorted, canonical, sequences); an unordered Store stays unhashable, by design | yggdrasil `incremental_bound.hpp:156` (plans by a bit vector), `goal_search.hpp:116` (memo by a sorted row); tyr `seen_bindings`; loki union keys in hashed form; every "set of sets" | `Hash<Store>` by `hash_range` over its elements (a map's entries), `EqualTo` through its `operator==`; a pin in `tests/store.cpp` |
| 2 | **Layered interner**: positions continuing after a frozen parent's, `find` falling through | loki's 40 repositories, one layer per translation phase and per task; yggdrasil's 22 `BasicSymbolRepository` instantiations and the per-relation slots; `GoalHeuristic::m_states` (ids counting across a clear) | `Layered<IndexedSet>` whose positions add the parent's size (ROADMAP M6's deferred item) |
| 3 | **Swap-erase on interned rows, reported**: `erase(h)` on `RowInterner` that moves the last row in and returns the moved handle; `replace(key, old, new)` on `multi` Stores | yggdrasil: 37 `RawArraySet` sites (`Builder<Relation>` among them), the join and semijoin postings, `ProjectionEvaluator`, `Counts` | the two operations; every sync becomes two calls |
| 4 | **`change_log` over dense sets and fixed-shape bit rows** | runir `DenotationState` and its ~12 evaluators; tyr `FactsWorkspace`'s fact sets plus delta fact sets | the dense half done (M15, 2026-10-29): `Store<key<I>, dense, change_log>` is `ChangeLogged` over the tracked bitset (sorted and tree sets too); for bit rows, `assign_row` recording changed bits by XOR of words, or a logging row proxy, remains |
| 5 | **Dense keys with stable references or concurrent growth**, no generations | search nodes: tyr 3, runir 2 (`dense, stable_refs`); tyr `ProgramWorkspace` rule workspaces; tyr `DynamicNoveltyTable` and yggdrasil `ConcurrentSlotDirectory` (dense concurrent); tyr `FDRContext` | `dense, stable_refs` done (M14, 2026-10-29): `IndexMap` over segmented slots without the lock, with or without generations; `DenseSet` with atomic `fetch_or` for `dense, concurrent` and the concurrent slot directory (a dense concurrent map without generations) remain |
| 6 | **Growable dense multimap, dynamic bidirectional graph** | runir `StaticGraphBuilder`, `DynamicGraph`, `GeneratedSentences`; yggdrasil's incremental `DistanceGraph` + `m_reverse` | `GroupedMultiMap<IndexGroups>` without the frozen build (M3's deferral), then `assign` to freeze |
| 7 | **Decrease-key by key** (`push(key, p)` updates or inserts) | tyr ground scheduler (2), `CostBuckets::update`, pending achievers' stale entries | `AddressableHeap` plus an `IndexMap` of handles: `Store<key<I>, value<P>, dense, by_priority>` |
| 8 | **Insertion order over a two-level dense key** with a proportional clear | tyr `PredicateHeadUpdates`, LMCut goal zones (4), `PendingPredicateAchievers::by_key`, `CostBuckets` buckets, FF markings | `OrderedMap` over any key Store with insert, contains and clear (today an `IndexedSet` or a `SparseSet`) |
| 9 | **Edge-indexed `bidirectional`**: in-edges as edge ids, the value type not the key type | every runir graph (`StateGraph`, `EquivalenceGraph`, the backward BFS) | `bidirectional` taking a projection from the edge to its other end |
| 10 | **Uniform stride** on fixed shapes (no offsets array) | tyr 4 matrices; yggdrasil's 37 `RawArraySet` sites and both block pools hold rows of one length | the `uniform` tag left out of M9.5 and M10.5; memory, not expressiveness |
| 11 | **Ties by arrival under `order_by`** | tyr `NumericSupportSelectorWorkspace`; the counter tyr adds to its open lists' key tuples | a sequence number the Store keeps, as `FlatEntryMap` and `Heap` can |
| 12 | **Innermost-scope membership** on `ScopedMap`. Done (M15, 2026-10-29): `bound_in_scope(key)` on every `scoped` Store | loki `builder_declarations.cpp:350` (blocks the parser's 7 `VariableScope` uses) | `bound_in_scope(key)` from the log entries since the last `push_scope` |
| 13 | **O(1) posting erase** | yggdrasil `TupleIndex` (4 per datalog table) | a `PostingMap` policy linking values by position; measure against O(r) first |
| 14 | Minor or out of vocabulary: transparent lookup by a probe type (`find` by any sized range: yggdrasil's row sets, `JoinIndexCache`, `GenericJoinTrie`); an interner insert with a factory given the reserved index; a bit-packed element width; a map whose value holds the map; gtl shard tuning (yggdrasil: 128 shards and a spin mutex, measured); cista-backed rows and the serialized-arena interner | — | leave, or verify first |

Found by the pins, and fixed: a `concurrent` map by an indexable key (`Store<key<int>, value<V>, concurrent>`) resolved
to the thread-safe generation `IndexMap`, a slot per index, because the parallel hash map stood after the dense containers
in the registry; it now stands among the hash tables, and `tests/store.cpp` pins it.
