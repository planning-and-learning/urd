# urd

**Say what your data is. urd picks the container and checks that it keeps its promises.**

```cpp
#include <urd/store.hpp>
using namespace urd;

struct User                                                             // an index: one integral member
{
    std::uint32_t id;
    static constexpr auto identifying_members() noexcept { return Members<&User::id> {}; }
};

using Names    = Store<key<std::string>, positional, monotone>;         // an interner: each name gets an index
using Follows  = Store<key<User>, value<User>, multi, dense, frozen, bidirectional>;  // a frozen graph
using Seen     = Store<key<User>, dense>;                               // a bitset
using Frontier = Store<value<User>, fifo>;                              // a queue
using Distance = Store<key<User>, value<int>, dense, constant_clear>;   // cleared per search in O(1)

// Everyone a user reaches by following, breadth first.
auto follows = Follows(edges.begin(), edges.end());  // built once from (follower, followed) pairs
auto seen = Seen { start };
auto frontier = Frontier();
frontier.push(start);
while (!frontier.empty())
{
    const auto user = frontier.front();
    frontier.pop();
    for (const auto next : follows.values(user))     // a span: whom this user follows
        if (seen.insert(next).second)
            frontier.push(next);
}
// follows.keys(user) gives who follows a user; follows.insert(…) does not compile: frozen.
```

`User` is an index, as `dense` asks: integers, enums, or types identified by one integral member (`urd::index_of`).
User 17 is bit 17 of the bitset.

You write *what* the data is (keys or values, ordered or not, dense indices, frozen, cleared every round, shared
between threads) instead of *which* container holds it. urd resolves the request at compile time to a container
from the standard library, Boost or its own `repr`, and gives the Store exactly the interface those properties
allow: a `frozen` Store has no `insert`, an unordered one no `lower_bound`, a queue no iteration.

Every container urd can choose is checked to keep the properties it declares: typed tests compare each one with a
reference model through thousands of random operations, and deliberately broken versions of the containers are
caught by them. The vocabulary is small enough for people and code-writing agents alike to state instead of
hand-rolling a structure, and errors say what is wrong:

```
error: static assertion failed: urd::Store: two properties answer the same question (dense and sparse, ...)
In instantiation of 'no_representation_for<spec<std::string, void, ..., urd::tag::dense, ...>, culprits<urd::tag::dense>, ...>'
error: static assertion failed: urd::Store: no representation provides this combination of properties. The
       culprits<...> above name the tags whose removal, one at a time, would make it servable: ...
error: no matching function for call to 'Store<std::set<int, ...>, spec<int, void, urd::tag::sorted,
       urd::tag::unique, urd::tag::shrinkable>>::push_back(int)'
note:  constraints not satisfied: 'denied<"push_back/emplace_back: a keyed Store (a set or a map) takes insert">'
```

Header-only C++20, GPL-3.0.

## Elements: any type, any depth

`key<K>` and `value<V>` take any type, and every Store is itself a type they take. The only demands come from the
properties, and only on the type they concern:

| What the request says | What the type needs | Given for |
|---|---|---|
| keys, unordered (the default) | `hashable` (`urd::Hash`, `urd::EqualTo`) | arithmetic types, enums, strings, ranges, Stores whose contents fix their order (`sorted`, `canonical`, sequences), and any type that names its identifying members |
| keys, `sorted` (also `by_priority` elements) | `orderable` (`urd::Less`, `urd::EqualTo`) | the same |
| keys, `dense` | `indexable` (`urd::index_of`) | non-negative integers, enums, types identified by one integral member, and `urd::index<T>` (what a positional set or a pool mints) |
| values, elements of sequences and queues | nothing | — |

A type names its identifying members once and is then hashable, orderable and comparable, in member order:

```cpp
struct Edge
{
    Vertex from, to;
    static constexpr auto identifying_members() noexcept { return Members<&Edge::from, &Edge::to> {}; }
};
using Edges = Store<key<Edge>>;                    // hashed by (from, to)
```

**Nesting.** A Store of Stores is written as one: an element, key or value can be another Store, to any depth.

```cpp
using Tags     = Store<key<std::string>, sorted>;
using Tagged   = Store<key<Item>, value<Tags>>;                          // a map of sorted sets
using Families = Store<key<Tags>>;                                       // a set of sets (hashed by contents: Tags is sorted)
using Trips    = Store<key<std::string>, value<Store<value<Point>>>>;    // a map of sequences
using Layers   = Store<value<Store<key<Cell>, value<Store<value<Item>>>>>>;  // sequences of maps of sequences
```

Most rows are held as Stores, each owning its storage, which every container can hold. Where the outer request
says how rows live, urd stores them together instead, and hands out a row as a view:

| Rows | Request | Container | A row is |
|---|---|---|---|
| never change | `Store<value<Store<value<T>, frozen>>>` (any depth) | `Jagged`: CSR, one array per level | `std::span<const T>`, or a range of the rows below |
| fixed lengths, changing contents | `Store<value<Store<value<T>>>, fixed_shape>` | `Jagged` built from `urd::row_lengths` | `std::span<T>` |
| … of dense indices | `Store<value<Store<key<I>, dense>>, fixed_shape>` | `Jagged` over one word array | a `BitsetSpan` (one bit per index) |
| … at any depth | `Store<value<Store<value<Store<key<I>, dense>>, fixed_shape>>, fixed_shape>`, built from nested vectors of lengths | `Jagged` of `Jagged` | a range of the rows below |
| reused after removal | `Store<value<Row>, retains>` with a `retains` row | `RowPool` | the row Store, cleared, not freed |
| map values reused after erase | `Store<key<K>, value<Row>>` with a `retains` row (`sorted` or not) | `Recycling<std::unordered_map>` / `Recycling<std::map>` | the value Store, cleared, kept with its node |
| interned, by handle | `Store<value<Row>, handles, unique>` | `RowInterner`, tree pools | the row, read by handle |
| records, each one block of bytes | `Store<value<Record>, handles, unique, flat>` | `RecordInterner` (trivially copyable records), `ArenaInterner` (serialized by cista; `<urd/flat.hpp>`) | the record, read by `index<Record>` |
| over a parent pool | `Store<value<Record>, handles, unique, flat, layered>` (rows too) | `LayeredPool` over the interner: indices continue after the parent's, lookups fall through | the record, from the layer that owns it |

`Rows::reference` names what a row is handed out as; code needs no name from `urd/repr`.

**Repositories.** A record type names its identity and its members; a family of pools, one per record type, interns
the records, numbers them (`urd::index<T>`, a dense key), layers one phase over the previous, and resolves the
indices a record holds into views of what they name:

```cpp
struct Atom { index<Predicate> predicate; flat_vector<index<Term>> terms; /* identifying_members, cista_members */ };
template<typename T> using Symbols = Store<value<T>, handles, unique, flat, monotone, layered>;
using Repository = compose::Family<Symbols, Object, Predicate, Term, Atom>;
auto task = Repository(&domain);                                     // the task's symbols after the domain's
const auto [atom, fresh] = task.insert(Atom { on, { block, x } });   // index<Atom>; the domain's records are found too
task.view(atom).get<&Atom::predicate>().get<&Predicate::name>();     // view<Predicate, Repository>, from the layer that owns it
```

## What a request resolves to

[docs/RESOLUTION.md](docs/RESOLUTION.md) lists, for a grid of requests over a few element types, the container each
resolves to (`include/urd/registry.hpp`; the order of candidates there is backed by
[docs/BENCHMARKS.md](docs/BENCHMARKS.md)). The table is generated from the registry and checked against it by a
test, so it is what the current registry does, not a description of it. A selection: a set is an open-addressing
hash table (`boost::unordered_flat_set`), a tree with `sorted`, a sorted vector with `sorted, frozen`, a bitset
with `dense`; a sequence is a `std::vector`; a map by hashed keys is `boost::unordered_flat_map`, by dense keys an
`IndexMap` (a slot per index); `fifo`, `lifo` and `by_priority` are a deque, a vector and a binary heap; rows and pools
are in [Nesting](#elements-any-type-any-depth).

`Store<…>::representation` names the choice, for tests that pin it; code that uses a Store never needs it.

## Properties

Each property answers one question (a category) with a tag; a tag derived from another promises at least as much,
and the root of a category promises nothing. List them in any order; equal requests are the same type. Containers
declare the guarantees in `container_traits`; the rest only a request states.

| Category | Tags (later ones promise more) | Guarantees |
|---|---|---|
| order | `sorted`, `insertion_ordered` | iteration ascending by `urd::Less`, or in the order elements first came |
| uniqueness | `unique` | no two elements equal (implied for sets, and for maps without `multi`) |
| keys | `dense`, `sparse` | keys are indices 0..n-1 (`urd::index_of`), or few in their range |
| references | `stable_refs` | a reference stays valid until its element is removed |
| layout | `contiguous` | elements next to each other in memory |
| representation | `canonical` | equal contents, equal representations |
| threads | `concurrent` | inserts from several threads at once; for maps also `update` and `read`, for pools the operations they offer |
| memory | `retains`, `reclaims` | storage freed by erase and clear is kept for reuse, or returned |
| clear | `proportional_clear` → `constant_clear` | clear costs O(elements present), or O(1) |
| access | `fifo`, `lifo`, `by_priority` | a queue: reached by front or top, not iterated |
| handles | `handles` | a pool: insert gives a handle; with `unique`, equal rows get equal handles; on a priority queue, push gives a handle to update or erase by |
| sharing | `structural_sharing` | equal parts of different rows are stored once |
| scopes, layering | `scoped`, `layered` | changes undone by `pop_scope`; lookups falling through to a parent |
| tracking | `change_log` | `added()` and `removed()` since `clear_changes()` |
| grouping, direction | `partition`, `bidirectional` | union-find classes; a map answering `keys(value)` too |
| shape | `fixed_shape` | rows whose number and lengths are set at construction (`urd::row_lengths`, or one per row for rows of rows); their contents change in place, and `reset()` sets them back to defaults (with `proportional_clear`, only rows handed out to change since) |
| packing | `flat` | each record of an interning pool is one self-contained block of bytes (trivially copyable, or serialized by cista), frozen once stored and read by `index<Record>` |

| Requested only | Tags | Effect |
|---|---|---|
| lifecycle | `monotone` → `frozen` | only grows (no erase), or changes only by `assign(first, last)`, a rebuild (in the storage held where the container can) |
| emptiness | `nonempty` | at least one element |
| multiplicity | `functional`, `multi` | one value per key, or many (a multimap) |
| identity | `positional` | elements identified by position (sequences, and sets with positions) |
| workload | `small` | a few elements: picks compact containers, never changes a guarantee |
| priority | `monotone_priority` | a priority queue never pushed below the last popped: integral priorities go to buckets |
| order | `order_by<P>` | a sorted Store or priority queue ordered by `P(element)` (`P(key)` in a map), or a map ordered by `P(key, value)`: a member pointer or a stateless lambda; ties by the element's (the key's) own order. Ordered by values, a map's values change only through `update`, `merge`, `insert_or_assign` |

The kind of a Store follows from its arguments: `key<K>` is a set, `key<K>, value<V>` a map, `value<V>` a sequence;
an access tag makes a value Store a queue, `handles` a pool, `partition` a key Store a partition.

## Cost per operation

n elements; U the key range (largest index + 1); r a row's length; k the changes in a delta; A the tree arity (4).
\* amortized (growing the storage); ~ expected (hashing).

**Sets and maps**

| Container | insert | lookup | erase | clear | iterate | memory |
|---|---|---|---|---|---|---|
| `boost::unordered_flat_set/map` | O(1)\*~ | O(1)~ | O(1)~ | O(capacity) | O(capacity) | ~n/0.875 slots + 1 byte each; retains |
| `std::unordered_set/map` | O(1)\*~ | O(1)~ | O(1)~ | O(n + buckets) | O(n) | a node per element; stable references |
| `std::set/map`, multi | O(log n) | O(log n) | O(log n) | O(n) | O(n), in order | a node per element; reclaims |
| `Recycling<Map>` | as Map, a kept node first | as Map | as Map; keeps the node | O(n) | as Map | erased nodes and their values kept for reuse |
| `boost::container::flat_set/map`, multi | O(n) | O(log n) | O(n) | O(n) | O(n), contiguous | n elements; retains |
| `DenseSet` (bitset) | O(1)\* | O(1) | O(1) | O(U/64); O(n) tracking its words | O(U/64 + n), in order | U bits (+ the words set, tracking); set algebra by words |
| `SparseSet` | O(1)\* | O(1) | O(1); O(n) keeping order | O(1) for trivial keys | O(n), contiguous | n keys + U positions |
| `IndexMap` (dense map) | O(1)\* | O(1) | O(1) | O(U); O(1) with generations | O(U), in key order | a slot per index (+ a 1-byte lock, thread-safe) |
| `IndexedSet` | O(1)\*~ | O(1)~ | O(n) (keeps order) | O(capacity) | O(n), contiguous | n elements + a table of positions |
| `OrderedMap` | as its key set | as its key set | O(n) (keeps order) | as its key set | O(n); keys and values as spans | key set + n values |
| `FlatEntryMap` (ordered by values) | O(n) (into place) | O(n) scan | O(n) | O(n) | O(n), in the given order | n entries; for few |
| `PostingMap` (multi) | O(1)\*~ | O(1)~; `values(k)` a span | O(r) per value, O(1)~ per key | O(capacity + n) | O(n) | one vector of values per key |
| `CsrMultiMap` (frozen) | build O(n + U) | O(1); `values(k)` a span | — | — | O(n + U), in key order | n values + U + 1 offsets |
| `ScopedMap` | O(1)\*~ | O(1)~ | O(1)~ | O(capacity) | O(capacity) | + a log entry per change in a scope; `pop_scope` O(changes) |
| `Layered` | O(1)\*~ | O(1)~ own + parent | O(1)~ own | O(capacity) | own elements | the parent is not owned |
| `ChangeLogged` | O(1)\*~ | O(1)~ | O(1)~; O(added) for one added since the reset | O(n) | O(capacity) | + the elements added and removed |
| `ConcurrentIndexedSet` | O(1)\*~ (new ones under a lock) | O(1)~ without a lock | — | O(capacity) | O(n) | segments that never move |
| `gtl::parallel_flat_hash_set/map` | O(1)\*~ under one shard's lock | O(1)~ under it | O(1)~ | O(capacity) | O(capacity) | 16 shards, each an open-addressing table |

**Sequences, queues, partitions**

| Container | push / insert | access | erase | clear | memory |
|---|---|---|---|---|---|
| `std::vector` | O(1)\* at the end, O(n) inside | O(1) | O(n) | O(n) | contiguous; retains |
| `SegmentedVector` | O(1)\* (no element moves) | O(1) | — | O(n) | doubling segments; stable references |
| `Jagged` (frozen rows) | O(r)\* per row | a row: O(1), a span | — | O(n) | one offsets array per level |
| `Jagged` (fixed_shape) | built once, O(total length) | a row: O(1), a span or `BitsetSpan`; bit ops O(r/64) | — | — | one array; bit rows ⌈r/64⌉ words each |
| `Fifo` / `Lifo` | O(1)\* | front / top O(1) | `pop` O(1) | O(n) | deque / vector |
| `Heap` | O(log n) | `top` O(1) | `pop` O(log n) | O(n) | vector; retains |
| `AddressableHeap` | O(log n) | `top` O(1) | `pop`, `erase(h)`, `update(h, e)` O(log n) | O(n) | heap of handles + a slot per handle; retains |
| `BucketQueue` | O(1)\* | `top` O(1) | `pop` O(1)\* under monotone pushes | O(range) | a vector per priority; retains |
| `DisjointSets` | `insert` O(1)\* | `find`, `same`, `merge` O(α(n))\* | — | O(1) | n parents + n sizes |

**Pools** (rows by handle)

| Container | insert a row | insert a delta | read a row | `contains` | memory |
|---|---|---|---|---|---|
| `RowInterner` | O(r)~ | O(r)~ (copies the row) | O(r); `view(h)` O(1), no copy | O(r); O(1) for dense rows | each distinct row once, in full; dense-set rows as bitset words |
| `TreeVectorSet` | O(r)~ | O(k log_A r)~ | O(r) | — | nodes shared across rows: a delta adds about k·log_A r nodes |
| `TreeBitsetSet` | O(r/32)~ | O(k log_A (r/32))~ | O(r/32 + set bits) | O(log_A (r/32)) | as above; all-zero subtrees take nothing |
| `SlotVector` | O(1)\* (a freed slot first) | — | O(1) by handle | — | stable slots; erased elements keep their storage |

Measured, not only bounded: for 20,000 versions of a row that each differ from an earlier one in 3 positions, a tree
pool takes 136–271 bytes per version against 447–4,409 for rows stored in full, and is faster from a thousand
positions on
([docs/BENCHMARKS.md](docs/BENCHMARKS.md)).

## Using it

```sh
cmake -S src -B dependencies-build -DCMAKE_INSTALL_PREFIX="$PWD/dependencies-install" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build dependencies-build && cmake --install dependencies-build    # Boost, cista, gtl, GoogleTest, Google Benchmark
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8 && ctest --test-dir build
cmake --install build --prefix <where>                                    # then find_package(urd) from there
```

Link the `urd::urd` target (`find_package(urd)` after the install, or `urd_DIR=<build>` against the build tree) and
include `urd/store.hpp`. The target carries its dependencies: Boost 1.89 or later (Container, Unordered, Hash2),
cista, and with `URD_WITH_GTL` (on by default) gtl, whose sharded tables serve `concurrent` sets and maps by hashed
keys; without it such a request has no representation. Requests use only the vocabulary of `urd/traits.hpp` and
`urd/properties.hpp`; what `urd/repr` holds is the representations' business, not code that uses a Store (a check
keeps containers out of the include root).

- [examples/](examples/): sets, sequences, maps, rows, dense keys, interning, queues, scopes, pools, and more.
- [docs/INTEGRATION.md](docs/INTEGRATION.md): adding a container (declare its traits, offer what they require,
  register it, test it), and every property and facet in detail.
- [docs/BENCHMARKS.md](docs/BENCHMARKS.md): the measurements behind the choices.
- [docs/SURVEY.md](docs/SURVEY.md): data structures found in existing code, and how each is written as a request.
- [docs/ROADMAP.md](docs/ROADMAP.md): what is done and what is next.
