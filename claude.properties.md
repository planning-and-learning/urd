# Plan: operation costs and workload hints as properties — deferred

## Where it stands (2026-10-30)

This plan proposed three cost classes as guarantees (erase, iteration, lookup), workload hints beside `small`, and
a machine-readable description of every tag. The description landed as M17 (`share/urd/vocabulary.json`,
docs/ROADMAP.md). The rest is deferred, for these reasons:

- **Lookup** (`log_lookup` → `constant_lookup`): the order already settles it. Unordered requests get hash tables
  (expected O(1)), `sorted` ones get trees or flat sets (O(log n)). The O(n) cases (`FlatEntryMap`, flat scans) are
  chosen only by `small` or `frozen`, which the request states. `sorted, constant_lookup` would need a hash index
  beside a tree, which no site asks for.
- **Iteration** (`proportional_iteration`): `dense` and `sparse` already answer it. They describe the keys: either
  they fill their range, or they are few in it. Containers that iterate over the key range (`DenseSet`, `IndexMap`)
  are chosen only for `dense` keys, where the range is about n.
- **Erase** (`log_erase` → `constant_erase`): the only class that draws a real line, between positional or
  insertion-ordered storage (O(n) erase that keeps order) and everything else. It has no site of its own. It comes
  with gap 3 of docs/SURVEY.md (swap-erase on interners, `std::optional<handle_type> erase(handle_type)`), where
  `positional, constant_erase` is the first request to need it, or with the tombstoned insertion-ordered set
  deferred in M14.
- **Workload hints** (`large`, `read_mostly`, `iterate_often`): none has a benchmark showing it changes the best
  choice. `iterate_often` restates `contiguous`, and `large` is the default order. A hint is added only with such a
  benchmark in docs/BENCHMARKS.md.

## When the erase class lands

As the clear-cost category does it: a root that promises nothing, `log_erase` → `constant_erase`, declared by each
container only where every operation keeps it (amortized and expected bounds count).

- **Verifier:** count the work instead of timing it. Erase in O(n) shifts elements, so count moves and copies, not
  only hash, equality and `<` calls. Put the test in its own file, not `tests/properties.cpp`, which already takes
  145 s and 2.4 GB on GCC.
- **Sabotage:** a constant-erase container made to shift its elements must fail.
- **The grid:** every new row is served, missing (with a note), or rejected with a compile-fail case (M13).
- **The vocabulary:** an entry in `tag_vocabulary`, then regenerate.
