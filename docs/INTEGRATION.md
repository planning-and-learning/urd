# Adding a representation

A representation is any data structure a `urd::Store` can resolve to: one from the standard library, a
copied or hand-written container, or a wrapper around another library. Downstream code never names it; it
asks for properties, and the registry picks the best representation that has them. Adding one takes five
steps.

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

## 3. Adapt it: `repr::ops<R>`

Store calls every representation the same way, through a specialization of `urd::repr::ops` for its type
(see `include/urd/repr/ops.hpp`): `make` and `assume` build it from a `std::vector` of elements, `insert`
adds one, `contains` looks one up (sets), and `erase` removes one, only for representations that can.

## 4. Register it: a candidate

A candidate is a template over the request's spec `S` (see `include/urd/repr/segmented.hpp`):

| Member | Says |
|---|---|
| `type` | the representation for `S`; it may depend on `S` (`SegmentedVector` is thread-safe when `concurrent` is requested) |
| `name` | how messages and the catalog name it |
| `accepts` | whether it can represent `S` at all: kind (set, map, sequence) and element types |
| `erases` | whether `ops<type>::erase` exists; a request that allows erasing only resolves to representations that can |
| `guarantees` | the properties every value of `type` has, exactly; claim only what holds for the `type` chosen |
| `rank` | how well it suits `S`'s workload, lower is better |

Add it to `detail::registry` in `include/urd/registry.hpp`. Where two candidates fit, the rank decides; on
a tie, the earlier one in the list.

## 5. Prove it

- Pin what requests now resolve to it (`static_assert` on `Store<…>::representation` in `tests/store.cpp`).
- For every property in `guarantees`, a test that would fail without it: `stable_refs` takes an element's
  address, appends 100 000 more and checks the address; `concurrent` inserts from several threads and
  checks every element arrived, and runs under ThreadSanitizer.
- Run it against a reference container under random operations (as the set representations run against
  `std::set`).
- A request it makes possible, and one that should still fail, as compile-fail cases.

## What the first tier showed

- **Registration needs operations, not only properties.** `SegmentedVector` keeps references stable but
  cannot erase from the middle. Without `erases`, `Store<value<T>, stable_refs>` would have resolved to it
  and offered an `erase` it cannot do. Now that request finds no representation (a compile-time error), and
  `Store<value<T>, stable_refs, monotone>` finds it.
- **Some containers are not stores.** `UniqueObjectPool` and `SharedObjectPool` hand out handles that own
  recycled objects; they hold no set, map or sequence of contents. They are copied with their tests but not
  registered: they need their own entry point, or an allocation property for other stores, which is still
  open.
- **The vocabulary grew by one property,** `concurrent`, which the copied container already offered as a
  template parameter.
