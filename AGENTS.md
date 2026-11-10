# urd coding rules

`MUST` and `MUST NOT` are requirements on every change; `MAY` grants permission. Each rule has an id, so a review or
a commit can name it. A rule with a **Check** can be verified mechanically; the command must print nothing. A rule
marked *open* is decided but not yet met by the whole code base: new code MUST meet it, and the count MUST NOT grow.

Terms: a **container** is a class under `include/urd/repr/`. Its **declaration** is its `*_decl` struct (tags it
provides, operations with their costs). A **facet** is a class template `F<Derived, R, S>` under `include/urd/store/`
that gives a Store its members. The **request** is the spec `S` a Store is asked for (kind and tags).

## Facets

- **F1 Forward only.** A facet member MUST forward to one member of the container. It MUST NOT compute a result the
  container does not provide.
- **F2 No fallbacks.** A facet MUST NOT test whether the container has a member and do the work another way when it
  has not (`if constexpr (requires { representation().x(...); }) ... else ...`). A container that lacks the operation
  does not offer it: calling it MUST be a compile-time error.
  What an element may do is a trait specialized for its type (`urd::slot_release`, `urd::memory_of`), not a member
  the facet looks for.
  **Check**: `grep -rn "if constexpr (requires" include/urd/store include/urd/store.hpp`
- **F6 Gate by properties, not by functions.** Whether a Store has a facet or a facet member MUST be
  decided by properties: of the request (`has<S, Tag>`) or of the container's declaration (`has<R, Tag>`). It MUST
  NOT be decided by whether the container type has a member function (`requires requires(R& held) { held.x(); }`).
  A container that declares a property MUST have every function the facets forward to under that property; a
  missing one is a compile-time error. Representation-specific functionality that no property determines yet gets
  a property of its own, stated by the containers that have it.
  A member every container of a kind has needs no gate (a sequence's `resize` and `assign`): it is implemented in
  the containers (F3). A member
  that takes another form of a key (`find(K)`, a set's `insert(prehashed)`) is a plain forward: the Store's hash,
  equality or order decide what a key may be. The others, by property: `reserve` by `reservable`; `capacity`,
  `max_size`, `shrink_to_fit` by `contiguous`; `update`, `visit` by `dense` or `concurrent`.
  **Check**: `grep -rn "requires.*requires(\(const \)\?R&" include/urd/store include/urd/store.hpp`
- **F3 Filling a gap.** Where a container lacks a member that the standard library's containers of that kind have
  (`equal_range`, `merge`, `reserve`, ...) or that a caller needs, the member MUST be implemented in the container,
  not in the facet.
- **F4 Minimal surface.** A facet's members MUST form one capability. A member that is not a standard-library name and
  has no caller MUST be removed; an established standard-library member MAY stay without a caller.
- **F5 One accessor rule.** A facet reaches its container through `representation()` and its Store through `self()`,
  each overloaded on const. No other accessor names.
  **Check**: `grep -rn "constant()\|mutable_representation\|mutable_self" include/urd/store`

## Interfaces

- **I1 Standard names.** A member that does what a standard-library member does MUST have that member's name,
  parameters and return type (`insert` returns `pair<iterator, bool>`, `find` returns an iterator, `at`, `operator[]`,
  `try_emplace`, `insert_or_assign`, `erase`, `contains`, `count`, `equal_range`, `reserve`).
- **I2 Standard member types.** A container MUST declare the member types its standard counterpart has (`value_type`,
  `size_type`, `difference_type`, `reference`, `const_reference`, `iterator`, `const_iterator`, and `key_type` /
  `mapped_type` where it has keys).
- **I3 Only what is correct.** A member that is correct only for some element types or families MUST be constrained
  (`requires`) to them. A generic member that is silently wrong for some instantiation MUST NOT exist.
- **I4 Constrained templates** (*open*). A public class or function template MUST state what it needs of each type
  parameter with a concept or a `requires` clause. For a Store parameter that is `store_of<X, Kind, Tags...>`.

## Containers

- **C1 Policies, not branches.** Where variants of a container differ (thread-safe or not, tracked or not, rows or
  elements), the difference MUST be a policy type or an overload set chosen once. A member body MUST NOT branch on
  such a parameter with `if constexpr`; the policy's own body MAY.
  **Check** (*open*, 79 sites): `grep -rc "if constexpr" include/urd | grep -v ":0"`
- **C2 No representation special cases.** Generic code MUST NOT name or test for a particular container. One generic
  mechanism per need.
- **C3 Standard components first.** A component of the standard library or Boost MUST be used where it does the job;
  urd MUST NOT carry its own equivalent.
- **C4 One word type.** Bit-packing code MUST use `urd::word_t` and `urd::word_bits` (`urd/config.hpp`) and the
  helpers in `repr/containers/bit.hpp`. It MUST NOT spell `std::uint64_t` for a word or the literals 64, 63, 6.
  **Check**: `grep -rnE "\b(64|63)\b|>> 6\b|<< 6\b" include/urd/repr/containers include/urd/repr/pools`
- **C5 Checks stay.** A check that throws MUST NOT be removed or turned into an assert unless a profile shows it is a
  major cost.
- **C6 Simple before fast.** Specialized code for one case MUST NOT be added for a gain of a few percent. Measure,
  report the number, keep the simple code.
- **C8 One hash for equal values.** Equal values MUST hash alike whatever holds them (contiguous, segmented,
  decoded): `hash_range` mixes every range element by element. A faster path chosen by the holder (bytes of a
  contiguous range) MUST NOT be added: a key in a span and the equal row a packed pool stores would hash
  differently, and the table would lose the row. A type with one holder MAY hash its bytes: text does
  (`Hash<std::string_view>`, which every text type goes through).
- **C7 Measure where it shows.** A change to hashing, comparing or copying elements MUST be measured with
  `benchmarks/record.py` (long keys, strings) before its cost is reported, not only on one application.

## Declarations

- **D1 Derive, do not repeat.** A declaration that is a variant of another MUST inherit it and state only what
  differs (`with`, `with_header`, `costs_over`).
- **D2 Generated tables are the proof.** `share/urd/complexity.json`, `docs/RESOLUTION.md` and
  `benchmarks/requests.json` MUST pass their checks without being regenerated when a change is meant to leave the
  declarations' meaning as it was.
- **D3 Memory by members.** A container whose memory is the sum of its members' MUST state them with
  `memory_members()`; it MUST NOT specialize `urd::memory_of`.

## Outside urd

- **O1 Public names only.** Code outside urd MUST NOT name `urd::repr::`, except to include the header of the
  container a Store resolves to. What it needs of a representation has a public name: the bit vocabulary is
  `urd/bits.hpp` (`urd::bitset_span`, `urd::set_bit_indices`, `urd::bits::`), a tree bitset's flip `urd::block_flip`.
  A failing assertion in a template is `static_assert(false, ...)`.
  **Check** (in a downstream checkout): `grep -rn "urd::repr::" include src python tests`

## Adding or changing a rule

A rule is one sentence with MUST or MUST NOT, an id that is never reused, and a Check where one command can decide
it. A rule the code base does not meet yet is marked *open* with its current count.
