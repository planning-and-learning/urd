# Plan: operation costs and workload hints as properties

## Context

urd's properties are sufficient for correctness and not for performance. A request can state how much a clear may
cost (`proportional_clear`, `constant_clear`), but not:
- that erase must be O(1);
- that iteration must cost O(n) rather than O(key range);
- that lookup must be expected O(1) rather than O(log n).

It also cannot state its workload beyond `small`. So code that relies on a cost cannot say so, and a container that
is right in general but wrong for a site is chosen without a trace. Concrete cases today:
- an insertion-ordered map erases in O(n);
- a dense map iterates in O(U);
- `FlatEntryMap` is O(n) per operation, fine only for few entries.

Goal: a small extension of the vocabulary.
- **Cost classes**, for the operations where real alternatives exist. They are guarantees, declared by containers,
  asked for by requests, and checked like every other guarantee.
- **Workload hints.** They are requested only and never change a guarantee; they only reorder candidates.
- **Plain-language descriptions of every tag**, so that the search tool (claude.search.md) can map a need to tags.

Rules, as for all urd work:
- one commit per step on `main`, behind the gate (the four builds, ctest, the lint with no findings);
- every new guarantee comes with a verifier;
- every combination is served, recorded as missing, or rejected with a message (the grid and docs/RESOLUTION.md).

## 1. Cost classes (guarantees)

Same shape as the clear-cost category: a root that promises nothing, and tags that promise more.

| Category | Tags (later ones promise more) | Meaning | Today |
|---|---|---|---|
| clear (exists) | `proportional_clear` → `constant_clear` | O(elements present), O(1) | unchanged |
| erase | `log_erase` → `constant_erase` | erase one element in O(log n), or expected O(1) | in-order arrays are O(n): `IndexedSet`, `OrderedMap`, ordered `SparseSet`, flat sets and maps |
| iteration | `proportional_iteration` | a full iteration costs O(n + 1), not O(capacity) or O(key range) | `DenseSet` (O(U/64 + n)), `IndexMap` (O(U)), open-addressing tables (O(capacity)) do not declare it |
| lookup | `log_lookup` → `constant_lookup` | find/contains in O(log n), or expected O(1) | hash tables are expected O(1), trees and flat sets O(log n), `FlatEntryMap` and small scans O(n) |

**Declarations.** Each container declares what it keeps. Examples:

| Container | Declares |
|---|---|
| `boost::unordered_flat_set/map` | `constant_erase`, `constant_lookup` (no `proportional_iteration`) |
| `std::unordered_set/map` | `constant_erase`, `constant_lookup`, `proportional_iteration` |
| `std::set/map`, trees | `log_erase`, `log_lookup`, `proportional_iteration` |
| `boost::container::flat_*` | `log_lookup`, `proportional_iteration` (erase O(n): none) |
| `DenseSet` | `constant_erase`, `constant_lookup` (iteration O(U/64)) |
| `SparseSet` (unordered) | `constant_erase`, `constant_lookup`, `proportional_iteration` |
| `IndexMap` | `constant_erase`, `constant_lookup` (iteration O(U)) |
| `IndexedSet` | `constant_lookup`, `proportional_iteration` (erase O(n)) |
| `FlatEntryMap` | `proportional_iteration` |
| `Jagged`, CSR | `proportional_iteration` |

Work out the full table from the code, container by container. Declare a class only where the code keeps it
through every operation, including growth (amortized and expected bounds count, as they do for clear today).

**Missing combinations this will expose.** Record them in RESOLUTION.md as missing, or add a representation where
it is cheap:
- `insertion_ordered, constant_erase`: a tombstoned order with periodic compaction (amortized O(1)). M14 already
  names "a tombstoned insertion-ordered set" as deferred.
- `dense, proportional_iteration`: `DenseSet` with `TrackWords` (iterate the touched words), `SparseSet`, and a
  generation map with a list of live keys.
- `insertion_ordered, value-ordered, constant_lookup`: a hash index beside the order. This is the deferred
  Boost.MultiIndex representation.

## 2. Workload hints (requested only)

They live in the `general` category beside `small` (one per request), or in a second hint category if two must
combine. They never change a guarantee or the set of serving containers, only their order (`ordered_for` in
`registry.hpp`, as `small` and `frozen` reorder today).

| Hint | Effect on the order |
|---|---|
| `small` (exists) | compact containers first |
| `large` | containers with the better asymptotic cost first, even at a constant-factor cost |
| `read_mostly` | containers with the fastest lookup and iteration first; costly writes accepted |
| `iterate_often` | containers declaring `proportional_iteration` and contiguous layout first |

Keep the list short. A hint earns its place only if a benchmark in docs/BENCHMARKS.md shows that it changes the best
choice for some request. Hints without such evidence are not added.

## 3. Verification

**Cost classes are checked deterministically where possible.** Count instead of timing:
- **A counting element type** for the typed suites, with a key whose hash, equality and `<` increment counters, so a
  test counts the work an operation does.
- **One test per declared cost class.** Run the operation on n and on 4n elements and require the counted work to
  grow as the class says:
  - constant: about equal;
  - log: about +2;
  - proportional: about 4×.
- **Iteration over the key range is invisible to comparison counts.** For `proportional_iteration`, measure the
  steps of the iterator, or use the timing harness from tests/store.cpp `Complexity` (best of three, ratio below 8)
  where counting cannot see the cost.
- **Sabotages:** a constant-erase container made to shift its elements must fail; a container declaring
  `proportional_iteration` made to scan its capacity must fail.

**Hints are not guarantees and get no verifier.** Their effect on the order is pinned in docs/RESOLUTION.md (the
`small` rows already show this).

**The grid** (`tests/grid.hpp`) gains the new tags in each kind. Every new row is served, missing (with a note), or
rejected with a compile-fail case for its message, as M13 requires.

## 4. Descriptions for search

Every tag gets one machine-readable entry, generated next to RESOLUTION.md and checked by the same kind of test (a
`--check` against the code):
- its category;
- one line on what it guarantees or asks;
- a few plain-language phrases a need is stated in, such as:
  - `constant_clear`: "reset every round", "cleared per search";
  - `dense`: "ids", "indices", "vertices numbered 0..n-1";
  - `monotone`: "only grows", "append-only";
  - `stable_refs`: "pointers stay valid";
- what it implies, and which tags it conflicts with.

The text lives in the code: a `description` and `phrases` beside each tag in `traits.hpp`, or one table in a single
header. That way the generated file cannot drift. claude.search.md reads it.

## Steps (one commit each)

1. **The erase class:** the tags, declarations, the counting element type, per-class tests, sabotages, grid rows,
   RESOLUTION.md, and docs/properties.rst.
2. **The iteration class:** the same, plus `proportional_iteration` for tracked dense sets and generation maps where
   cheap.
3. **The lookup class:** the same.
4. **Workload hints:** add only those whose benchmark shows a different best choice; `ordered_for` rules; pinned
   rows.
5. **Tag descriptions and phrases:** the generated vocabulary file and its check.
6. **Docs:** README property and cost tables, INTEGRATION ("declare your cost classes"), ROADMAP.

## Verification

- All existing pins unchanged. Requests that state none of the new tags resolve as before; the RESOLUTION.md diff
  shows only new rows.
- Each declared cost class passes its counting or scaling test on every container that declares it, and each
  sabotage fails.
- Every new grid row is served, missing (with a note), or rejected with a message.
