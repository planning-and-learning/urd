# Plan: a search and resolve tool for urd requests

## Context

An agent writing a `Store<…>` request may guess tags or combinations that do not exist. urd rejects every bad guess
at compile time:
- an unknown tag does not compile;
- an incompatible combination hits a `to_spec` rule with a message;
- an unservable one names its culprits (`no_representation_for<…, culprits<…>>`).

The cost is the loop: guess, compile, read, guess again. The goal is a tool that answers before code is written:
- which requests exist for a need;
- what a given request resolves to;
- for a rejected request, the nearest requests that do resolve.

It must never invent anything. It only looks up what the registry serves and compiles what it is asked to check.

The same engine is offered three ways:
- a local CLI, `urd-search`;
- an MCP server for Claude Code and Codex;
- later, a hosted endpoint. It needs only requests, never customer code.

## Sources of truth (generated, checked, never written by hand)

1. **The resolution index.** Extend the existing `resolution_table` tool, which already generates docs/RESOLUTION.md
   from the grid in `tests/grid.hpp` and is checked by the `resolution` ctest, with a `--json` output:
   `share/urd/resolution.json`, checked the same way. One entry per grid request:
   - `request`: the tags and type parameters as written, and as a normalized tag set;
   - `spec`: the implied tags;
   - `status`: served, missing (with its note), or incompatible (with its rule's message);
   - `representation`: for served requests;
   - `members`: what the Store offers, read from which facets are on. For example `insert`, `erase`, `lower_bound`,
     `values(k)`, `update`, `merge`, `push`/`pop`/`top`, `view(h)`, `reset()`. Generate these from the facet gates
     (one compile per request in the generator), not by hand;
   - `costs`: the rows from the README cost tables, kept in one table the generator reads (cost classes as
     guarantees are deferred: docs/ROADMAP.md, M17);
   - `header`: the family header to include (docs/RESOLUTION.md names it since M16).
2. **The vocabulary.** `share/urd/vocabulary.json`, landed (M17; `vocabulary_table`, the `vocabulary` test), one
   entry per tag:
   - category;
   - role: guarantee, demand (a promise about the data), kind, request or hint;
   - what it guarantees or asks;
   - plain-language phrases;
   - what it implies and what it excludes (every other tag of its category).
3. **The compiler,** for requests outside the grid: a syntax-only compile of a two-line file, the way the
   compile-fail tests already run.

## Commands

All of them answer as JSON for agents, or as text for people.

- **`search <need>`.** The need is tags (`dense constant_clear`), plain words (`ids, reset every round`), or both.
  1. Words become tags through the vocabulary's phrases: deterministic keyword and phrase matching, no model in the
     loop.
  2. Return the served requests whose specs contain those tags, ranked by fewest extra tags, then by the
     registry's order.
  3. Each result shows the request as it must be written, the representation, members, costs and header.
- **`resolve <request>`.**
  1. Look the request up in the index. If it is missing there, compile it.
  2. Return the representation, members and costs, or the rejection message and its culprits.
  3. For a rejection or a missing request, also return the nearest requests that resolve (next command).
- **`neighbours <request>`.** Served requests within a small edit distance in tag space: one tag dropped, added, or
  swapped within its category (`sorted` → `insertion_ordered`, `constant_clear` → `proportional_clear`).
  - Rank by fewest edits, then by keeping the stated tags.
  - Explain each edit in one line from the vocabulary ("drop `concurrent`: no sorted container is thread-safe;
    `dense` keys are").
- **`explain <tag | request>`.**
  - For a tag: what it guarantees, what it implies, and what it conflicts with.
  - For a request: its spec, what each implied tag means, and which tag removed which member (the "denial facets"
    from B.2 already name these in compile errors).
- **`members <request>`.** The operations a request offers. For an agent planning code, this answers "will I have
  `lower_bound`?" before writing it.

## MCP server

- **Tools:** `urd_search`, `urd_resolve`, `urd_neighbours`, `urd_explain`, `urd_members`, wrapping the CLI.
- **Transports:** stdio for local use in Claude Code and Codex; HTTP for the hosted variant.
- **No arbitrary code is compiled from a request.** The compiler path builds the file from a fixed template plus the
  parsed tag list, and rejects anything that is not a known tag or a plain type name. This matters most when hosted.

## Agent integration

- The skill's workflow: describe the need, call `urd_search`, pick a request, call `urd_resolve` on the exact text
  written, then write code using only the `members` returned.
- The skill text and its tables are generated from the same JSON files, so the skill and the tool cannot disagree.
- A hook compiles the changed file syntax-only after an edit and passes urd's message, plus `neighbours` for a
  rejected request, back to the agent.

## Tests

- **Generated files:** `resolution.json` and `vocabulary.json` are checked against the code (`--check`), like
  RESOLUTION.md.
- **Search:** a table of needs and expected top results, for example:
  - "ids, reset every round" → `dense, constant_clear`;
  - "sorted, from threads" → the rejection for `sorted, concurrent` with the neighbours `dense, concurrent` and
    `concurrent` (unsorted);
  - "append-only interner shared by threads" → `positional, monotone, concurrent`.
- **Resolve:** every compile-fail case in tests/compile_fail_store.cpp, run through `resolve`, returns its rejection
  with the same culprits. Every served grid row resolves to its RESOLUTION.md representation.
- **Neighbours:** for each missing or incompatible grid row, at least one neighbour is served and differs in one or
  two edits. Where none exists, the tool says so instead of guessing.
- **No invention:** every request any command returns compiles. A test feeds all returned requests through the
  compiler.

## Steps (one commit each, gate green)

1. `resolution_table --json`: the index with requests, specs, status, representation and members. Add its check.
2. The `urd-search` CLI with `resolve`, `members` and `explain` over the index, and compile fallback with the safe
   template.
3. `neighbours` with edit distances and explanations; its tests over the missing and incompatible rows.
4. `search`, by tags and by the phrases of `vocabulary.json`.
5. The MCP server over the CLI; a short guide for Claude Code and Codex configuration.
6. The skill: workflow text plus generated tables. A hook example (syntax-only compile of the changed file, with
   `neighbours` on rejection).
7. Later, a hosted HTTP variant: the same server, with rate limits and request-only logging.

## Notes

- The index covers the grid. Requests outside it go through the compiler, so the grid's breadth decides how much is
  answered instantly. Grow the grid with each new property, as M13 already requires.
- Write the CLI in whatever the repository already uses for tools (`resolution_table` is C++). Keep it free of a
  model; any language-model help belongs to the agent calling the tool, not to the tool.
