# Adding a representation

A representation is an existing container a `urd::Store` can resolve to: from the standard library, from
another library (Boost's flat sets), or copied in (yggdrasil's segmented vector). urd does not wrap containers
to give them properties; it registers containers that keep their properties by themselves. Downstream code
never names one; it asks for properties, and the registry picks the best representation that has them.
Adding one takes six steps; per container, only its `container_traits` and its place in the registry are written.

Before adding a container, check whether it is a variant of one already there: then it is a policy (a template
argument) of that one, not a new class. `IndexMap<K, V, Generations, ThreadSafe>`, `IndexedSet<T, Index,
ThreadSafe>`, `SegmentedVector<…, ThreadSafe>`, `Jagged` (frozen or fixed-shape rows, of elements or of bits),
`GroupedMultiMap<Groups>` (CSR or postings) and `Queue<T, Order>` (fifo, lifo, least first) are each one class
whose variants the registry names by alias.

## 1. Bring in the container

Put it under `include/urd/repr/`. Code taken from elsewhere keeps its copyright header and gets a line
naming its origin. Its names move into `urd::repr` and its macros take the `URD_` prefix, so a program can
include both urd and the original library without two definitions of one name. Its own tests come along
under `tests/repr/`, unchanged but for those names.

A representation is two headers. `repr/X_fwd.hpp` declares the class template (its default arguments and its
constraints go on this declaration, and nowhere else), carries its `container_traits`, `element_traits`,
`relies_on` and `orders_by_value`, the aliases the registry names, and says which family header defines it
(`urd::defined_in<X<…>>`, a `fixed_string` such as `"urd/use/dense.hpp"`); a wrapper also lists what it wraps
(`urd::parts<Layered<R>>` is `type_list<R>`). `repr/X.hpp` includes the declaration and defines the class. A
container from a library is declared through the library's own forward header (`boost/unordered/*_fwd.hpp`,
`boost/container/container_fwd.hpp`, `gtl/phmap_fwd_decl.hpp`; the standard's cannot be, and are included). The
registry includes declarations only, so that `urd/store.hpp` costs the same however many containers exist; the
family header `urd/use/<family>.hpp` includes the definitions and defines the marker `urd::include<"urd/use/
<family>.hpp">`. Families are cut by what they pull in (`docs/representations.rst` lists them): a container on
Boost.Unordered joins `hash`, a std-only one the family of its kind.

It must build cleanly under urd's warnings (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wsign-conversion`, errors in CI), which are stricter than yggdrasil's: fix the code, do not weaken the
flags. Copying `SegmentedVector` needed two explicit casts where signed and unsigned values met.

## 2. Add its dependencies

A dependency goes into `docs/SOURCES.json` (repository and revision, or archive and checksum) with a recipe
in `src/<name>/CMakeLists.txt` and its license under `src/<name>/licenses/`, the same setup as yggdrasil's.
Building `src/` installs everything into `dependencies-install/`:

```sh
cmake -S src -B dependencies-build -DCMAKE_INSTALL_PREFIX="$PWD/dependencies-install" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build dependencies-build -j8 && cmake --install dependencies-build
```

The main build then finds each one as a package (`find_package` in `CMakeLists.txt`, the prefix on
`CMAKE_PREFIX_PATH`) and links its imported target to `urd::urd`, so a consumer of urd gets it too through
`cmake/urdConfig.cmake.in`'s `find_dependency`; a new dependency is added in both places. Imported targets' headers
are system headers, so the warnings cover only urd. gtl is optional (`URD_WITH_GTL`): the registry lists its
tables under `URD_HAS_GTL`, and so do the tests that name them. A recipe that skips the build when the package is
already installed (cista's does) keeps an old install until `dependencies-install/lib/cmake/<name>` is removed.

## 3. Declare its properties: `container_traits<R>`

What a container guarantees is a type trait, as `graph_traits` is for Boost.Graph: one tag per category
(`include/urd/tags.hpp`), the root of a category where it promises nothing. List the tags it has:

```cpp
template<typename K, typename Allocator>
struct urd::container_traits<std::set<K, std::less<K>, Allocator>>
    : urd::properties_of<urd::sorted, urd::unique, urd::stable_refs, urd::sparse, urd::shrinkable>
{
};
```

Claim only what holds for every value of the type and through every operation; step 6 checks it.

Inside, properties are values, not types: each tag carries its number in `urd::tag`, and `properties_of`, `has`,
`satisfies` and the request rules compute with a `property_set` (one entry per category) by constant evaluation.
That keeps resolving a request cheap for the compiler. A Store's spec lists the tags that say more than their
category's root, so a diagnostic reads `spec<int, void, urd::tag::sorted, urd::tag::unique, urd::tag::shrinkable>`.
A new tag goes into its category's struct list and into `urd::tag`, in the same order (a `static_assert` checks
it), with its words beside it: a `urd::tag_synopsis` specialization (its name; for a root, its theme and question;
for any other tag, its role, description and phrases; a `static_assert` in `tests/names.hpp` checks that every tag has
them). Then regenerate the vocabulary with
`vocabulary_table --write share/urd/vocabulary.json`; the `vocabulary` test fails until it is.

Memory is a category too: `retains` (storage that erase and clear free is kept, so refilling to an earlier size
allocates nothing: vectors, flat sets) or `reclaims` (erase and clear return it: trees). A hash table that
frees its nodes but keeps its buckets declares neither. No representation yet serves a sequence that
`reclaims`; a Store could provide it by shrinking after removals.

## 4. Offer what the properties require

As a graph must model Boost.Graph's concepts, a container must offer the operations its declared properties
require (`include/urd/interface.hpp`), with the standard library's names and results; `urd::conforming<R>`
checks it at compile time:

| Property | Requires |
|---|---|
| any | a sized forward range, constructible from an iterator range |
| `canonical` | `==` |
| `contiguous` | a contiguous range |
| kept by value (`sorted` or `unique`, as std::set and std::flat_multiset) | `find`, `count`, `contains` |
| `sorted` | a bidirectional range; `lower_bound`, `upper_bound`, `equal_range` |
| `growable`, kept by value | `insert(e)` (`pair<iterator, bool>` if `unique`, else `iterator`), `insert(e&&)`, `emplace`, `insert(first, last)`, `insert(hint, e)` |
| `growable`, a sequence | `push_back(e)`, `push_back(e&&)`, `emplace_back` returning a reference |
| `shrinkable` | `clear`, `erase(position)` (its result convertible to an iterator, as Boost's flat hash tables return a proxy), `erase(first, last)` |
| `shrinkable`, kept by value | `erase(e)` returning a count |
| `shrinkable`, a sequence | `pop_back`, `insert(position, e)`, `emplace(position, …)` |

The standard library's containers, Boost's, and urd's own (`SegmentedVector`) offer them as they are; a container
of urd's should be written that way. A library with other names gets an adapter: a class that wraps it and
offers the standard names. An adapter only renames; it adds no guarantee, and a library update that changes
its interface breaks the adapter's build, not its behavior.

## 5. Register it

Add its template to one list in `include/urd/registry.hpp`: `element_preference` for one taking the element
type, `map_preference` for a map taking its key and value types. A container with further template parameters
gets an alias that fixes them (`ConcurrentSegmentedVector`, `TreeMapOf`, which also fixes urd's order). Nothing
else is written per container: a request takes the first container in the list that serves it, meaning its
`container_traits` for the request's elements satisfy the request and it offers the interface a Store for the
request offers. Where a request is `small` or `frozen`, containers with stable storage (declaring `stable_refs`: a
node per element, or segments that never move) move last, as contiguous storage is faster for few or unchanging
elements.

Selection reads traits and template arguments only, never a candidate's members: a request is resolved where the
candidates are declared but not necessarily defined, and a member read of a declared-only type is silently false,
which would drop the candidate rather than fail. So a `container_traits` specialization and its constraints may use
`has<container_traits<X>, …>`, `indexable`/`hashable`/`orderable` of an element type, type traits of template
arguments and `detail::row_element_t<Row>` (a row Store's elements, by its request), but not `std::ranges::*` of
the container, its `value_type`, `mapped_type` or `sizeof`. Two properties exist so that selection can ask them of
a declaration: `positional` (the container is a random-access range; `tests/properties.cpp` checks the two agree)
and `functional` (a value per key, a map). A nested Store's own semantics (`Hash`, `EqualTo`, `Less`) and
properties (`traits_t`) come from its request alone (`semantics/concepts.hpp`, `is_store`).

Beside its `container_traits`, in the same header, a container declares its costs: a specialization of
`urd::container_costs` (`include/urd/costs.hpp`) with a `static constexpr cost` per operation it has (`insert`,
`lookup`, `ordered_lookup`, `erase`, `clear`, `iterate`, `access`, `push`, `pop`, `top`, `merge`, `insert_row`,
`find_row`, `scope`), its memory in machine words per element (`words`) and what it holds besides its elements
(`holds`). A wrapper derives from `part_costs<part_of<W>>` and declares only what it changes. The same rule as for
the traits holds: values only, never a member of the container, so that reading a container's costs completes
nothing; the request groups (`tests/requests_group.cpp`) read every candidate's costs with no family header
included, which `tests/layout.cmake` keeps true. `tests/complexity.cpp` fails on an operation a Store offers
without a cost, and `tests/complexity_verify.cpp` counts the work of every cost not marked timed.

The Store's first base checks that its representation's family is included, by the marker and never by
completing the representation (completing a wrapper of an undefined part is an error, not a false), the parts
first; a file that holds a Store without its family fails with the header in the error, on both compilers:

```text
GCC:   error: static assertion failed: urd::Store: the representation's family is not included here: #include the
       header that urd::include<"..."> names in this error (docs/RESOLUTION.md names each request's)
       note: ‘(((bool)urd::included<urd::detail::fixed_string<18>{std::array<char, 18>{"urd/use/dense.hpp"}}>) || …)’
       evaluates to false
Clang: error: static assertion failed due to requirement 'included<defined_in<urd::repr::DenseSet<int>>> ||
       dependent_false<urd::include<urd::detail::fixed_string<18>{{"urd/use/dense.hpp"}}>>::value': urd::Store: …
```

The marker is read once per file, so a family header goes before the first Store that needs it (included later,
the Store still fails, with the same message). Two things prove the registry defines nothing: `resolution_table`,
which resolves every request of the grid, is built with no family header (`tests/layout.cmake` checks its
sources), and `tests/selection.cpp` resolves requests whose element is a Store of records stored flat without that
family's header, so completing the element would fail. The table's `--check` also rejects a chosen representation
that names no family. `tests/compile_fail_include.cpp` holds a Store of each family with its header left out and
expects the header in the error, plus a wrapper without its part's header (both named, the part first), a nested
Store without its inner's family (needed where the outer is held), and a header included too late. A budget test
(`tests/budget.py`) holds the bare include of `urd/store.hpp` under a memory limit per compiler, so the rule cannot
erode unnoticed.

A request no container serves fails in `no_representation_for<spec, culprits<…>, hashable, orderable, indexable>`,
whose arguments say why: the culprits are the stated tags whose removal, one at a time, would make the request
servable (`to_spec::without<tag>` re-runs the rules without one, so dropping `frozen` drops the implied
`stable_refs` too), and the three booleans say what the key type is where no tag is the culprit. The probes run
only on failure. Every rule of `to_spec` and every registry-level rejection has a compile-fail case that checks the
message, not only the failure (`urd_compile_fail_expects` in `CMakeLists.txt`); docs/language/index.rst lists them
under "Incompatible combinations", so a request is one of three things: served, missing (a `none` row of
docs/RESOLUTION.md with a note), or incompatible (a message).

What a container needs of its elements belongs in its traits, as constraints with the concepts of
`include/urd/elements.hpp` (`orderable`, `hashable`, `natural_order`, `natural_hash`): a tree is `sorted` only
for orderable keys, so a request for a sorted set of keys urd cannot order is not served by it.

## 6. Prove it

A property is part of what a Store is: it holds from construction (from any elements, which the
representation puts in the order asked for) and every operation keeps it, or the operation is not offered. A container whose property holds only
if callers remember to restore it is not a representation.

The exceptions are part of the properties' definitions (`include/urd/tags.hpp`), the same for every
representation, and the test checks them as defined:

| Property | Exception |
|---|---|
| `stable_refs` | the reference is the one the kind's path hands out (a map's value by `at` or an entry's `.second`, a queue's `front`, a pool's `[index]`); `erase` and `clear` end the references to what they remove |
| `concurrent` | covers inserts and reads of published elements by the kind's paths (`include/urd/tags.hpp` lists them per kind); erase of another thread's element, `clear`, iteration, `reserve`, copy, move and destruction need no other operation running |

- Pin what requests now resolve to it (`static_assert` on `Store<…>::representation` in `tests/store.cpp`).
- Add the container, and a Store that resolves to it, to the type list in `tests/properties.cpp`. That
  instantiates one typed test per property for it: each declared property is checked through the operations
  alone (sorted: order and `lower_bound`/`equal_range` after inserts in reverse; stable_refs: an address taken by
  the kind's path, a map's `at`, a sequence's position, a set's `find`, kept through 100000 inserts; retains: no allocation when refilling after a clear or after erasing each element;
  reclaims: a clear frees each element's storage; contiguous, canonical, unique, nonempty, concurrent
  likewise), and the others are skipped. The test binary counts allocations with its own global operator new. Another typed test runs `urd::verify` (`tests/verify.hpp`): it checks `conforming<R>`, then runs every operation the
  properties require at random (inserts, erases of positions and ranges, clears, assignments where the
  iterators allow them), and after every single step compares the contents, each operation's result and the
  lookups with a reference model and checks every declared property (sorted, unique, nonempty, stable_refs,
  contiguous, canonical), so a property that breaks and later recovers is caught too.
- `concurrent` needs threads: its typed test inserts from several threads and checks every element arrived;
  run it under ThreadSanitizer.
- A request it makes possible, and one that should still fail, as compile-fail cases.

## What the first tier showed

- **Registration needs operations, not only properties.** `SegmentedVector` keeps references stable but
  cannot erase from the middle. Without its mutability (`growable`, not `shrinkable`),
  `Store<value<T>, stable_refs>` would have resolved to it and offered an `erase` it cannot do. Now a rule rejects
  that request with a message (erasing at a position moves the elements after it; compile-fail case 13), and
  `Store<value<T>, stable_refs, monotone>` finds it.
- **Object pools are pools of slots.** yggdrasil's `UniqueObjectPool` and `SharedObjectPool` hand out owning,
  reference-counted handles. urd keeps the part that matters, slots reused once freed: `repr::SlotVector`, a
  segmented vector plus the indices of freed slots, thread-safe with `ThreadSafe`; the handle is the slot's
  index, released by `erase`.
- **The vocabulary grew by one property,** `concurrent`, which the copied container already offered as a
  template parameter.

## Keys and elements

What a container needs of its elements comes from `urd::Hash`, `urd::EqualTo` and `urd::Less`
(`include/urd/semantics/`, copied from yggdrasil): `hashable` and `orderable` in `include/urd/elements.hpp`,
and the registry instantiates every hashed or ordered container with them. They cover numbers, enums,
strings and the standard's composites (pairs, tuples, arrays, vectors, optionals, variants, spans), and Stores
whose contents fix the order their elements come in: `sorted`, `canonical` (a bitset hashes its words, up to the
last one set), or a sequence (`Hash<Store>` in `include/urd/store.hpp`, over the representation's own hash where
it has one, else `hash_range` over the elements). An unordered Store has no `Hash`, since equal ones could
iterate in different orders; a request keyed by one fails with no culprit tag and `hashable` false. A type of
your own becomes a key by naming its identity, with nothing in namespace std:

```cpp
struct Atom
{
    int predicate;
    std::vector<int> objects;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Atom::predicate, &Atom::objects> {}; }
};
```

Hash, equality and order then go member by member. Hashing is deterministic (xxHash64 over bytes, no
std::hash), so a hash table iterates in the same order on every platform. `tests/semantics.cpp` checks the
laws the containers rely on: equal values hash equally, and Less is a strict weak order agreeing with EqualTo.

## Maps in insertion order

A map with unique keys may ask for `insertion_ordered`: its entries stay in the order their keys were first inserted,
and `keys()`, `values()` and `position(it)` read them by position (spans). `repr::OrderedMap<Keys, V>` keeps the
keys in an insertion-ordered set and the values beside them: an `IndexedSet` hashes the keys, a `SparseSet<K, true>`
(the sparse set with its order kept on erase) places dense ones by index, with O(1) clear for values that need no
destructor. Erase keeps the order and costs O(n).

Every map with unique keys also has `update(key, f)`, which calls `f(value, held)` on the key's value, inserting a
default one first where there is none (`held` false), and `read(key, f, missing)`. A map cleared by generation
(`dense, constant_clear`) hands `update` the value it kept from before the clear, storage and all, so buffers are
reused. With `concurrent`, its slots lie in a thread-safe segmented vector with a spin lock each: `update`, `read`
and inserts may run from threads at once. A concurrent map by hashed keys is gtl's `parallel_flat_hash_map`, registered
by its traits alone: where a representation changes and reads a key's value under a lock of its own (gtl's
`lazy_emplace_l` and `if_contains`), the Store's `update` and `read` go through those, so no wrapper renames them.

Fixpoints ask whether a step changed anything. Every map with unique keys has `merge(key, value, combine) -> bool`:
it inserts where the key has no value, else stores `combine(held, value)` (a hull of intervals, the lower of two
costs), and says whether the map changed. Every set has `unite(other) -> bool`, a union that says whether it added
anything.

## Dense keys

Keys that are indices 0..n-1 (`urd::index_of`: integers, enums, types identified by one integral member; specialize
it for others) can be stored by position. A request for `dense` keys gets one of the dense containers
(`include/urd/repr/dense.hpp`): `DenseSet` (a bitset, iterated in key order), `SparseSet` (O(1) clear),
`IndexMap` (a slot per index, with or without generations for an O(1) clear). They come after the hash tables in
the registry, so only dense requests reach them. Clear cost is a category of its own: `proportional_clear`
(time in proportion to the elements present) and `constant_clear` (O(1)); the `ClearCost` typed test checks a
clear does not cost what the container's history did. Every set also has `includes`, `intersects`, `|=`, `&=` and
`-=` (`facet::set_algebra`), on whole words for a bitset.

## Insertion order and interners

`insertion_ordered` is an order beside `sorted`: a new element goes last, the others keep their places. A set
may also be `positional`: its elements have positions 0..n-1, and `operator[]`, `at`, `front`, `back` and
`position(iterator)` read them. A positional set keeps insertion order unless it asks to be `sorted`, so an
element's position is its index: `Store<key<T>, positional, monotone>` is an interner, whose `insert` returns
the element's iterator (`position` of it is the index) and whose indices never shift. Both resolve to
`repr::IndexedSet` (`include/urd/repr/indexed.hpp`), a vector plus a flat hash table of positions; erase keeps
the order and costs O(n). A container serves `positional` by declaring it, which the random-access ranges do
(vectors, segmented vectors, indexed sets, sparse sets, rows, the flat sorted containers).

## Indices and views

`urd::index<T>` (`include/urd/index.hpp`) is the index of a T in the Store that numbers Ts: a positional set
mints one (`index(iterator)`, read back by `operator[]`, `at` and `view`), and a pool's handles are indices too
(`handle_type` of a row interner is `index<Row>`, of a slot pool `index<T>`; the tree pools keep their own
handle, since they number nodes, not rows). An index is one 32-bit integer with a `none` value, a different type
per T that converts to no other, hashable, orderable, and a dense key, so `Store<key<index<T>>, value<V>, dense>`
keys a table by it and finds `none` absent. Write it `urd::index`: the C library declares a function `index` in
the global namespace, so an unqualified `index<T>` after `using namespace urd` is ambiguous.

`urd::view<T, C>` (`include/urd/view.hpp`, included where views are made) is a reference to an element held by
index: the index and a pointer to a context `C` that resolves it, where `resolves<C, T>` means `context[index<T>]`
gives the record. A positional set resolves its own type, and any type with such an `operator[]` serves, a family
of Stores for every member type among them. `record()` reads the element; `get<&T::m>()` reads a member resolved
through the same context (`urd::resolve`): an `index<U>` becomes a `view<U, C>` where the context resolves U, else
stays an index; a range of indices a range of views; characters a `string_view`; another contiguous range a span;
a variant the variant of its alternatives resolved. Views compare and hash by index, and by the context's
`layer()` number where it has one, so that two views of one element from one layer are equal wherever they were
made, and views from different layers are not. `urd::context_of<T>` names a record type's default context once,
so `view<T>` needs no second argument, and `urd::view_mixin<T, V>` is where a library adds the named accessors of
a record, one line each over `get<>`. The denials name `view` for the Stores that have none.

`Store<key<K>, value<V>, multi, dense, frozen>` resolves to `repr::CsrMultiMap` (`include/urd/repr/grouped.hpp`):
compressed sparse rows, built at once from pairs in any order by a counting sort that keeps the order of equal
keys. Every multimap Store has `values(key)`, the values of a key as a range: a `std::span` here, a view of
`equal_range` elsewhere. With `bidirectional`, a map answers by value too: `keys(v)` gives the keys that map to
`v`. A frozen graph is `Store<key<V>, value<V>, multi, dense, frozen, bidirectional>` (`repr::BidirectionalCsr`, a
CSR in each direction): successors are `values(v)`, predecessors `keys(v)`.

A frozen Store changes only by `assign(first, last)`, which leaves it holding what a fresh build would. A
representation with `assign` rebuilds in the storage it holds: compressed sparse rows count each key's values, sum the
counts to offsets, place the values and shift the offsets back, so a per-state index rebuilt from pairs of the same
size allocates nothing.

## Postings

A multimap without order or stable references (`Store<key<K>, value<V>, multi>`) resolves to `repr::PostingMap`
(`include/urd/repr/grouped.hpp`): a flat hash table from each key to the vector of its values, in insertion
order, so `values(key)` is a span. The multimaps are one class, `GroupedMultiMap`, over how the groups of values are
kept: `IndexGroups` (frozen compressed sparse rows), `HashGroups` (postings) and `DenseGroups` (a vector of values
per index, growing: `Store<key<I>, value<V>, multi, dense>`, `repr::DenseMultiMap`); another way to keep them is a
policy, not a new multimap. Keeping postings in sync when the rows they point to move (a swap-erase) is the
caller's.

## Order by a projection

`order_by<P>` orders a sorted Store or a priority queue by `P(element)` (`P(key)` for maps): `P` is a member pointer
or a stateless lambda, and a tuple orders by several fields (`std::tuple(job.deadline, -job.weight)`: earliest
deadline, then largest weight).
Ties fall back to the element's own order, so a sorted set still holds every distinct element; unique keys
must therefore be orderable, while a queue's elements need no order of their own. The comparator,
`urd::ProjectedLess<T, P>`, is part of the spec, and only containers that take a comparator are candidates for it
(`ordered_element_preference`, `ordered_map_preference` in the registry): a container with a comparator joins
those lists to serve projected orders too.

With two arguments, `order_by<P>` orders a map by `P(key, value)`: entries are found by key and come in the order
their values give, ties by key (`Store<key<Interval>, value<Offer>, sorted, small, order_by<cheapest>>`). A value then moves
its entry, so it changes only through what puts the entry back in order: `update`, `merge`, `insert_or_assign`; the
Store has no mutable `at` or `operator[]`, and no bounds by key. Representations that order by values declare
`orders_by_value<R>` and offer `update` in place of a mutable `at`; `repr::FlatEntryMap` keeps few entries in one
sorted vector, found by a scan, so the request must say `small` or `frozen` (a value-ordered map of many changing
entries is rejected with a message, compile-fail case 55).

## Queues

A Store with an order of leaving is a queue: `fifo`, `lifo` or `by_priority` (least first by `urd::Less`). It offers
`push`, `emplace`, `pop`, `size`, `empty`, `front()` and `back()` (fifo) or `top()` (the others), `clear` unless it is
`monotone`, and `begin`/`end`: in the order pushed, or for a priority queue in none (a bucket queue, which would visit
its whole priority range, is not iterated). It is not compared. They resolve to `repr::Fifo` (a deque), `repr::Lifo` and `repr::Heap` (vectors,
which retain their storage across clears) in `include/urd/repr/queues.hpp`; the standard adaptors lack `clear`,
which a queue reused per search needs. A queue's container declares its order in `container_traits` and offers
the operations above in place of a range's (`include/urd/interface.hpp`); tests/queues.cpp checks each against a
model.

With `handles`, a priority queue's `push` returns a handle, and `update(h, e)` and `erase(h)` change or remove that
element wherever it is (`repr::AddressableHeap`, an indexed binary heap). `monotone_priority` says pushes never go
below the last popped (Dijkstra with non-negative costs); with integral priorities (the element, or its `order_by`
projection) it resolves to `repr::BucketQueue`, which pops in order whatever is pushed but is fast only under that
promise, so it serves only requests that make it: a representation names the request-only promises it relies on
in `relies_on<R>`, and selection skips it for requests that do not make them.

## Scopes and layers

`scoped` (a map): `push_scope` opens a scope and `pop_scope` undoes every change made since, latest first, so an
inner binding shadows an outer one until its scope ends; `depth()` counts open scopes, and `bound_in_scope(key)`
says whether the innermost one bound the key (a redeclaration check). `repr::Scoped<Map>`
(`include/urd/repr/scoped.hpp`) is any unique map plus a log of previous values, registered over a flat hash
table, a tree (`sorted`) and a dense map (`dense`); `clear` closes every scope.

`layered` (a set or map with unique keys): `layer_over(&parent)` makes lookups find the parent's elements too. The
Store holds, iterates and erases only its own; inserting what the parent has inserts nothing; in a map, its own
value shadows the parent's, and `at()` on a layer that may change copies the parent's value up first (read
through a const Store to avoid that). The parent is not owned: it must outlive the layer and stay unchanged.
`repr::Layered<R>` (`include/urd/repr/layered.hpp`) layers a flat hash table, or a dense map (`Store<key<I>,
value<V>, dense, layered>`, `Layered<IndexMap>`). Where a map's values layer themselves, `at()` on a layer makes
its own value as a layer over the parent's instead of a copy, so a map of layered pools layers through both levels:
the relations of a planning task, `Store<key<index<Predicate>>, value<Rows>, dense, layered>` with `Rows` a
layered pool of rows of object indices, fall through to the previous phase's pool for a predicate the phase did not
touch, and continue that pool's indices for one it did (`tests/store.cpp`, `RelationsLayerThroughBothLevels`). A
memo kept for two generations is two maps swapped and cleared per generation (examples/scopes.cpp), not a
representation.

### Records stored flat

A pool of records, `Store<value<Record>, handles, unique, flat>`, interns whole records instead of rows: a record is
any type that names its identity (`identifying_members`), and `flat` says each is stored as one block of bytes.
A trivially copyable record is such a block already and goes into `repr::RecordInterner` (an indexed set); any
other must be serializable by cista, naming its members in `cista_members()` and holding its sequences and text as
`urd::flat_vector<T>`, `urd::flat_string` and `urd::flat_variant<Ts…>` (`include/urd/flat.hpp`, which also gives
their hash and equality), and goes into `repr::ArenaInterner`: the record is serialized into a byte arena
(`repr::ByteArena`, segments that never move) and read back in place, so the stored record's vectors point inside
its block. The registry names both interners through declarations only (`repr/records_fwd.hpp`); the Store that
holds one needs the records family, `<urd/use/records.hpp>`, included, which the error of a Store without it
names. A request for a pool of records that are no range of elements
must say `flat`, and `flat` goes with `handles, unique` only.

A flat pool is counted and read by index: `insert(record)` gives `pair<index<Record>, bool>`, an equal record its
index again; `insert(key, make)` runs `make(index)` with the index reserved, for a record that holds its own index
(it must give a record equal to the key); `find(record)` is the index or `none`; `contains` takes a record or an
index; `operator[]`, `at` and `view` read by index; `memory_usage`, `size`, `empty`, and `clear` where shrinkable,
which keeps the arena's segments. The records are frozen once stored. With `concurrent` both interners take inserts
from threads at once: lookups without a lock, a new record under the writer lock; clear, move and destruction need
quiescence. The typed tests run `run_records` (`tests/verify.hpp`) over both.

### Layered pools

A pool may be `layered` too: `Store<value<Record>, handles, unique, flat, layered>` (records) or `Store<value<Row>,
handles, unique, layered>` (rows), `repr::LayeredPool` over the interner the request would otherwise get. A layer
branches from a parent by `layer_over(&parent)` while it holds nothing of its own; its indices continue after the
parent's size at the branch, so an index means the same in every layer below. A lookup answers from the layer,
then from the parent's chain; an insert finds its record anywhere in the chain before adding it here; reading by
index goes to the layer that owns it (`owner(index)`), and a `view` made of a layered pool points at that layer, so
two views of one element from two layers are equal. `layer()` numbers the chain from the root, `base()` is where
the layer's own indices start, `parent_grew()` tells whether the parent broke the rule that it must not grow after
the branch, which inserts assert. `clear` forgets the layer's own elements and keeps the chain. The tree pools do
not layer, since they number nodes, not rows. `run_layered_records` (`tests/verify.hpp`) runs a chain of three
against its model.

## Change logs

`change_log` (a set with unique elements): `added()` and `removed()` are spans of exactly the elements that
changed since `clear_changes()`; an element added and removed again is in neither. For semi-naive evaluation:
each round works on `added()`. `repr::ChangeLogged<R>` (`include/urd/repr/change_logged.hpp`) logs into two
indexed sets.

## Partitions, and compositions of Stores

`Store<key<K>, partition>` is a partition of indices into classes (`repr::DisjointSets`, union-find):
`insert(k)` adds elements, `merge(a, b)` joins classes, `find`, `same`, `class_size` and `classes()` ask about them.

The include root holds only the vocabulary and `Store` (tests/layout.cmake checks it); containers are in
`urd/repr`. Compositions that hold Stores and choose no representation of their own are in `urd/compose`:
`urd::compose::Family<F, Ts...>` keeps one `F<T>` per type, reached by `get<T>()`. Where the members number their
elements (positional sets, pools), the family resolves an index of any member type, `family[index<T>]`, so a
`view<T, Family>` reads a record and the indices held in it through one context; `insert`, `find` and `contains` go
to the member for the record's type and speak in indices. Where the members layer, the family layers:
`Family(&parent)` layers each member over the parent's member of its type, `layer()` numbers the chain from the
root, and `view(index)` points at the family that owns the element, so the views' identities are (index, layer).
A repository of symbols is such a family of layered pools of flat records, one phase over the previous
(`tests/survey.cpp`, `formalism`). A two-level dense map needs
nothing new: nested dense Stores compose (`Store<key<R>, value<Store<key<Row>, value<V>, dense>>, dense,
constant_clear>`). A matrix of bits of fixed shape waits for a `uniform` property on nested Stores (any depth,
`repr::BitTensor`).

## Pools: rows by handle

A Store with `handles` is a pool: `insert` gives a handle, and the element (a row) comes back by it. An interning pool's
insert gives `pair<handle, bool>`, as a set's gives `pair<iterator, bool>`, the bool saying it stored something; `find(row)`
gives the handle insert would without storing, or the handle type's none. It is not iterated; without
`structural_sharing` it is counted, its handles 0..size()-1. A row pool without `structural_sharing`, `monotone`, `concurrent` or `layered` erases by moving
its last row into the hole and reporting which handle moved.

- `handles, unique`: an interning pool. Equal rows get equal handles, so a handle stands for its row in comparisons
  and as a key. `get(h)`, `read(h, span)`, `row_size(h)`, `memory_usage()`; a successor goes in as a delta:
  `insert(base, writes)` for rows of values (`urd::Write{position, value}`), `insert(base, added, removed)` for
  rows that are sets, which also answer `contains(h, element)`. `repr::RowInterner` stores each row once, in full.
- With `structural_sharing` the rows are trees whose nodes are interned, so states share what they have in
  common and a successor costs only the paths to what changed: `repr::TreeVectorSet` for rows of values,
  `repr::TreeBitsetSet` (32-bit blocks, zero subtrees left out) for rows that are sets of indices. Both are copied
  from yggdrasil.
- `handles` without `unique`: a pool of slots, `repr::SlotVector`. Each insert takes a new slot; `erase(h)`
  frees it for reuse, and `acquire()` hands out a freed element as it was left (a scratch buffer keeps its
  storage). Slots never move.
- `concurrent` makes any of them thread-safe (`ThreadSafe` in the representations): inserts from threads at once,
  and reads of what is already inserted. The interning tables are then `repr::ConcurrentIndexedSet`, which also
  serves the thread-safe interner `Store<key<T>, positional, monotone, concurrent>`.

A pool that keeps rows in full lends them out: `view(h)` is the stored row without a copy, a span of its elements,
or for rows of dense indices a `BitsetSpan` over the words they are stored as. The table moves the rows' vectors as
it grows, never their elements, so a view stays valid until the pool is cleared.

## Maps

A map's values, where they are Stores, are rows like a sequence's: the representation's `element_traits` say what
they keep. Most maps free a value when its key goes; `repr::Recycling<Map>` (over a std::map or std::unordered_map) keeps the node and the value, cleared, for
the next key, so `Store<key<K>, value<Row>, sorted>` with a `retains` Row refills without allocating.

`key<K>` with `value<V>` makes a map, `multi` a multimap; the elements are `std::pair<const K, V>`. A map
container names `mapped_type`, which tells the registry it is one; its lookups and `erase` take a key, and
with unique keys it offers `at`, `try_emplace` and `insert_or_assign` (`include/urd/interface.hpp`). The
element requirements (`orderable`, `hashable`) apply to the key. A map Store hands out const iterators and
changes values by key only, since Boost's flat maps would let a key be overwritten through an iterator.

## The Store's facets

`Store` holds only what every Store has, whatever its representation: construction from elements and swap.
Everything else is a facet, even `sizing` (size, empty), `iteration`, `equality` and `ordering` (== and <=>), and
`capacity` (reserve): every kind so far has them, but a filter would not count. Each group of properties
has a facet in `include/urd/store/`, a class with the members those properties allow, which the Store
inherits where its properties switch it on: `lookup` (kept by value or key), `ordered_lookup` (`sorted`), `reverse_iteration` (an order: `sorted`, or a sequence),
`positions` (a sequence), `contiguous_data`, `growth`, `removal`, and `values_by_key` (a map with unique keys).
A new property or kind adds a facet, a case in `facet_switches` (`store/types.hpp`), and a line in the Store's
list of bases. Within one Store each member name lives in one facet only, so that calls are never ambiguous
between two; across kinds a name recurs (`insert` in `growth`, `pool`, `slots` and `grouping`), which the
denials record per name. Facets take their types from `store_types`, as the Store is incomplete while they are
built. A Store whose facets are off is no larger than its representation.

Which operations a request gets follows one rule: a Store offers an operation wherever every representation that
can serve its request performs it at the best cost there is for it, and nowhere else. A pool without
`structural_sharing` is served by `RowInterner` alone, which counts its rows in O(1), so it has `size()`; a tree pool
cannot count its rows without a list of them, so it has not. Where a request rules an operation out, a denial (or a
denied overload in the facet that declares the name) says which property does, and a compile-fail case pins it. A
new representation that cannot do an offered operation optimally must not serve those requests, or the rule moves
the operation off them.

**Denials.** Where a facet is off, its names would be missing and a call would fail with "no member named
insert" and no word on why. `store/denials.hpp` stands in for them: for each group of names, a class declaring
the same names as function templates constrained by `denied<"reason">`, a concept never satisfied, so the error
names the reason ("insert: the Store is frozen; it changes only by assign(first, last)") while
`requires { store.insert(x); }` stays false, as for a missing member. A denial is inherited only where none of the
facets that declare one of its names is on, since two bases with one name would make its lookup ambiguous; the
`declares_*` tables say which facets those are, and `tests/denials.cpp` checks every table against the facets, so
a facet that gains or loses a name fails there. Adding a name to a facet means adding the facet to that name's
table; adding a facet means a case in `facet_switches` and, for each name it declares, its table. The
message-checking compile-fail cases (`urd_compile_fail_expects` in `CMakeLists.txt`) pin five of the reasons.

## Choosing among containers

A workload (`small`) is the one category that only chooses: a request says how the Store is used, which
changes the representation picked, never what the Store guarantees. Each workload must be backed by a
benchmark.

The preference lists in `include/urd/registry.hpp` are judged against costs and measurements, both kept as data:

- **Costs.** Each container declares its cost per operation and its memory per element (`urd::container_costs`,
  section 5); `share/urd/complexity.json` is generated from them (`complexity_table`).
- **What serves what.** `benchmarks/requests.json` lists, for every request of the grid, the containers serving it
  in the registry's order and the operations its Store offers, and the experiments: each container on each shape of
  element. It is generated (`requests_table --write benchmarks/requests.json`, one object per group of the grid) and
  checked by the `requests` test, so a new container or a reordered list shows up there.
- **Measurements.** Each container is measured on its own: `benchmarks/experiment.cpp` is compiled once per
  container, with only its family headers, and runs its lifecycle at three sizes. `benchmarks/record.py record
  --binary <build>/benchmark_comparisons` measures the cells that are missing or stale (a cell is stale when a header
  it measures changed), so a new or changed container is measured alone.
- **Claims.** `benchmarks/record.py check` (the `benchmark_results` test) compares, for each request, the chosen
  container against every other serving it, from the data alone. An other that gives a costly guarantee the request
  did not ask for and the chosen one does not give (`stable_refs`, `concurrent`, `layered`, ...: the `costly` list in
  `tests/complexity.hpp`) is no fair alternative and is not compared, as the registry never hands such a guarantee
  out unasked. The others are compared by cost (a wrong order fails) and otherwise by the measured time of a
  lifecycle: the chosen one may be slower by at most 10% or 5 ns per element, whichever is larger. Nothing is excused
  in writing, and no benchmark compares two containers: a comparison reads their two results.

A change to an order is backed by these: `docs/BENCHMARKS.md` shows the result.

A declared guarantee the request left open is free: a request that did not ask for `stable_refs` may receive a
node-based table. One guarantee is also a demand on the caller, `dense`: a container that stores by index needs
keys that are indices, its storage grows with the largest key, and a negative key is no index at all. So
`demands_met` (`include/urd/property_set.hpp`) lets a dense container serve only a request that promises `dense`,
whatever its place in the list; a partition promises it by rule. Before that rule, a concurrent map by an integer
key with `sorted` or `retains` silently fell through to the thread-safe generation map and asserted on the first
negative key.

Two tests watch the order. `docs/RESOLUTION.md` is generated from a grid of requests (`tests/grid.hpp`) by
`resolution_table --write docs/RESOLUTION.md` and checked by the `resolution` test, so a change to the registry that
moves a request shows up as a diff of that file, to be reviewed and committed with the change. A `none` row of that
file carries a note (`notes` in `tests/resolution_table.cpp`): missing, a representation that could be written, or
incompatible, a contract none can keep; the tool fails on a `none` row without a note and on a note for a request
that is served now, so the record of gaps cannot go stale. The `registry` test (`tests/registry.cpp`) audits each
request: among the candidates that serve it, the one chosen must give no more costly guarantees nobody asked for
(storage by index, nodes, locks, generations, a log, a parent, shared parts) than another serving one; where the
order makes such a choice on purpose, a tree for a sorted set that changes, the request is listed there with its
reason, and the test also reports when a listed request no longer surprises. A second, absolute rule needs no
comparison: no serving candidate demands `dense` of a request that did not promise it.

## Nested Stores

A Store's elements can be Stores (rows): `Store<value<Store<value<int>, frozen>>>`. The same rules apply one
level down. What a container guarantees about rows it holds is `element_traits<R>`
(`include/urd/traits.hpp`): by default the rows' own properties, except memory, since destroying a row frees
what it owns. A representation serves a nested request only if its `element_traits` satisfy the rows'
request, so the inner properties choose the outer representation too:

| Request | Resolves to | Rows come as |
|---|---|---|
| rows `frozen` | `repr::Jagged`: CSR, one offsets array per level and one array of the innermost elements; three levels are a `Jagged` of a `Jagged` | a read-only slice: `std::span` of the elements, or a `std::ranges::subrange` of the rows below; a sorted row is searched with `std::ranges::lower_bound` |
| rows and Store `retains` | `repr::RowPool`: removed rows are cleared and kept, so nothing is freed at any level | the inner Store, `Inner&` |
| anything else | `std::vector` of the inner Stores | the inner Store |

`retains` is shallow, about a container's own storage; deep retention is the rows' `memory_category` in
`element_traits`. A row, and references into it, are valid until the outer Store changes.
`tests/rows.cpp` checks every nested Store against a `std::vector` of `std::vector`s under random inserts
and erases of rows, sorted rows' lookups, and zero allocations when a retaining Store is refilled.
