# Adding a representation

A representation is an existing container a `urd::Store` can resolve to: from the standard library, from
another library (Boost's flat sets), or copied in (yggdrasil's segmented vector). urd does not wrap containers
to give them properties; it registers containers that keep their properties by themselves. Downstream code
never names one; it asks for properties, and the registry picks the best representation that has them.
Adding one takes six steps.

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
| `shrinkable` | `clear`, `erase(position)`, `erase(first, last)` |
| `shrinkable`, kept by value | `erase(e)` returning a count |
| `shrinkable`, a sequence | `pop_back`, `insert(position, e)`, `emplace(position, …)` |

urd calls them as free functions, `urd::insert(c, e)`, which use the container's member of that name. The
standard library's containers, Boost's, and urd's own (`SegmentedVector`) offer them as they are; a container
of urd's should be written that way. A library with other names gets an adapter: free functions of the
standard names in its namespace, found by argument-dependent lookup. An adapter only renames; it adds no
guarantee, and a library update that changes its interface breaks the adapter's build, not its behavior.

## 5. Register it: a candidate

A candidate is a template over the request's spec `S` (see `include/urd/repr/segmented.hpp`):

| Member | Says |
|---|---|
| `type` | the representation for `S`; it may depend on `S` (`SegmentedVector` is thread-safe when `concurrent` is requested) |
| `name` | how messages and the catalog name it |
| `accepts` | whether it can represent `S` at all: kind (set, map, sequence) and element types |
| `rank` | how well it suits `S`'s workload, lower is better |

Add it to `detail::registry` in `include/urd/registry.hpp`. A candidate fits a request when it accepts it,
its `container_traits` satisfy the request, and it offers the interface a Store for the request offers; where two fit, the rank decides; on a tie, the earlier one
in the list.

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
- Add the container, and a Store that resolves to it, to `tests/properties.cpp`. Both go through
  `urd::verify` (`include/urd/verify.hpp`): it checks `conforming<R>`, then runs every operation the
  properties require at random (inserts, erases of positions and ranges, clears, assignments where the
  iterators allow them), and after every single step compares the contents, each operation's result and the
  lookups with a reference model and checks every declared property (sorted, unique, nonempty, stable_refs,
  contiguous, canonical), so a property that breaks and later recovers is caught too.
- `concurrent` needs threads: a test that inserts from several threads and checks every element arrived,
  run under ThreadSanitizer (`tests/store.cpp`).
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

## Your own container

Steps 3, 4 and 6 work for any container, without registering it: specialize `urd::container_traits`, offer
the required operations (directly or through an adapter), and check both with `urd::verify`. It then meets
urd's concepts (`urd::Sorted<R>`, …), which require the operations too, so generic code can call them.
`examples/custom_container.cpp` does this for a library's journal with its own names. Making it a candidate a
`Store` can pick needs a registry users can extend, which is not there yet.
