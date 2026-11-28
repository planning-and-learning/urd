Layout
======

urd has the layout every library of the chain has
(`brokkr: Layout <https://planning-and-learning.github.io/brokkr/directory_layout.html>`__): its public headers are
under ``include/urd/``, one directory per component, and ``src/``, ``tests/unit/`` and ``benchmarks/`` mirror them.
urd is header-only and binds nothing itself: its Python packages ship its headers and the native libraries of the
chain, and its ``python/`` component holds what the libraries above it include to bind their own types. This page
names urd's components, shows the directories of its headers and of its Python packages, and says where its
sources, tests and benchmarks are.

Components
----------

A component is a directory of ``include/urd/``. None is bound to a Python module: urd has no extension module.

.. list-table::
   :header-rows: 1
   :widths: 14 22 20 44

   * - Component
     - Headers
     - Python module
     - What it holds
   * - elements
     - ``include/urd/elements/``
     - not bound
     - What a Store holds and hands out: indices, records (data, builder, views), their children, hashing, equality
       and order, handles, and copying a record between contexts.
   * - registry
     - ``include/urd/registry/``
     - not bound
     - How a request chooses its container: each container's declaration, the tables of declarations by kind, the
       scan that matches tags, and the traits derived from a declaration.
   * - repr
     - ``include/urd/repr/``
     - not bound
     - The containers a request can resolve to (``urd::repr``), by kind, and the building blocks they share.
   * - store
     - ``include/urd/store/``
     - not bound
     - The facets of a Store: one class per group of properties, holding the members those properties switch on.
   * - python
     - ``include/urd/python/``
     - not bound
     - What a library's nanobind bindings include: helpers that bind records, builders and views, and the casters of
       Stores, by the representation they resolve to.
   * - testing
     - ``include/urd/testing/``
     - not bound
     - What tests reach: counting allocations, and what of a container a Store does not offer.

Beside the components, ``include/urd/`` has the headers of the Store and of the request it is described by, and
the operations written once for any set or graph:

.. list-table::
   :header-rows: 1
   :widths: 26 74

   * - Role
     - Headers
   * - The Store
     - ``store.hpp``: a Store, its core and the facets its properties switch on (``store/``), and the headers of
       the container it resolves to.
   * - A request
     - ``spec.hpp`` (a kind with its element types, then tags), ``kinds.hpp`` (the kinds of Store), ``tags.hpp``
       (the vocabulary), ``property_set.hpp`` (what computes with the tags), ``type_traits_of.hpp`` (the traits of
       elements and orders), ``traits.hpp`` (what urd knows about a container, a Store or a request).
   * - Costs and memory
     - ``cost.hpp`` (what each operation costs a representation), ``memory.hpp`` (``urd::memory_usage``).
   * - Generic operations
     - ``sets.hpp`` (on any set), ``graphs.hpp`` (on any graph), ``graph_types.hpp`` (what a graph is built from),
       ``key_range.hpp`` (a window of keys a dense set is built from), ``bits.hpp`` (urd's bit vocabulary for code
       outside urd).
   * - Calls and words
     - ``access.hpp`` (the tags a lookup or an insert passes where threads share a container), ``config.hpp``
       (urd's word, of which bit-packed storage is made).
   * - Output
     - ``format.hpp`` (urd's types with fmt), ``dot.hpp`` (a graph in the DOT language).

Two rules hold across components.

**A container is declared apart from its definition.** In ``repr/``, a ``*_fwd.hpp`` declares containers with what
the registry reads of them (``values/dense_set_fwd.hpp`` the bitset, the sparse set and the slot-per-index map), and
each is defined in a header of its own name (``values/dense_set.hpp``); only the standard library's
(``values/std_set.hpp``, ``maps/std_map.hpp``, ``sequences/sequence.hpp``) are declared where they are defined.
``registry/declarations.hpp`` includes the declarations, so a request is resolved without a container being
defined; the container is defined where its header is included, and a Store whose container's header is not names
the headers to include in its error.

**The Python headers follow the representations.** ``python/<category>/<name>.hpp`` converts the Stores represented by
``repr/<category>/<name>.hpp``: ``python/maps/index_map.hpp`` those of ``repr/maps/index_map.hpp``. A representation
without a header there does not convert.

Headers
-------

.. code-block:: text

   include/urd/
   ├── elements/                   indices, records and their views and builders, children, hashing and order
   ├── registry/                   declarations, tables, the scan and selection of a container, derived traits
   ├── repr/                       the containers a request resolves to
   │   ├── values/                 sets: hashed, sorted, flat, dense, sparse, indexed (interning)
   │   ├── maps/                   maps and multimaps: hashed, sorted, insertion-ordered, by dense key, CSR
   │   ├── sequences/              sequences, segmented vectors, and nested rows (jagged, row pools)
   │   ├── queues/                 first in, last in, least first: heaps and bucket queues
   │   ├── pools/                  pools of rows by handle, and the tree pools that share equal subtrees
   │   ├── records/                interners of records, and the arena interner of records stored flat
   │   ├── slots/                  elements in slots that never move, erased slots reused
   │   ├── partitions/             disjoint sets (union-find)
   │   ├── graphs/                 adjacency lists, compressed sparse rows, rows of bits per vertex and part
   │   ├── wrappers/               layers over a parent, layered and multi-index pools, recycling, scopes
   │   ├── containers/             building blocks: bit manipulation, bitset spans, rows of bits, segmented storage
   │   │   └── detail/             the segment layout, threading and tracking of segmented storage
   │   └── core/                   what several containers share: numbering, iterators, the memory of std, Boost, gtl
   ├── store/                      the facets of a Store
   ├── python/                     nanobind helpers and casters
   │   ├── values/                 casters of the Stores represented by repr/values/
   │   ├── maps/                   casters of the Stores represented by repr/maps/
   │   ├── sequences/              casters of the Stores represented by repr/sequences/
   │   └── containers/             a bitset span, converted to a list of its bits
   └── testing/                    allocation counting, inspection of containers

A container a request can resolve to is in the ``repr/`` directory of its kind (a set in ``values/``, a map or
multimap in ``maps/``); one that adds to a container it is over (a layer, scopes, recycled nodes) is in
``wrappers/``, with the layered and multi-index pools. ``containers/`` and ``core/`` hold no container of their own:
what the containers are built from, and what they share.

Python
------

urd's Python packages are two wheels, neither with an extension module: ``pyurd`` ships urd's headers and CMake
package, and ``pyurd-native`` (``pyurd_native``) the native libraries urd and the libraries above it build with.

.. code-block:: text

   python/
   ├── src/
   │   ├── pyurd/                  pyurd: where urd's headers and CMake package are, and the wheel repair helper
   │   └── pyurd_native/           pyurd_native: where the bundled native dependencies are
   ├── native/                     the native dependencies, built as the pyurd-native wheel
   │   ├── <dependency>/           its CMakeLists.txt, patches, licenses/, package/ where upstream ships none
   │   └── wheel/                  the wheel that ships the prefix the dependencies installed
   └── tests/                      pytest: the build backends, the native wheel, the repair helper

The bindings of urd's types are written by the libraries that use them, with the headers of
``include/urd/python/``; ``tests/integration/python/`` binds test modules with them, as a downstream library does.

Sources, tests and benchmarks
-----------------------------

``src/`` has one directory, ``src/testing/``: urd is header-only, and its one source replaces the global
``operator new`` and ``delete`` for the ``urd::count_allocations`` target (``include/urd/testing/allocations.hpp``).

``tests/`` has the directories of brokkr's layout:

.. code-block:: text

   tests/
   ├── rules/                      checks of a rule over the source tree, and code that must not compile
   ├── laws/                       container properties and the facet surface, for every registered container
   ├── tables/                     generated files (vocabulary, requests, complexity, resolution) checked
   ├── cost/                       compile cost: budget/, mechanism/, overhead/, scaling/, each driven by its script
   ├── unit/                       Stores and containers by example
   │   ├── repr/
   │   │   ├── containers/         bits, bitset spans, the segmented vector
   │   │   └── pools/              the tree pools
   │   └── store/                  Stores of each kind, through store.hpp
   ├── integration/
   │   ├── consumer/               a consumer of the installed package
   │   └── python/                 nanobind test modules and the scripts that drive them
   └── support/                    shared headers and the gtest main
       └── verify/                 urd::verify by kind: random operations checked against a model

The unit tests of ``elements/`` and some of ``repr/`` are at the top of ``tests/unit/`` (``builders.cpp``,
``views.cpp``, ``pools.cpp``), not in the directory of their headers.

``benchmarks/`` has its programs at the top (``containers.cpp``, ``pools.cpp``, ``experiment.cpp``), not by
component, beside ``support/``, ``suites/``, ``results/`` (the results the container choices are based on) and
``tools/``.

Checks
------

brokkr's layout rules decide part of this page (``python -m pybrokkr.rules L1``): every tracked file matches a pattern
of the layout (L1.1); a directory under ``tests/unit/`` and ``benchmarks/`` names a component that exists under
``include/urd/`` or ``src/`` (L1.3), as ``tests/unit/repr/pools/`` names ``include/urd/repr/pools/``; a unit test
includes a header of its component (L1.5), and every source is built and every test script run (L1.7). No check
decides which directory of ``repr/`` a container belongs to, nor that ``python/`` follows ``repr/``: the kind of the
container does, and the casters' lookup by the representation's header name.
