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
(`include/urd/traits.hpp`), the root of a category where it promises nothing. List the tags it has:

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
it).

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
request offers. Where a request is `small` or `frozen`, node-based containers (declaring `stable_refs`) move
last, as contiguous storage is faster for few or unchanging elements.

What a container needs of its elements belongs in its traits, as constraints with the concepts of
`include/urd/elements.hpp` (`orderable`, `hashable`, `natural_order`, `natural_hash`): a tree is `sorted` only
for orderable keys, so a request for a sorted set of keys urd cannot order is not served by it.

## 6. Prove it

A property is part of what a Store is: it holds from construction (from any elements, which the
representation puts in the order asked for) and every operation keeps it, or the operation is not offered. A container whose property holds only
if callers remember to restore it is not a representation.

The exceptions are part of the properties' definitions (`include/urd/traits.hpp`), the same for every
representation, and the test checks them as defined:

| Property | Exception |
|---|---|
| `stable_refs` | `erase` and `clear` end the references to what they remove |
| `concurrent` | covers inserts and reads of published elements; `erase`, `clear` and iteration need no other operation running |

- Pin what requests now resolve to it (`static_assert` on `Store<…>::representation` in `tests/store.cpp`).
- Add the container, and a Store that resolves to it, to the type list in `tests/properties.cpp`. That
  instantiates one typed test per property for it: each declared property is checked through the operations
  alone (sorted: order and `lower_bound`/`equal_range` after inserts in reverse; stable_refs: an address kept
  through 100000 inserts; retains: no allocation when refilling after a clear or after erasing each element;
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
  `Store<value<T>, stable_refs>` would have resolved to it and offered an `erase` it cannot do. Now that request finds no representation (a compile-time error), and
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
strings and the standard's composites (pairs, tuples, arrays, vectors, optionals, variants, spans). A type of
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
the order and costs O(n).

## Frozen multimaps by dense key, and graphs

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
order, so `values(key)` is a span. Both multimaps are one class, `GroupedMultiMap`, over how the groups of values are
kept (`IndexGroups`, `HashGroups`): a third way to keep them is a policy, not a new multimap. Keeping postings in sync when the rows they point to move (a swap-erase) is
the caller's.

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
their values give, ties by key (`Store<key<Interval>, value<Offer>, sorted, order_by<cheapest>>`). A value then moves
its entry, so it changes only through what puts the entry back in order: `update`, `merge`, `insert_or_assign`; the
Store has no mutable `at` or `operator[]`, and no bounds by key. Representations that order by values declare
`orders_by_value<R>` and offer `update` in place of a mutable `at`; `repr::FlatEntryMap` keeps few entries in one
sorted vector, found by a scan.

## Queues

A Store with an order of leaving is a queue: `fifo`, `lifo` or `by_priority` (least first by `urd::Less`). It is
not iterated; it offers `push`, `emplace`, `pop`, `size`, `empty`, `front()` (fifo) or `top()` (the others), and
`clear` unless it is `monotone`. They resolve to `repr::Fifo` (a deque), `repr::Lifo` and `repr::Heap` (vectors,
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
inner binding shadows an outer one until its scope ends; `depth()` counts open scopes. `repr::ScopedMap`
(`include/urd/repr/scoped.hpp`) is a flat hash table plus a log of previous values; `clear` closes every scope.

`layered` (a set or map with unique keys): `layer_over(&parent)` makes lookups find the parent's elements too. The
Store holds, iterates and erases only its own; inserting what the parent has inserts nothing; in a map, its own
value shadows the parent's, and `at()` on a layer that may change copies the parent's value up first (read
through a const Store to avoid that). The parent is not owned: it must outlive the layer and stay unchanged.
`repr::Layered<R>` (`include/urd/repr/layered.hpp`) layers a flat hash table. A memo kept for two generations is
two maps swapped and cleared per generation (examples/scopes.cpp), not a representation.

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
`urd::compose::Family<F, Ts...>` keeps one `F<T>` per type, reached by `get<T>()`. A two-level dense map needs
nothing new: nested dense Stores compose (`Store<key<R>, value<Store<key<Row>, value<V>, dense>>, dense,
constant_clear>`). A matrix of bits of fixed shape waits for a `uniform` property on nested Stores (any depth,
`repr::BitTensor`).

## Pools: rows by handle

A Store with `handles` is a pool: `insert` gives a handle, and the element (a row) comes back by it. It is neither
iterated nor counted by rows.

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
`capacity` (reserve): every kind so far has them, but a queue would not iterate and a filter would not count. Each group of properties
has a facet in `include/urd/store/`, a class with the members those properties allow, which the Store
inherits where its properties switch it on: `lookup` (kept by value or key), `ordered_lookup` (`sorted`), `reverse_iteration` (an order: `sorted`, or a sequence),
`positions` (a sequence), `contiguous_data`, `growth`, `removal`, and `values_by_key` (a map with unique keys).
A new property or kind adds a facet and a line in the Store's list of bases. Each member name lives in one
facet only, so that calls are never ambiguous between two; facets take their types from `store_types`, as the
Store is incomplete while they are built. A Store whose facets are off is no larger than its representation.

## Choosing among containers

A workload (`small`) is the one category that only chooses: a request says how the Store is used, which
changes the representation picked, never what the Store guarantees. Each workload must be backed by a
benchmark.

The preference lists in `include/urd/registry.hpp` follow `docs/BENCHMARKS.md`, which `benchmarks/` produces
(Google Benchmark, `-DURD_BUILD_BENCHMARKS=ON`). A change to an order cites a benchmark.

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
