# Adding a representation

A representation is an existing container a `urd::Store` can resolve to: from the standard library, from
another library (Boost's flat sets), or copied in (yggdrasil's segmented vector). urd does not wrap containers
to give them properties; it registers containers that keep their properties by themselves. Downstream code
never names one; it asks for properties, and the registry picks the best representation that has them.
Adding one takes six steps; per container, only its `container_traits` and its place in the registry are written.

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
Building `src/` installs everything into `dependencies-install/`, whose headers urd includes as system
headers:

```sh
cmake -S src -B dependencies-build -DCMAKE_INSTALL_PREFIX="$PWD/dependencies-install" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build dependencies-build -j8 && cmake --install dependencies-build
```

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

Add its template, taking the element type, to both preference lists in `include/urd/registry.hpp` (`detail::preference`): one
for Stores whose elements are few or never change, one for the rest. A container with further template parameters
gets an alias that fixes them (`ConcurrentSegmentedVector`). Nothing else is written per container: a request
takes the first container in its list that serves it, meaning its `container_traits` for the request's
elements satisfy the request and it offers the interface a Store for the request offers.

What a container needs of its elements belongs in its traits, as constraints with the concepts of
`include/urd/elements.hpp` (`orderable`, `hashable`): `std::set<K>` is `sorted` only for orderable `K`, so a
request for a sorted set of elements without `<` is not served by it.

## 6. Prove it

A property is part of what a Store is: it holds from construction (from any elements, or `assume`, which
Debug builds check) and every operation keeps it, or the operation is not offered. A container whose property holds only
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
- **Some containers are not stores.** `UniqueObjectPool` and `SharedObjectPool` hand out handles that own
  recycled objects; they hold no set, map or sequence of contents. They are copied with their tests but not
  registered: they need their own entry point, or an allocation property for other stores, which is still
  open.
- **The vocabulary grew by one property,** `concurrent`, which the copied container already offered as a
  template parameter.

## Maps

`key<K>` with `value<V>` makes a map, `multi` a multimap; the elements are `std::pair<const K, V>`. A map
container names `mapped_type`, which tells the registry it is one; its lookups and `erase` take a key, and
with unique keys it offers `at`, `try_emplace` and `insert_or_assign` (`include/urd/interface.hpp`). The
element requirements (`orderable`, `hashable`) apply to the key. A map Store hands out const iterators and
changes values by key only, since Boost's flat maps would let a key be overwritten through an iterator.

## The Store's facets

`Store` holds only what every Store has: construction, iteration, size, comparison. Each group of properties
has a facet in `include/urd/store/`, a class with the members those properties allow, which the Store
inherits where its properties switch it on: `lookup` (kept by value or key), `ordered_lookup` (`sorted`), `reverse_iteration` (an order: `sorted`, or a sequence),
`positions` (a sequence), `contiguous_data`, `growth`, `removal`, and `values_by_key` (a map with unique keys).
A new property or kind adds a facet and a line in the Store's list of bases. Each member name lives in one
facet only, so that calls are never ambiguous between two; facets take their types from `store_types`, as the
Store is incomplete while they are built. A Store whose facets are off is no larger than its representation.

## Choosing among containers

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
| rows `frozen` | `repr::Jagged`: CSR, one offsets array per level and one array of the innermost elements; three levels are a `Jagged` of a `Jagged` | `urd::View` (`include/urd/view.hpp`): read-only, with the row request's lookups (binary search where sorted) |
| rows and Store `retains` | `repr::RowPool`: removed rows are cleared and kept, so nothing is freed at any level | the inner Store, `Inner&` |
| anything else | `std::vector` of the inner Stores | the inner Store |

`retains` is shallow, about a container's own storage; deep retention is the rows' `memory_category` in
`element_traits`. A View, and references into a row, are valid until the outer Store changes.
`tests/rows.cpp` checks every nested Store against a `std::vector` of `std::vector`s under random inserts
and erases of rows, sorted rows' lookups, and zero allocations when a retaining Store is refilled.
