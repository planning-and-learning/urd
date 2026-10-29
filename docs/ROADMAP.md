# Roadmap: covering the containers of yggdrasil, loki, tyr and runir

## Context

urd now covers sets, sequences, sorted sequences, memory behavior and nested Stores: CSR rows and row pools,
at any depth. The survey (`docs/SURVEY.md`) counted 4,274 container declarations in the four libraries and 18
patterns that could become Stores. Much of that is not expressible yet:
- maps (`flat_hash_map` 560, `std::unordered_map` 63);
- the 224 hash containers keyed by dense indices;
- interners;
- priority queues;
- insertion-ordered sets;
- symbol tables;
- the 600+ `clear()` sites with hand-rolled clear strategies.

This plan sequences the work by coverage per effort, as the survey's "Order for urd" did. It adds the
foundations those patterns need first, and fixes a definition of done that keeps the library as clean as it
is now:
- per container, only `container_traits` and a place in the registry;
- every property checked by a typed test;
- downstream code only ever sees a Store.

It covers the libraries; integrating urd into them stays out of scope, as before. Everything happens in the
urd clone, directly on `main`, one milestone at a time, each with its own short plan when it starts.

## Definition of done (every milestone)

**For a new property:**
- a category in `include/urd/traits.hpp`, with a root that promises nothing;
- the interface it requires or unlocks, in `include/urd/interface.hpp` and `include/urd/store.hpp`;
- a typed test in `tests/properties.cpp` or its own suite, skipped where the property is not declared;
- a sabotage that the test catches, not committed;
- a line in `docs/INTEGRATION.md`.

**For a new representation:**
- its `container_traits` (constrained on what it needs of its elements);
- a place in the preference lists in `include/urd/registry.hpp`;
- an entry in a typed test list;
- pins in `tests/store.cpp` for the requests it now serves.

Code taken from yggdrasil is copied with its tests, as in tier 1 (`docs/INTEGRATION.md` step 1).

**For a milestone:**
- **Survey sites as pins:** a handful of concrete declarations from the four libraries, rewritten as Store
  requests in `tests/survey.cpp`, each with a `static_assert` on what it resolves to. That shows the pattern
  is covered.
- **An example** added to `examples/`.
- **Builds:** GCC and Clang, Debug and Release, green.
- The `urd-design` memory and `docs/SURVEY.md` ("urd today" column) are updated.

## M0: Foundations (needed by most patterns) — done

Maps and multimaps, Boost's flat hash tables first where no order is asked for (docs/BENCHMARKS.md), the
benchmark harness, CI with ThreadSanitizer, clang-tidy and clang-format through peachycheck, test names by
container, survey pins in tests/survey.cpp. One change from the plan below: a map Store's iterators are
const, and values change by key only, as Boost's flat maps would let a key be overwritten.


1. **Maps.** Today `key<K>` with `value<V>` is rejected. Maps follow the standard's map interface:
   - **Interface:**
     - `insert`, `emplace`, `try_emplace`, `insert_or_assign`;
     - `operator[]` where the value is default-constructible and the Store not frozen;
     - `at`, `find`, `count`, `contains`, `erase(key)`;
     - bounds where sorted;
     - for `multi`: `equal_range` and an insert that returns an iterator.
   - **Elements** are `pair<const K, V>`; rows (`value<Store<…>>`) work as in nested Stores.
   - **Representations:** `boost::unordered_flat_map`, `std::unordered_map` (`stable_refs`), `std::map`,
     `boost::container::flat_map`, and their multi variants.
   - **Tests:** the reference model in `tests/verify.hpp` gains a map mode, and the typed tests gain a map
     list.
2. **Boost.Unordered.** Add it to `docs/SOURCES.json` and register `boost::unordered_flat_set` and
   `unordered_flat_map` first among the hash containers. `interface.hpp` relaxes the result of
   `erase(iterator)` to "convertible to an iterator", for Boost's flat tables. Pins move to the faster
   tables; the typed tests confirm their traits (`retains`, no `stable_refs`).
3. **Benchmarks** (`benchmarks/`, Google Benchmark, built only on request):
   - one benchmark per workload (insert-heavy, lookup-heavy, clear-and-refill, iterate);
   - each runs every container that serves a request;
   - the preference orders in the registry then cite these numbers, which the survey asked for ("only
     benchmarks justify workloads").
4. **CI and lint for urd:**
   - a GitHub workflow building GCC and Clang, Debug and Release;
   - a ThreadSanitizer job, so `concurrent` is really checked;
   - peachycheck with the yggdrasil-strength configuration;
   - typed-test names that say the container (a name generator over `examples/show.hpp`'s naming, moved
     into `tests/`).

## M1: Dense ID maps and sets (survey patterns 2, 3, 17) — done

Dense keys (urd::index_of), the clear-cost category, DenseSet, SparseSet, IndexMap with and without generations,
set algebra on every set, the ClearCost test, survey pins, examples/dense.cpp. Changes from the plan below: no
extent or partial/total category (the dense containers grow to fit; a total map is a sequence), and DenseSet is
written for urd rather than copied, since yggdrasil's dynamic bitset is a view over storage it does not own.


This is the largest win: the 224 dense-keyed hash containers, `vector<optional<T>>` memos, and most
markings.

**Properties:**
- `dense` already exists. New: an `indexable` element requirement in `include/urd/elements.hpp`: a key
  converts to a `std::size_t` index, through a user-specializable `urd::index_of<K>`, so views that wrap an
  index qualify.
- An **extent** category: root `any_extent`; `auto_resize` (writing past the end grows, filling defaults);
  `fixed_size` (the size is set at construction, and going past it is a Debug assertion).
- A **values** category for maps: root `partial` (a key may be absent); `total` (every key in range has a
  value; reads return it).
- A **clear cost** category, for the 600+ `clear()` sites:
  - root `any_clear`;
  - `proportional_clear`: O(elements present), as with a touched list or sparse set;
  - `constant_clear`: O(1), as with a generation counter.

  Requesting one picks the strategy; the hand-rolled variants in tyr become requests.

**Representations:**

| Representation | Serves | Source |
|---|---|---|
| `DenseMap`: `std::vector<V>`, plus a presence bitset when `partial` | dense map | new, small |
| `GenerationMap`: values with generation stamps | dense map with `constant_clear` | modeled on tyr `DenseRelationMap` |
| `DynamicBitset` | dense set | copy `yggdrasil/containers/dynamic_bitset.hpp` |
| `SparseSet`: dense and sparse arrays | dense set with `proportional_clear` and iteration | new, small |

**Tests:** typed tests for the extent (writes past the end grow; `fixed_size` asserts) and for each clear
cost. The clear-cost test counts operations through an instrumented element, or times the clear against the
universe size.

**Survey pins:** search-node storages, `PatternDatabase`, tyr `PredicateFactSets`, FF markings, and runir
flag vectors.

**Example:** `examples/dense.cpp`.

**Decision:** whether a read past the end returns a default or is an error. Recommended: a default under
`auto_resize` (what `DenseRelationMap` does), and an error under `fixed_size`.

## M2: Interners and insertion-ordered sets (patterns 1, 11) — done

`insertion_ordered`, positional sets (insertion order by default, so a position is an index), `position(it)`,
`repr::IndexedSet` (vector plus flat hash table of positions), survey pins (runir `SignatureInterner`, loki
`push_unique`, tyr `IndexedAchievers`), examples/interning.cpp. Changes from the plan below: `insert` keeps the
standard's `pair<iterator, bool>` (`position` turns the iterator into the index); yggdrasil's `IndexedHashSet` is
not copied, as it is built on its buffers and arenas. Deferred until a site needs them: a concurrent interner
(tyr's state storages, over `SegmentedVector` for stable references), an insertion-ordered map, and positions
hashed through the vector instead of copied keys.


- **Interner:** `Store<key<T>, positional, monotone>`, the combination that compile-fail case 7 forbids
  today.
  - It becomes legal with `monotone`.
  - `insert` returns `pair<index, bool>`, `operator[]` gives the element at an index, and `find` gives the
    index.
  - Representations: yggdrasil `IndexedHashSet` (copy) and `RawArraySet` for arrays of indices; with M0,
    also a vector plus a `boost::unordered_flat_map` index.
  - With `concurrent`, a thread-safe variant (tyr's state storages).
- **Insertion-ordered set:** the order category gains a sibling, `insertion_ordered`, next to `sorted`.
  `Store<key<T>, insertion_ordered>` is served by the same index-plus-vector representation, so no new
  container is needed.
- **Survey pins:** runir `SignatureInterner` and `PropertyMap`; loki `push_unique`; tyr goal atoms.
- **Example:** `examples/interning.cpp`.

## M3: Frozen CSR multimaps and graphs (patterns 5, 7) — done

`repr::CsrMultiMap` for `Store<key<K>, value<V>, multi, dense, frozen>`, built by a counting sort; `values(key)` on
every multimap Store; graphs as `bidirectional` frozen multimaps (`keys(v)` gives predecessors; first a
combinator `urd::Graph`, replaced by the property); survey pins (runir `StaticGraph`, tyr
effect groups); the rows example shows both. Deferred: edge properties on graphs (key a Store by an edge's
position for now), and a non-frozen dense multimap.


- `Store<key<I>, dense, value<V>, multi, frozen>` resolves to a CSR multimap. It is the jagged array from
  nested Stores, indexed by key.
- **Builder:** construction from unsorted `(key, value)` pairs by a counting sort. This replaces the CSR
  build that runir writes three times.
- **Graphs:** a graph is two CSR multimaps (out-edges and in-edges) over the same vertices. Provide it as a
  combinator `urd::Graph<…>` over Stores rather than a new kind; `id_recycling` (for dynamic graphs) is
  deferred.
- **Survey pins:** runir `StaticGraph`; tyr `EffectAtomIndex`; yggdrasil `MustPass` sources.

## M4: Hash postings (pattern 6) — done

`repr::PostingMap` (a flat hash table of value vectors) first for multimaps, 2–3 times faster than
`std::unordered_multimap` on postings (docs/BENCHMARKS.md); survey pins (yggdrasil `JoinIndex`, tyr invariant
postings as maps of dense sets). yggdrasil's `UnorderedMultiMap` is not copied: the flat table of vectors is
simpler. Sync under swap-erase stays the caller's.


- `Store<key<K>, value<V>, multi>` with sparse keys resolves to yggdrasil's `UnorderedMultiMap` (copy), or
  to `boost::unordered_flat_map<K, row>` with rows from the nested-Store machinery.
- Keeping postings in sync when the indexed rows move (a swap-erase) is left to the caller in this
  milestone.
- **Survey pins:** yggdrasil `JoinIndex` and `KeyIndex`; tyr invariant postings.

## M5: Priority queues (pattern 8) — done

The queue kind: an access category (`fifo`, `lifo`, `by_priority`), the `queue_access` facet (push, emplace, pop,
front or top, clear), `repr::Fifo`, `repr::Lifo`, `repr::Heap`, written for urd since the standard adaptors have
no clear; a typed queue test against a model, with retains; survey pins (yggdrasil `OpenList`, tyr's priority
queue, breadth-first frontiers); compile-fail cases 22–24; examples/queues.cpp. Deferred until a site needs them:
`monotone_priority` (a bucket queue, for integral priorities), `addressable` (decrease-key through a handle), and
orders other than `urd::Less` on the element (OpenList breaks ties by larger g, which a request cannot say yet).


- **A new kind:** elements are accessed by order of removal, not by key or position.
- **Category:**
  - root `any_access`;
  - `fifo`, `lifo`, and `by_priority` (min first, by a projection or `operator<`);
  - `monotone_priority` (priorities never decrease, so a bucket queue works);
  - `addressable` (`decrease_key` through a handle).
- **Interface:** `push`, `top`/`front`, `pop`, `size`, `empty`; with `addressable`, `push` returns a handle
  and `update(handle, priority)` changes it.
- **Representations:**
  - `std::deque` for fifo and `std::vector` for lifo;
  - a d-ary heap, from Boost.Heap or a small one of our own;
  - a bucket queue, which merges tyr's `CostBuckets` and `PendingPredicateAchievers`;
  - yggdrasil's `fibonacci_heap` (copy) for `addressable`.
- **Tests:** the typed tests compare the pop order with a sorted model.
- **Survey pins:** yggdrasil `OpenList`; the tyr scheduler; BrFS deques.

## M6: Scoped symbol tables and layering (patterns 10, 9, 15) — done

`scoped` (`repr::ScopedMap`: an undo log, so pop restores what the scope shadowed) and `layered`
(`repr::Layered<R>` over a parent Store of the same type); facets `scopes` and `layers`; a random test of scopes
against saved copies and a test of layer semantics; survey pins (loki `TranslationStorage`, parser bindings);
examples/scopes.cpp with the two-generation memo. Deferred: a layered interner (positions continuing after the
parent's), layered sorted Stores, and scoped sets.


- **`scoped`:** maps with `push_scope` and `pop_scope`, where lookups find the innermost binding. The
  representation is a vector of `(key, value)` plus a hash map from each key to its stack of bindings. It
  serves loki's parse contexts and runir's parser maps (about 40 sites).
- **`layered`:** a Store that answers from itself, then from a frozen parent it does not own. It serves
  loki's `TranslationStorage` and yggdrasil's parent-layered repositories, and lets the interner (M2) layer
  over a frozen parent.
- **Memo tables (9):** these become maps with `constant_clear` (M1) or `layered`. A two-generation reset is
  a pair of maps swapped each generation: an example, not a new representation.

## M7: Remaining patterns — done in part

Done: the two-level dense map by nesting (pinned and tested), partitions (`Store<key<K>, partition>`, first a class
`urd::Partition`), `urd::compose::Family`,
and `change_log` (`repr::ChangeLogged`), each with a test against a model and a sabotage it catches;
examples/structures.cpp. Not done, for want of a representation worth its weight or a site that needs it now:
`persistent` (tree sets, immer), `handles` identity (hive, slot map), the `uniform` property that would let a
nested dense Store be a bit tensor of any depth (a first two-dimensional `urd::BitMatrix` was removed for it),
symmetric or triangular variants, and `refinable` partitions.


| Pattern | Approach |
|---|---|
| Two-level dense map (4) | nested Stores: `Store<key<Rel>, dense, value<Store<key<Row>, dense, value<V>, auto_resize>>>` with `constant_clear`; no new representation if M1 and nested Stores compose |
| Bit matrix (14) | a uniform-length nested dense set: rows of equal length need no offsets; a `uniform` property makes it one bitset with a stride |
| Partition (13) | a new kind, union-find, with `merge`, `find` and `same`; `refinable` for colour refinement later |
| Persistent (15) | a `persistent` property (removed until it has a representation and a test): yggdrasil `tree_vector_set` and `tree_bitset_set` (copy), or immer if its license and size fit |
| Delta tracking (16) | a `change_log` property: the Store records exactly what was added and removed since the last reset, for semi-naive evaluation |
| Type-indexed family (18) | a combinator over Stores, one per type in a tag list; not a Store |
| Hive and slot map | `handles` identity (from the earlier discussion), for object pools whose objects need stable addresses and reuse |

## M8: Rows shared between states (pattern 15) — done

Pools: the `handles` identity (interning with `unique`, slots without), `structural_sharing`, the `pool` and
`slots` facets. Representations: `RowInterner`, `TreeVectorSet` and `TreeBitsetSet` (copied from yggdrasil),
`SlotVector`, `ConcurrentIndexedSet`, each thread-safe with `ThreadSafe`; `BitsetSpan` from yggdrasil for bit
operations, which `DenseSet` uses too. Deltas by writes (value rows) and by added and removed elements (set rows),
`contains` along one path. Tests: run_pool and run_slots against the rows given, sharing bounds, threaded tests under
ThreadSanitizer, yggdrasil's tree tests ported; benchmarks in docs/BENCHMARKS.md; examples/states.cpp. Deferred:
tree pools over rows that are maps, and per-handle generations for slots (stale handles are not detected).

## M9: tyr's container classes as requests

From the audit in docs/SURVEY.md ("tyr's container classes as requests"), with as little new code as possible:
policies and reuse over new containers.

1. Maps in insertion order — done: `insertion_ordered` on maps, the `entries` facet, `repr::OrderedMap<Keys, V>`
   over an existing insertion-ordered set, and a `KeepOrder` policy on `SparseSet`.
2. Merging values and reporting change — done: `merge(key, value, combine) -> bool` on maps with unique keys,
   `unite(other) -> bool` on sets.
3. A bitset with touched-word clear — done: a `TrackWords` policy on `DenseSet` (`proportional_clear`, sorted).
   The borrowed word view is left out: no property can promise a bitset, and the set algebra works by words.
4. DenseRelationMap — done: `update(key, f)` and `read(key, f, missing)` on maps with unique keys; the generation
   map hands stale values to `update`; a `ThreadSafe` policy on `IndexMap` (segmented slots, a spin lock each from
   yggdrasil) for `concurrent`. (relation, row) keys are nested dense Stores, so clearing all costs one clear per
   relation rather than one counter.
5. Fixed-shape rows — done: `fixed_shape` (number and lengths of rows set at construction by `urd::row_lengths`,
   contents mutable), served by `Jagged`: rows of elements as mutable spans, rows of a dense set as `BitsetSpan`s
   over one word array (callers name `Rows::reference`). Left out: a `uniform` shape (stride instead of offsets),
   nested fixed shapes (vertex × partition), and resetting all contents.
6. Later: sorted maps keeping value storage or a bucket queue, borrowed pool rows, order by projection, CSR rebuilt
   in place.

## Generalization (after M9.5)

Containers that repeated one another became one class with a policy; requests resolve as before, the old names
are aliases:

| Merged | Into | Lines |
|---|---|---|
| bit rows | `Jagged` (fixed-shape rows of bits) | the separate class removed |
| `CsrMultiMap`, `PostingMap` | `GroupedMultiMap<IndexGroups or HashGroups>` | 407 → 405: one interface, not fewer lines |
| `IndexedSet`, `ConcurrentIndexedSet` | `IndexedSet<…, ThreadSafe>` | 383 → 256 |
| `Fifo`, `Lifo`, `Heap` | `Queue<T, Order>` | 125 → 108 |

Kept apart: `RowPool`, whose only job (rows kept for reuse at every level) a plain sequence cannot promise; and
`Jagged` with `CsrMultiMap`, which share little more than offsets, while building a `Jagged` of Stores inside
`urd/repr` would make the representations depend on `Store`.

## M10: gtl, order by projection, priority queues, the remaining row gaps

1. gtl — done: the dependency as yggdrasil installs it (same revision and patches); `gtl::parallel_flat_hash_set`
   and `parallel_flat_hash_map` (16 shards, a mutex each) for concurrent sets and maps by hashed keys, which had no
   representation before; `update` and `read` through the table's own locked calls, in the facet. gtl's B-trees
   are left out (`btree_map` lacks `insert_or_assign`) until a benchmark asks for them.
2. Order by projection — done: `order_by<P>` (a member pointer or a stateless lambda; a tuple gives tie-breaks, such
   as larger g first) on sorted Stores and priority queues. `urd::ProjectedLess<T, P>` compares `P(element)`, then the
   element where it has an order; unique keys must have one, or two with equal `P` would be one. A request with
   `order_by` chooses among the comparator-taking containers only, instantiated with it; others are unaffected.
3. Priority queues — done: `handles` on a `by_priority` queue (`repr::AddressableHeap`, an indexed binary heap with
   `update(h, e)` and `erase(h)`; no new tag), and `monotone_priority` (a requested-only tag; `repr::BucketQueue`
   for integral priorities, chosen only when the request states it). A queue container now serves only queue
   requests.
4. Rebuilding a frozen Store in place — done: `assign(first, last)` on every Store, through the representation's
   `assign` where it has one, else a fresh build. Compressed sparse rows rebuild in their arrays (count, offsets,
   place, shift back; no allocation for a rebuild of the same size, checked by counting allocations), and frozen rows
   by clear and append.
5. Nested fixed shapes and reset — done: `fixed_shape` rows of `fixed_shape` rows, at any depth (generalized after
   M10 from two levels), built from nested vectors of lengths; a row of rows is a range of the rows below, all in one
   array.
   `reset()` on fixed-shape Stores; with `proportional_clear`, a `TrackRows` policy on `Jagged` resets only the rows
   handed out to change since the last reset (writes through a row are not seen; handing it out counts). Deeper
   nesting and a uniform stride are left out.
6. Borrowed rows — done: `view(h)` on pools that keep rows in full (`RowInterner`): a span of the row's elements,
   or for a dense set its `BitsetSpan`; dense-set rows are now stored as their bitset words (trailing zero words
   dropped, so equal sets stay equal). A view stays valid until the pool is cleared. Tree pools keep `get` and `read`.
7. Sorted maps that keep erased values' storage — done: a map's values, where they are Stores, are now checked as
   rows are (`element_traits` describe them), and `repr::Recycling<Map>` (std::map or std::unordered_map with a free list of erased nodes,
   their values cleared) serves sorted maps whose values `retain`: buckets emptied and refilled allocate nothing.

M10 is done. A pass after it removed what was tailored to one use: fixed shapes nest at any depth, value recycling
works over any node-based map (hashed too), and the bucket queue's reliance on its promise is a general trait
(`relies_on<R>`: the request-only promises a representation needs) rather than a flag of its own. Open gaps found by
probing nearby requests, left until a site needs them: a dense concurrent set (the second survey pass found three
sites, docs/SURVEY.md gap 5), an insertion-ordered concurrent map, a concurrent map whose values retain.

## M11: maps ordered by their values

Identity and order can differ: `order_by<P>` with `P(key, value)` orders a map's entries by their values while keys
identify them (one-argument projections stay as they were). A value then moves its entry, so the Store changes values
only through `update`, `merge` and `insert_or_assign` (no mutable `at` or `operator[]`, no bounds by key);
`orders_by_value<R>` marks such representations, and the interface asks them for `update` instead. `repr::FlatEntryMap`
serves it (one sorted vector, found by a scan; for few entries, as `small` says). Covers tyr's
`NumericIntervalAnnotations`. Left out until a large site needs it: a hash index beside an ordered one
(Boost.MultiIndex) for many entries.

## Before M12: four prerequisites — done

Fixed before any library tries urd, since a migration would hit each in its first hour (the weaknesses discussed
on 2026-10-29):

- D. Dependencies: cista linked and exported (built without `CISTA_FMT`), gtl optional (`URD_WITH_GTL`), urd
  installable (`find_package(urd)`, a consumer test against the build tree), Boost 1.89 the minimum. Done.
- B.1. An unservable request names its culprit tags (`no_representation_for<…, culprits<…>>`). Done.
- A. A generated resolution table (docs/RESOLUTION.md, checked by a test) and a registry audit that reports a
  chosen container giving more costly unasked guarantees than another serving one. Done.
- C. `Hash` over Stores whose contents fix their order (sorted, canonical, sequences): a set of sets compiles. Done.
- B.2. A missing operation names the property that removed it (denial facets). Done.

## M12: the repository pattern as generic Stores — done

The symbol repositories of yggdrasil, tyr and loki as requests (the plan of 2026-10-29): a strong `index<T>`, a
`flat` pool of records serialized into a byte arena, a `view<T, C>` of an element by index and context, `layered`
on pools, a `Family` with a parent, and a dense map of layered pools for relation bindings.

1. `urd::index<T>` and `urd::view<T, C>` over a single Store: handles are indices, a positional set mints them and
   resolves views, members resolve through the context. Done.
2. Flat record pools: the in-memory interner for trivially copyable records, the cista byte arena for the rest,
   both thread-safe on request; the `flat` tag; the `records` facet. Done.
3. `layered` on pools: indices continue after the parent, lookups fall through, views borrow the owning layer. Done.
4. `compose::Family` with a parent and a layer number, resolving every member type. Done.
5. Relations: a dense layered map of layered row pools, layered through both levels. Done.

Deferred, named here so they are not re-invented: layered positional sets, lock-free parent lookups, rows of one
stride and bit-packed widths, a concurrent dense map without generations, erase on interners.

## M13–M15: the audit's order

An audit of revision `814ba83` (docs/codex.summary, 2026-10-29) found two contract failures and asked that the
property space be deepened rather than widened: every meaningful combination becomes a tested implementation, a
missing implementation recorded as such, or an incompatible contract rejected with a diagnostic. Its suggested
order, each item a milestone or half of one:

1. Repair the two contract failures and pin their behavior.
2. Define reference stability and concurrency precisely for each exposed access path, then complete their useful
   cross-family combinations.
3. Separate stable storage, synchronization and generation policies where they are currently coupled.
4. Complete retention/reclamation and clear-cost combinations, with stronger checks for `constant_clear`.
5. Expand `change_log` and `scoped` support, then the remaining layering, dense multimap, inverse-access and
   projection combinations.

**M13 (item 1 and the classification) — done.** `dense` is the one guarantee that is also a demand on the caller:
a representation declaring it serves only a request promising it (`demands_met`), checked by an absolute rule in the
registry test over the whole grid; the concurrent sorted and retaining maps by an integer key no longer fall
through to the generation map. `nonempty` holds through the fixed-shape constructors and is rejected on queues and
pools (a promise about iterated elements). Every rule of `to_spec` and every registry-level rejection has a
compile-fail case checking its message, listed in docs/properties.rst, "Incompatible combinations"; every `none`
row of docs/RESOLUTION.md carries a note saying whether it is missing or incompatible, which the table tool
requires.

**M14 (items 2 and 3).** Stability and concurrency defined per access path and checked through it (a map's value by
`at`, a queue's `front`, a pool's `[index]`); a `Stable` policy split from `ThreadSafe` on `IndexedSet`,
`RecordInterner` and `IndexMap`, so `stable_refs` alone buys no lock; `Fifo` declares `stable_refs`; stable
concurrent hashed tables through gtl's node tables; the rejections of `sorted, concurrent` (dense excepted), queues
with `concurrent`, positional `stable_refs` without `monotone`, and `frozen, concurrent`; an operation-pair matrix
under ThreadSanitizer.

**M15 (items 4 and 5).** `constant_clear` checked against live cardinality; the rejections and recorded gaps of
retention, clear cost, dense order and by-value maps; `RecyclingSet` (stable retaining sets), `RingFifo` (`fifo,
retains`); `ChangeLogged` over dense, tree and node sets; `Scoped` generic over its map with tree and dense
registrations; layered node sets and maps and the layered generation map; `DenseGroups`, the growable dense
multimap.

## After M11: the second pass over the libraries

A second reading of the four libraries on their newest branches (docs/SURVEY.md, "Second pass", 2026-10-29) verified
the tyr audit, found every class in loki and runir expressible or domain logic but for the gaps below, and ranked what
is left by the sites it unlocks. Candidates for the next milestones, in that order; each is small:

1. A Store as a hashed key (`urd::Hash` over a Store; the README already promises it). Gap 1. Done (prerequisite C).
2. The layered interner, M6's deferred item: loki's 40 repositories, yggdrasil's 22. Gap 2. Done (M12).
3. `erase(h)` on `RowInterner` reporting the moved handle, `replace` on multimaps: yggdrasil's row sets and the postings
   kept in sync over them. Gap 3.
4. `ChangeLogged` over `DenseSet` and over fixed-shape bit rows: runir's incremental evaluators, tyr's delta fact sets.
   Gap 4.
5. `dense, stable_refs` and `dense, concurrent` without generations: search nodes in three libraries, the novelty table,
   the concurrent slot directory. Gap 5.
6. A growable dense multimap (M3's deferral) and the dynamic graph over it. Gap 6.

Then decrease-key by key, insertion order over two-level dense keys, edge-indexed `bidirectional`, the uniform stride,
ties by arrival, innermost-scope membership (gaps 7 to 12), as sites ask.

## Order and size

| Milestone | Coverage | Size |
|---|---|---|
| M0 maps, flat hash tables, benchmarks, CI | 620+ map declarations, faster defaults for all sets | large |
| M1 dense ID maps and sets | 224 dense-keyed tables, markings, most clears | large |
| M2 interners, insertion order | repositories in all four libraries | medium |
| M3 CSR multimaps, graphs | runir graphs, tyr indexes | medium (reuses `Jagged`) |
| M4 postings | joins, invariants | small after M0 |
| M5 priority queues | every search | medium |
| M6 scoped, layered | parsers, translation storage | medium |
| M7 the rest | long tail | per item |

## Verification (per milestone)

- `ctest` green on GCC and Clang, Debug and Release (`-j8`, one build at a time); from M0 on, also in CI
  with ThreadSanitizer.
- Every new property has a typed test, and a sabotage it catches.
- Every pattern has survey pins in `tests/survey.cpp`.
- From M0, benchmarks back each change to a preference order.
- The examples run.
