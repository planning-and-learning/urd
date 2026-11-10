# urd

**Say what your data is. urd picks the container and checks that it keeps its promises.**

```cpp
#include <urd/store.hpp>
#include <urd/repr/values/indexed.hpp>            // the containers these Stores resolve to: an interner,
#include <urd/repr/maps/bidirectional_csr.hpp>  // a graph by index both ways,
#include <urd/repr/values/dense_set.hpp>          // a bitset,
#include <urd/repr/queues/queue.hpp>              // a queue,
#include <urd/repr/maps/index_map.hpp>          // a slot per index
using namespace urd;

struct User                                                             // an index: one integral member
{
    std::uint32_t id;
    static constexpr auto identifying_members() noexcept { return Members<&User::id> {}; }
};

using Names    = Store<set<std::string>, positional, monotone>;         // an interner: each name gets an index
using Follows  = Store<map<User, User>, multi, dense, frozen, bidirectional>;  // a frozen graph
using Seen     = Store<set<User>, dense>;                               // a bitset
using Frontier = Store<queue<User>, fifo>;                              // a queue
using Distance = Store<map<User, int>, dense, constant_clear>;   // cleared per search in O(1)

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
- **You pay only for what you use.** A Store is the chosen container plus inline forwarding: no virtual calls, and no
  header of a container you do not use is compiled. Forget the one your Store resolves to, and the compiler names it:

  ```
  error: static assertion failed: urd::Store: this file does not include the headers that define the container its
  request resolves to. #include each header urd::include<"..."> names in the mp_list above, in that order
  ... boost::mp11::mp_list<urd::include<"urd/repr/values/dense_set.hpp">> ...
  ```
- **Promises are checked.** Every container urd can choose is compared with a reference model through thousands of
  random operations, and deliberately broken containers are caught by the same tests. Its time and its memory
  (`urd::memory_usage(store)`) are measured against what it declares.
- **Small vocabulary.** About 35 tags, few enough for people and code-writing agents to state instead of
  hand-rolling a structure.

## The language

Tags answer questions, and the questions fall into four groups, by what leaving out a tag you need causes:

| Group | Leaving it out | In the example |
|---|---|---|
| [Kind](https://planning-and-learning.github.io/urd/language/kind.html) | does not compile | `fifo` (Frontier) |
| [Semantics](https://planning-and-learning.github.io/urd/language/semantics.html) | is a bug | `positional`, `monotone` (Names); `multi`, `frozen`, `bidirectional` (Follows); `dense` (Seen, Distance, Follows) |
| [Costs](https://planning-and-learning.github.io/urd/language/costs.html) | is slower by a complexity class | `constant_clear` (Distance) |
| [Hints](https://planning-and-learning.github.io/urd/language/hints.html) | is slower by a constant factor | |

[How the language is organized](https://planning-and-learning.github.io/urd/language/index.html) explains groups, questions and tags;
[the index by need](https://planning-and-learning.github.io/urd/language/needs.html) starts from plain words ("reset every round", "union find").

## Use it

```sh
cmake -S src -B dependencies-build -DCMAKE_INSTALL_PREFIX="$PWD/dependencies-install" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build dependencies-build && cmake --install dependencies-build    # Boost, cista, gtl, GoogleTest, Google Benchmark
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8 && ctest --test-dir build -j8
cmake --install build --prefix <where>                                    # then find_package(urd) from there
```

Link `urd::urd`, include `urd/store.hpp`, and include the header of each container your Stores resolve to: a missing
one fails to compile with the headers to include in the error.

## Documentation

**https://planning-and-learning.github.io/urd/**

- [The request language](https://planning-and-learning.github.io/urd/requests.html) and [how it is organized](https://planning-and-learning.github.io/urd/language/index.html)
- The kinds of Store: [sets](https://planning-and-learning.github.io/urd/kinds/sets.html), [maps](https://planning-and-learning.github.io/urd/kinds/maps.html),
  [sequences](https://planning-and-learning.github.io/urd/kinds/sequences.html), [queues](https://planning-and-learning.github.io/urd/kinds/queues.html), [pools](https://planning-and-learning.github.io/urd/kinds/pools.html),
  [records](https://planning-and-learning.github.io/urd/kinds/records.html), [partitions](https://planning-and-learning.github.io/urd/kinds/partitions.html): members and complexity
- [What a request resolves to](docs/RESOLUTION.md), generated from the registry and checked by a test
- [Benchmarks](docs/BENCHMARKS.md), [examples](examples/)

Header-only C++20, GPL-3.0.
