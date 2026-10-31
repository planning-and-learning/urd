# Plan: four groups of tags — kind, semantics, costs, hints

To execute after the benchmarks branch (complexity as data, the registry's claims, benchmark_comparisons) is merged.

## Context

The vocabulary is grouped by six themes (docs/language/, `theme_vocabulary` in tests/names.hpp), and each tag has a
role written by hand (guarantee, demand, kind, request, hint). Themes group by topic, and every finer partition we
tried (seven levels from the literature, effects computed per tag) either forced tags into one box they do not fit
or was well defined without being meaningful.

## The test

**What does leaving out a tag you need cause?**

| Group | Omitting a needed tag | Categories (tags) |
|---|---|---|
| **Kind** | it does not compile: the operation does not exist | `key<K>`, `value<V>` (set, map, sequence); access (`fifo`, `lifo`, `by_priority`); handle (`handles`); grouping (`partition`) |
| **Semantics** | a bug | order (`sorted`, `insertion_ordered`, `order_by<P>`); positions (`positional`); uniqueness (`unique`); multiplicity (`multi`); change (`may_change` → `monotone` → `frozen`); emptiness (`nonempty`); threads (`concurrent`); scopes (`scoped`); layers (`layered`); tracking (`change_log`); direction (`bidirectional`); shape (`fixed_shape`); keys (`dense`); priorities (`monotone_priority`); references (`stable_refs`); layout (`contiguous`); representation (`canonical`); packing (`flat`) |
| **Costs** | slower by a complexity class | clear (`proportional_clear` → `constant_clear`); memory (`retains`, `reclaims`); sharing (`structural_sharing`) |
| **Hints** | slower by a constant factor; never changes a guarantee | workload (`small`) |

The test is about omission. Asking for a tag you do not need only costs speed, whatever its group.

Semantics holds two kinds of tag, and each is marked:
- **guaranteed**: the container keeps it (`sorted`, `stable_refs`);
- **promised by the caller**: the caller keeps it about its data (`dense`, `monotone_priority`). A false promise is
  a bug, so these are semantics too.

Kind is strictly part of semantics, since it decides which operations exist. It stays separate because it is the
first decision and every other changes with it.

**Within semantics**, the docs list the categories under four reading headings. The headings carry no rules and
only order the page:
- what you promise about your data: keys, priorities;
- what holds: order, positions, uniqueness, multiplicity, change, emptiness, threads, references, representation;
- what more it can do: scopes, layers, tracking, direction, shape;
- how it hands out memory: layout, packing.

## A tag earns its place

**A public tag changes some resolution or rejects some request.** One that does neither is internal: containers
still declare it, the library still derives it, and its error messages may still name it, but the vocabulary does
not offer it. This rule decides the changes below, and every future tag.

## The changes

1. **One lifecycle ladder.** `may_change` → `monotone` → `frozen` stays public. `immutable`, `growable` and
   `shrinkable` become internal; they are already derived (include/urd/properties.hpp, from `frozen`, `fixed_shape`,
   `monotone` and `nonempty`). `nonempty` and `fixed_shape` stay their own categories.
2. **`functional` internal.** It is derived from the kind (a map without `multi`, any sequence); users state only
   `multi`. The "two properties answer the same question" assert drops the pair.
3. **`sparse` internal**, after a check: every grid request resolves the same with and without it. Expected,
   because dense containers serve only requests that state `dense` (`demands_met`, include/urd/traits.hpp). If any
   resolution changes, `sparse` stays public and the difference is written down.
4. **`monotone_priority` implies `by_priority`** instead of asserting it, so the caller states one tag. It stays in
   semantics with its own root: it is the caller's promise, and access is kind, so folding it into access would
   make that category span two groups.
5. **Cost tags for erase, lookup and iteration only when one changes a resolution** (claude.properties.md gives the
   reasons none does yet). `constant_erase` is the likely first, with an O(1)-erase, position-keeping set (the
   tombstoned set deferred in M14). When the first lands, declared costs move into include/urd/ as container
   traits, like `constant_clear` today, and tests/complexity.hpp verifies them.
6. **Docs and vocabulary.json grouped by the four groups.**

## What is computed, what is checked

- **The group is written by hand, once per category**, each with a sentence saying why under the test.
- **Checks from the code:**
  - **kind:** with and without the tag, a grid request's spec differs in `is_queue`, `is_pool` or `is_partition`;
  - **hints:** with and without the tag, the facets and the guarantees are the same; only the chosen representation
    may differ (`ordered_for`);
  - **costs:** with and without the tag, the facets are the same and the declared guarantees other than the cost
    are the same;
  - **public tags:** each changes some grid resolution or is rejected somewhere (a compile-fail case);
  - **semantics:** everything else, with no check beyond the typed tests that already verify each guarantee.
- **The role is derived:**
  - kind → kind;
  - promised by the caller → demand;
  - hints → hint;
  - every other category → guarantee where its root is guaranteed (`guaranteed_roots`), request where it is
    request-only.

  The role column of `tag_entry` goes; `vocabulary_table` derives it and keeps the consistency check of
  `check_role`.

## Steps (one commit each; pins, grid and gate green: GCC and Clang, Debug and Release, ctest, peachycheck lint, sphinx -W)

1. **The `sparse` check.** A test resolving every grid request with and without `sparse`; it decides change 3.
2. **The internal tags** (changes 1–3). The tags leave `tag_vocabulary` and the docs; uses in tests and examples are
   rewritten. A compile-fail case states an internal tag and checks the message points to the public one.
3. **`monotone_priority` implies `by_priority`** (change 4). The static_assert goes; a pin checks that the request
   with one tag resolves like the one with both.
4. **The vocabulary.**
   - In tests/names.hpp, `theme_vocabulary` becomes `group_vocabulary`: kind, semantics, costs and hints, each with a
     title, the test as a sentence, and its literature.
   - `category_vocabulary` names a group instead of a theme, with its question and a reading heading (semantics
     only).
   - `tag_entry` loses its role and gains, for semantics, guaranteed or promised by the caller.
   - The `static_assert`s carry over: an entry per root; a known group; a heading exactly for semantics.
5. **The checks.** `tests/groups.cpp` runs the kind, hint, cost and public-tag checks over the grid's pairs (each
   request with the tag, against the same request without it, where that one is well formed). Each pair checked is
   named in a failure.
6. **vocabulary.json.**
   - Each tag and category gets `"group"`, semantics tags `"promise"` (guaranteed or caller), and semantics
     categories `"heading"`.
   - `"theme"` and `"themes"` go. `"role"` stays, derived, for urd-search and urd-miner; tell their owners.
7. **The docs.**
   - docs/language/ becomes four pages (kind, semantics, costs, hints). Semantics is sectioned by its headings. The
     `urd-theme` directive becomes `urd-group`; the category sections and tag cards are unchanged.
   - The overview states the test, works `Distance` through it, and gives one sentence of literature per group.
   - The costs page links the complexity tables and BENCHMARKS.md.
8. **The README.** The theme table becomes the four groups with the example's tags:
   - kind: `fifo`;
   - semantics: `positional`, `multi`, `frozen`, `monotone`, `bidirectional`, `dense`;
   - costs: `constant_clear`;
   - hints: none.

## Verification

- The gate on each commit. `vocabulary_table --check`, `resolution_table --check` (RESOLUTION.md unchanged except
  where a step says so) and the docs build pass.
- **Sabotages, run and reverted, not committed:**
  - a category without a group: the `static_assert` fails;
  - `small` grouped as semantics with a facet switched on by it: the hint check fails;
  - `retains` given a facet of its own: the cost check fails;
  - a public tag that changes no resolution: the public-tag check fails;
  - a category directive on another group's page: the docs build fails.
- urd-search's tests pass against the new vocabulary.json.
- **Meaning, measured, before the docs switch:** urd-search's score and urd-miner's mapping accuracy on its 48
  reviewed yggdrasil structures, with the agent shown the vocabulary grouped by theme against grouped by these four.
  The grouping that leads to more correct requests wins; a loss is reported, not hidden.

## Open

- **The name of the third group:** "costs" (chosen: it says these are guarantees, unlike hints) or "optimization".
