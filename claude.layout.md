# Plan: compile cost constant in the number of containers

## Context

urd will hold many more containers. A file that holds a Store must cost the compiler the same time and memory however
many containers the library has: constant in their number, not linear. This is a design requirement, met by
construction. A test then guards it, so a change that makes it linear fails.

**Today, three costs grow with the number of containers N, in every file that holds a Store:**

| Cost | Why it grows | Where |
|---|---|---|
| Resolving a request | `choose<S>` walks the candidates of the request's kind as types. For each it instantiates `serves<R, S>`, `container_traits<R>` with its constraints (`natural_hash`, `natural_order`, `slot_value`), `row_traits` and `relies_on`. `candidates_for` builds every candidate type with the element types (`instantiated`). | `include/urd/registry/select.hpp`, `preferences.hpp` |
| Declarations | `store.hpp` parses every container's declaration header: class, traits, costs, synopsis. | `include/urd/repr/*_fwd.hpp`, `include/urd/registry/aliases.hpp` |
| Standard headers | `std::set`, `std::map`, `std::unordered_*` and `std::vector` are candidates, and `std` can't be declared into, so `<set>`, `<map>`, `<unordered_set>`, `<unordered_map>` and `<vector>` are always parsed. | `repr/std_set.hpp`, `std_map.hpp`, `sequence.hpp` |

Definitions are per family today: a file includes `urd/use/<family>.hpp` for each family its Stores come from, and a
Store whose family is missing names that header in its error. A family is a hand-made group of containers (hash,
nodes, contiguous, dense, …): including one defines all of its containers, and a family grows with the library.

**Stays linear on purpose:** what walks every container by design (the registry tests, the grid generators, the
per-container tests and benchmarks), and one precompiled table per build (part 2).

## The invariant

`urd/store.hpp` contains the Store machinery, the vocabulary (tags, property sets) and the registry table, whose rows
are values. It contains no container: no definition, no class declaration, and no standard container header
(`<set>`, `<map>`, `<unordered_*>`, `<vector>`). A container enters a file only through its own header, which that
file includes because one of its Stores resolves to it. So what `store.hpp` costs is independent of how many
containers exist, but for one row of values each, which the precompiled table (part 2) removes from the per-file
cost too.

**The message.** Where a file holds a Store whose chosen container's header it has not included, compilation stops
with one error that names the request, the container chosen for it, and exactly the headers missing: the
container's own, and for a wrapper each part's (`urd::parts`), the part first, as its error must come first. Only
the missing ones are listed, not those already included. It is a `static_assert` in the Store's check
(`complete_representation`), reading the paths from the rows, so it comes before any error about an incomplete type.
For example:

    urd::Store: Store<key<int>, dense, change_log> resolves to change_logged_dense_set; include
    "urd/repr/dense_set.hpp", "urd/repr/change_logged.hpp"

## The design

### 1. Selection by value: a lookup, not a walk

- **A row per container,** declared with the container. Its properties are not a constant but a `constexpr`
  function from the request's element features to the container's properties, or to "does not serve these
  elements". It is one ordinary function per container, not a template, so calling every row is evaluation, not
  instantiation. The row records:
  - its id, kind and preference rank;
  - its properties for given element features (today `container_traits`, with the constraints of its partial
    specialization: what it requires of its elements);
  - its demands (`dense`);
  - for row containers, what it gives its rows (`element_traits`).

  These are the facts `serves<R, S>` reads today, written as data instead of as constraints on partial
  specializations.
- **The table:** the rows, grouped by kind, each group in preference order (`include/urd/registry/table.hpp`). The
  order of `preferences.hpp` becomes this order; adding a container adds its row at its place.
- **Element features,** computed once per request, for its key type and its value type: hashable and orderable by
  urd's semantics, indexable, trivially copyable, a slot value, serializable by cista, an integral priority; the
  comparator of `order_by` (orders entries, or projects elements); and for rows, the row request's property set and
  its elements' features. The survey of the 54 `container_traits` specializations found no other input.
- **Wrappers** (layered, change-logged, scoped, recycling, ordered maps, rows) compute their properties from their
  part's: a wrapper's row function calls its part's.
- **Choosing:**
  1. A request computes its spec once (as today), plus its element features.
  2. A `constexpr` function scans its kind's group for the first row that serves. That is a loop over values, with no
     type instantiated. It returns the chosen id.
  3. Only the chosen container's type is named, by id: `representation_of<id, K, V, P>`, specialized in the
     container's own header. `store.hpp` declares only the primary template, so it holds no container type. Where
     the header is not included, the specialization is missing, and the Store's check fails with the header's path,
     which the row carries.

  So template instantiations per request are constant. The `constexpr` loop is linear in the kind's group, but cheap
  per step. Indexing the group by its most selective category (order, then keys) bounds the loop by the containers
  that could serve at all, which stays small as the library grows within a kind.
- **The small/frozen reorder** (stable storage last) becomes a second key in the scan, not a reordered type list.
- **Diagnostics:** `culprits` (which tags to drop) becomes the same scan with one tag removed per try, as values. The
  messages stay what `tests/compile_fail_store.cpp` pins.
- **Interface:** `conforming<R>` is already checked per container by the tests (`tests/container.cpp`), not per
  request. That stays.

### 2. The table built once per build

- `include/urd/registry/table.hpp` holds only the rows, which are values (no class templates), and the ids.
- It is precompiled once per build: `target_precompile_headers` in urd's own targets, and documented for downstream
  projects (one line of CMake). Where the compiler supports it, a C++20 module (`urd.registry`) can follow. A file
  then loads the table instead of parsing it, and Clang's precompiled headers and modules load lazily, so a file
  pays for the rows it reads.
- Without a precompiled header the table is still only N small rows of values: linear, but about a line per
  container. The type-level cost, the expensive part, is constant either way.

### 3. Nothing defined that is not used: no families

A file includes exactly the containers its Stores resolve to, and nothing grouped: no family headers, no headers by
kind. The family as a concept goes.

- **The standard containers through urd's own declarations:** `repr::StdSet<T, C>`, `repr::StdMap<K, V, C>`,
  `repr::StdUnorderedSet<…>` and the rest, declared by urd and each defined in its own header as a thin wrapper over
  the standard ones (the vector stays a vector inside). `store.hpp` then includes no container header at all.
- **One header per container** (done: `repr/dense_set.hpp`, `repr/queue.hpp`, …), which includes the dependency it
  needs (cista, gtl, Boost) and nothing else.
- **`defined_in<R>` names the container's own header,** and the Store's check (`complete_representation`) names it,
  with its parts' headers for a wrapper (`urd::parts`), in the error: the user adds exactly what is missing.
- **A registry change can need one include in user code:** where a request moves to another container, the error
  names the new header. That is the price of paying only for what is used, and it is accepted.
- **What goes with families:**
  - the headers `urd/use/<family>.hpp` and their markers;
  - `requests.json`'s `families` (each container's header comes from its row; the per-container tests and
    benchmarks include it, and its parts');
  - `RESOLUTION.md`'s Include column, which lists each request's container headers instead;
  - the `compile_fail_include` cases, one per family, which become one case per kind of miss (a container, a
    wrapper's part);
  - the family vocabulary in the docs (`docs/INTEGRATION.md`, "Register it"; `RESOLUTION.md`'s preamble).
- **Declarations go from `store.hpp` entirely.** The `*_fwd.hpp` headers become part of each container's own header:
  its class, `container_traits`, `container_costs`, `container_synopsis` and `representation_of`. The registry table
  holds only rows. What the generators and tests need of every container (traits, costs, synopsis) they get by
  including every container's header, as they do today, which is linear on purpose.

## Steps (one commit each; pins, grid and gate green)

0. **Commit step 5 of the current plan** (one header per container in `repr/`), after verifying it: build,
   ctest, lint.
1. **The guard first.** `tests/scaling/`:
   - registers k copies of every container under new names (k = 1 and 4), generated, with no definitions;
   - compiles a file with one Store and one with ten;
   - fails when the instantiation count (`-ftime-trace`'s template events on Clang; on GCC, `-fdump-…` or peak
     memory) differs between k = 1 and k = 4 beyond a small margin.

   It fails today, which shows what it measures; it is marked expected-to-fail until step 4.
2. **The rows.** Each container's declaration gains its row, built from what its traits and constraints say today.
   A test checks, for every grid request, that the row-based choice equals today's `choose<S>`, so the two
   selections agree before one replaces the other.
3. **Element features.** One `constexpr` function computing the flags for a type, and the rows' requirements in
   those flags. The agreement test of step 2 covers it.
4. **Selection by value.**
   - `choose<S>` becomes the scan plus `representation_of<id, …>`;
   - `candidates_for`, `instantiated` and the type-level `serves` go;
   - the diagnostics become value scans.

   `RESOLUTION.md` must be unchanged, and the compile-fail messages must still match. The scaling test passes from
   here.
5. **Families go; the standard containers behind urd's declarations.**
   - `defined_in` per container;
   - the Store's check naming container headers;
   - the family headers removed;
   - generated files, per-container sources and docs moved to container headers;
   - the standard containers declared by urd, so `store.hpp` includes no container header.

   The scaling test also checks that a one-Store file includes only that container's header (and `<set>`, `<map>`
   and the rest only where it holds such a Store).

   Then the README gains its bullet, written from the compiler's real output: "Pay only for what you use", the
   compile cost of a Store being that of the container it resolves to, nothing for the others and no more as urd
   gains containers, and the error naming the headers a file misses, pasted as the compiler prints it.
6. **The precompiled table:** `table.hpp` precompiled in urd's targets; the CMake line for downstream projects in
   `docs/INTEGRATION.md`; a module where supported, behind an option.

## Verification

- **The scaling test** (step 1) passes from step 4 on, on GCC and Clang, Debug and Release.
- **`resolution_table --check`, the requests and the per-container tests** are unchanged at every step: the same
  requests resolve to the same containers.
- **The compile-fail cases** (`tests/CMakeLists.txt`, `urd_compile_fail_expects`) pass with the same messages, except
  the missing-include ones (`compile_fail_include`), which change to the new message. They pin:
  - a plain container whose header is missing: its header named;
  - a wrapper missing only its part's header: only the part's named;
  - a wrapper missing both: both, the part first;
  - a file that holds two Stores of different containers, one included: only the other named.
- **The budgets** (`tests/budget.py`): `bare` and `survey` drop, and their limits are lowered to the new numbers so
  they guard them.
- **Sabotages, reverted:**
  - a row that disagrees with its container's traits: the agreement test fails;
  - a container whose row requires nothing it needs: its per-container tests fail to compile;
  - a selection that instantiates every candidate again: the scaling test fails.

## Open

- **Modules:** GCC 13's support is not enough; the precompiled header is the default.
- **Directory layout of `repr/`:** with no families, the headers can stay flat, one per container, or go in
  directories by kind, with the two containers that serve two kinds (`layered`, `layered_pool`) placed by a rule.
  Kind matches how a user reads a request; flat needs no rule.
