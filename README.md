# urd

**Say what your data is. urd picks the container and checks that it keeps its promises.**

```cpp
#include <urd/store.hpp>
#include <urd/use/dense.hpp>   // the families these Stores resolve to: dense keys,
#include <urd/use/hash.hpp>    // an interner,
#include <urd/use/queues.hpp>  // a queue
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

`User` is an index, as `dense` asks, so `Seen` is a bitset and user 17 is its bit 17.

## What you get

- **Say what, not which.** You write what the data is; urd resolves the request at compile time to a container from
  the standard library, Boost or its own, picked by benchmarks.
- **The interface follows the properties.** A `frozen` Store has no `insert`, an unordered one no `lower_bound`, a
  queue no iteration, and the compiler says why:

  ```
  note: constraints not satisfied: 'denied<"push_back/emplace_back: a keyed Store (a set or a map) takes insert">'
  ```
- **Promises are checked.** Every container urd can choose is compared with a reference model through thousands of
  random operations, and deliberately broken containers are caught by the same tests.
- **Small vocabulary.** About 35 tags, few enough for people and code-writing agents to state instead of
  hand-rolling a structure.

## The language

Tags answer questions, and the questions fall into six themes:

| Theme | It asks | In the example |
|---|---|---|
| [What it is](https://verdog-ai.github.io/urd/language/kind.html) | a queue, a pool, a partition? one value per key or many? | `fifo` (Frontier), `multi`, `bidirectional` (Follows) |
| [What the data is like](https://verdog-ai.github.io/urd/language/data.html) | which keys? duplicates? how many? | `dense` (Seen, Distance, Follows) |
| [Order](https://verdog-ai.github.io/urd/language/order.html) | sorted, insertion order, by position? | `positional` (Names) |
| [Change over time](https://verdog-ai.github.io/urd/language/change.html) | built once, only grows? how is it cleared? | `frozen` (Follows), `monotone` (Names), `constant_clear` (Distance) |
| [Memory and layout](https://verdog-ai.github.io/urd/language/memory.html) | stable references, one array, freed memory? | |
| [Threads](https://verdog-ai.github.io/urd/language/threads.html) | shared between threads? | |

[How the language is organized](https://verdog-ai.github.io/urd/language/index.html) explains themes, questions and tags;
[the index by need](https://verdog-ai.github.io/urd/language/needs.html) starts from plain words ("reset every round", "union find").

## Use it

```sh
cmake -S src -B dependencies-build -DCMAKE_INSTALL_PREFIX="$PWD/dependencies-install" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build dependencies-build && cmake --install dependencies-build    # Boost, cista, gtl, GoogleTest, Google Benchmark
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8 && ctest --test-dir build -j8
cmake --install build --prefix <where>                                    # then find_package(urd) from there
```

Link `urd::urd`, include `urd/store.hpp`, and include `urd/use/<family>.hpp` for the families your Stores resolve to:
a missing one fails to compile with the header's path in the error.

## Documentation

**https://verdog-ai.github.io/urd/**

- [The request language](https://verdog-ai.github.io/urd/requests.html) and [how it is organized](https://verdog-ai.github.io/urd/language/index.html)
- The kinds of Store: [sets](https://verdog-ai.github.io/urd/kinds/sets.html), [maps](https://verdog-ai.github.io/urd/kinds/maps.html),
  [sequences](https://verdog-ai.github.io/urd/kinds/sequences.html), [queues](https://verdog-ai.github.io/urd/kinds/queues.html), [pools](https://verdog-ai.github.io/urd/kinds/pools.html),
  [records](https://verdog-ai.github.io/urd/kinds/records.html), [partitions](https://verdog-ai.github.io/urd/kinds/partitions.html): members and complexity
- [What a request resolves to](docs/RESOLUTION.md), generated from the registry and checked by a test
- [Adding a container](docs/INTEGRATION.md), [benchmarks](docs/BENCHMARKS.md), [roadmap](docs/ROADMAP.md),
  [examples](examples/)

Header-only C++20, GPL-3.0.
