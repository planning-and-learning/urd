# Plan: keep compile time and memory flat as containers are added

## Context

`urd/store.hpp` includes `urd/registry.hpp`, which includes every representation header (`repr/dense.hpp`,
`rows.hpp`, `pools.hpp`, `queues.hpp`, the tree pools, `records.hpp`, `arena.hpp`, …; `parallel.hpp` only with
`URD_HAS_GTL`). So every file that uses one Store parses every container and its dependencies (Boost.Unordered,
Boost.Container, gtl's phmap, …). That cost grows with each container added and is paid by every file: the part of
urd's compile cost that will explode.

Goal: including `urd/store.hpp` costs the vocabulary, selection and traits only, roughly constant as containers grow.
A file pays for the definitions of the container families it uses, and a missing one is a compile error that names
the header to include.

Constraints (as for all urd work):
- On `main`, one commit per step.
- Commit only behind the gate: the four builds (g++ and clang++, Debug and Release, `-j8`), ctest, and
  `PEACHYCHECK_JOBS=8 peachycheck lint --all` reporting "no findings". Also the TSan build for steps that touch
  concurrent containers.
- No NOLINT and no escape hatches.
- Downstream code never names `urd::repr`.

## Design

### 1. Selection reads declarations and traits only

`container_traits` are partial specializations matched on template arguments, so they compile against a forward
declaration:

```cpp
// urd/repr/dense_fwd.hpp
namespace urd::repr { template<typename K, bool TrackWords> class DenseSet; }
template<indexable K, bool TrackWords>
struct urd::container_traits<urd::repr::DenseSet<K, TrackWords>> : properties_of<sorted, unique, dense, …> {};
```

- Each `repr/X.hpp` splits into two files:
  - `repr/X_fwd.hpp`: forward declarations; `container_traits`, `element_traits`, `relies_on` and
    `orders_by_value` specializations; aliases used by the registry.
  - `repr/X.hpp`: the definitions, including `X_fwd.hpp`.
- `registry.hpp` includes only `*_fwd.hpp`, plus third-party forward headers:
  - `boost/unordered/unordered_flat_set_fwd.hpp`, `…_flat_map_fwd.hpp`, `…_concurrent_flat_set_fwd.hpp`;
  - `boost/container/container_fwd.hpp`;
  - gtl's `phmap_fwd_decl.hpp`.

  The standard containers cannot be forward-declared legally (namespace std), so `<vector>`, `<set>`, `<map>`,
  `<unordered_set>`, `<unordered_map>` and `<deque>` stay included. They are cheap next to Boost and gtl.

### 2. Nothing in selection may need a complete candidate

These places look inside a container today and must change:

- **`registry.hpp:86`, `serves`:** `(!has<S, positional> || std::ranges::random_access_range<R>)` instantiates
  `R`'s iterators. Replace it with a declared property, for instance a `random_access` trait in
  `container_traits`, or an existing tag that implies it. `interface.hpp`'s `offers_layout` still checks it once per
  container in the conformance tests.
- **`traits.hpp:~690–720`, `held<R>` and the default `element_traits<R>`:** they read `R::value_type` or
  `R::mapped_type`, which needs a complete `R`. Instead, pass the row type from the request:
  `element_traits<R, Row>`, defaulting to `with_t<container_traits<Row>, reclaims>`, where `serves` supplies
  `row_of<S>`. Specializations keep their meaning (Jagged, RowPool, RowInterner, the tree pools, Recycling, …).
  - `recycling.hpp:143` uses `typename Map::mapped_type`. That is fine, since `Map` is a complete std type, but
    switching it to the `Row` parameter makes it uniform.
- **Traits whose constraints read a container's members:** audit every `container_traits` specialization. The current
  ones use template arguments (`natural_order<C, K>`, `natural_hash<H, E, T>`, `hashable<…range_value_t<Row>>`,
  where `Row` is a Store that is complete anyway). Keep it that way. Write down the rule "traits depend on template
  arguments only" in `docs/INTEGRATION.md` and add a check (step 5).
- **`ordered_for`, `node_based<R>` and `queue_traits`:** these read traits only, which is fine. Verify that nothing
  else calls `std::ranges::*` or `sizeof` on a candidate.

Rule: **choosing never instantiates a candidate; only the chosen one is instantiated**, by the Store that holds it.

### 3. The chosen container must be complete where the Store is, with an error naming the header

`detail::Store<R, S>` holds `R m_representation`, so `R` must be complete there. Before the member, add a check
whose failure names the header.

C++20 `static_assert` takes only a literal message, so carry the name in a type, which compilers print:

```cpp
template<urd::detail::fixed_string Header> struct include;   // declared, never defined: its name is the message

template<typename R>
using complete_or_include = std::conditional_t<is_complete_v<R>, R, include<container_traits<R>::header>>;

template<typename R, typename S>
class Store : … {
    static_assert(sizeof(complete_or_include<R>) > 0);
    R m_representation;
};
```

- Each container's traits name its public header: `static constexpr fixed_string header = "urd/use/dense.hpp";`.
- The intended error, with GCC and Clang (check the exact text in step 4):

  ```
  error: invalid use of incomplete type 'urd::include<"urd/use/dense.hpp">'
    required from 'urd::detail::Store<urd::repr::DenseSet<Vertex>, urd::spec<Vertex, void, …, dense, …>>'
  ```
- `is_complete_v<R>` is a `sizeof`-based trait. Its result is cached per file, which is fine where the Store needs
  `R` complete anyway. Do not use it anywhere earlier.

### 4. Public include headers, not `repr` paths

- Downstream code must not reach into `urd/repr`. Add thin public headers, one per family, that include the
  definitions: `urd/use/dense.hpp`, `urd/use/hash.hpp`, `urd/use/trees.hpp`, `urd/use/rows.hpp`,
  `urd/use/pools.hpp`, `urd/use/queues.hpp`, `urd/use/maps.hpp`, `urd/use/parallel.hpp`, …
- Add `urd/use/all.hpp` for those who do not care about compile time.
- Group the families so that the headers a typical request needs are few, and so that Boost and gtl headers are
  only in the families that use them.
- `tests/layout.cmake` keeps the include root vocabulary-only. Add `use` as an allowed directory.
- Examples, tests and the README include what they use. Each example's include list shows what it costs.

### 5. Keep it flat: a CI budget

The rule erodes unless it is measured. Add a compile probe in CI, a job or a ctest with a fixed limit:
- **Bare include:** compile a file that includes only `urd/store.hpp`, recording wall time and peak memory
  (`/usr/bin/time -v`).
- **Per request:** the existing 400-request compile file, recording time and memory per request.
- Set each limit at the post-refactor number plus a margin, written in the test. A change that exceeds a limit
  fails, like a lint finding.
- **Rule check:** a test that compiles `registry.hpp` alone, with only `_fwd` headers, and selects a request of each
  kind without any definitions included. If selection instantiates a candidate, this stops compiling.

### 6. One registry everywhere (ODR)

- Every forward declaration and every traits specialization is always included through `registry.hpp`. Only the
  definitions are optional. So a request resolves to the same type in every file, whatever that file includes.
- Never make the registry depend on what was included: a request resolving differently in two files would be a
  silent ODR violation.

## Steps (one commit each, gate green before each)

1. **Measure the baseline** (in the commit message): bare-include time and memory, and the 400-request file. Add the
   probe from 5 with generous limits.
2. **Make selection instantiation-free** (design 2): replace the `random_access_range` check with a declared trait;
   change `element_traits` to take `Row`; audit the traits. The tests and pins must all be unchanged.
3. **Split one family as the pattern:** `dense` (`DenseSet`, `SparseSet`, `IndexMap`) into `dense_fwd.hpp` and
   `dense.hpp`, add `urd/use/dense.hpp`, add the completeness check and `include<…>` from 3, and update the tests
   and examples that use dense Stores. Record the actual GCC and Clang error text in `docs/INTEGRATION.md`.
4. **Split the remaining families**, a few per commit; the Boost- and gtl-heavy ones first, as they gain the most.
5. **Switch `registry.hpp` to `_fwd` headers only.** Add `urd/use/all.hpp`. Tighten the CI limits to the new numbers
   plus a margin.
6. **Docs:** README "Using it" (the include per family, and `all.hpp`); INTEGRATION ("adding a container": write
   `X_fwd.hpp` with traits that depend on template arguments only, `X.hpp` with the definition, a `urd/use/` entry,
   and `header` in the traits); a ROADMAP entry.

## Verification

- All existing pins (`tests/store.cpp`, `tests/survey.cpp`, `tests/pools.cpp`, `tests/queues.cpp`, …) resolve to the
  same representations, and `all_conform` passes.
- New compile-fail case: a Store whose family header is not included fails, and its output contains
  `include<"urd/use/…">` (use ctest `FAIL_REGULAR_EXPRESSION`/`PASS_REGULAR_EXPRESSION` on the compiler output).
- The rule check from 5 compiles with `_fwd` headers only.
- The bare include and the per-request cost are reported in the final commit message against the step 1 baseline,
  and the CI limits hold.

## Notes

- Do not rely on C++26 user-generated `static_assert` messages. The `include<"…">` type works in C++20 on GCC and
  Clang.
- C++20 modules would make parsing a one-time cost, but toolchain support is uneven. Leave them for later, and do not
  design anything that blocks them.
- Nested Stores (rows) need the inner container's family too. The error appears at the inner Store, which is the
  right place.
