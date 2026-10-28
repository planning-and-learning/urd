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
every multimap Store; `urd::Graph<Vertex>` with successors and predecessors; survey pins (runir `StaticGraph`, tyr
effect groups); the rows example shows both. Deferred: edge properties on `Graph` (key a Store by an edge's
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

## M5: Priority queues (pattern 8)

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

## M6: Scoped symbol tables and layering (patterns 10, 9, 15)

- **`scoped`:** maps with `push_scope` and `pop_scope`, where lookups find the innermost binding. The
  representation is a vector of `(key, value)` plus a hash map from each key to its stack of bindings. It
  serves loki's parse contexts and runir's parser maps (about 40 sites).
- **`layered`:** a Store that answers from itself, then from a frozen parent it does not own. It serves
  loki's `TranslationStorage` and yggdrasil's parent-layered repositories, and lets the interner (M2) layer
  over a frozen parent.
- **Memo tables (9):** these become maps with `constant_clear` (M1) or `layered`. A two-generation reset is
  a pair of maps swapped each generation: an example, not a new representation.

## M7: Remaining patterns

| Pattern | Approach |
|---|---|
| Two-level dense map (4) | nested Stores: `Store<key<Rel>, dense, value<Store<key<Row>, dense, value<V>, auto_resize>>>` with `constant_clear`; no new representation if M1 and nested Stores compose |
| Bit matrix (14) | a uniform-length nested dense set: rows of equal length need no offsets; a `uniform` property makes it one bitset with a stride |
| Partition (13) | a new kind, union-find, with `merge`, `find` and `same`; `refinable` for colour refinement later |
| Persistent (15) | a `persistent` property (removed until it has a representation and a test): yggdrasil `tree_vector_set` and `tree_bitset_set` (copy), or immer if its license and size fit |
| Delta tracking (16) | a `change_log` property: the Store records exactly what was added and removed since the last reset, for semi-naive evaluation |
| Type-indexed family (18) | a combinator over Stores, one per type in a tag list; not a Store |
| Hive and slot map | `handles` identity (from the earlier discussion), for object pools whose objects need stable addresses and reuse |

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
