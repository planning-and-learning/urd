The request language
====================

.. cpp:namespace:: urd

A Store is a request: its kind with its element types, then a list of property tags, in any order.

.. cpp:class:: template<typename Kind, typename... Tags> Store

   An alias (``urd/store.hpp``) for the Store class resolved from the request: ``Kind`` is one of the kinds below with
   its element types, and ``Tags`` are property tags from :doc:`language/index`, at most one per category. Equal
   requests, in whatever order, are the same type. ``Store<...>::representation`` names the container chosen, for
   tests that pin it; code that uses a Store never needs it.

.. cpp:class:: template<typename K> set
.. cpp:class:: template<typename K, typename V> map
.. cpp:class:: template<typename V> multiset
.. cpp:class:: template<typename V> sequence
.. cpp:class:: template<typename V> queue
.. cpp:class:: template<typename V> pool
.. cpp:class:: template<typename V> slots
.. cpp:class:: template<typename K> partition

   The kinds (``urd/kinds.hpp``), each with its element types: keys ``K`` identify elements, values ``V`` are
   carried. ``K`` must be :cpp:concept:`hashable` (the default), :cpp:concept:`orderable` (with ``sorted``) or
   :cpp:concept:`indexable` (with ``dense``); see :doc:`semantics`. ``V`` has no requirements of its own.

.. cpp:class:: template<typename Compare> order_by

   A tag that orders a ``sorted`` Store or a ``by_priority`` queue by ``Compare`` instead of :cpp:class:`Less`:
   ``Compare`` compares two elements (in a map, two keys), as ``std::set``'s comparator does. The order is ``Compare``
   and nothing else: two elements it does not separate are equivalent, so a unique Store keeps one of them.

.. cpp:class:: template<auto P> by

   A comparator by ``P(element)`` under :cpp:class:`Less`, for ``order_by<by<&Job::deadline>>``: ``P`` is a member
   pointer or a stateless lambda. Name what tells elements apart in ``P`` (``std::tuple(job.deadline, job.id)``) to
   keep elements with equal deadlines distinct.

.. cpp:struct:: row_lengths

   What a ``fixed_shape`` Store is built from: how long each row is, in order.

Nesting
-------

A Store of Stores is written as one: an element, key or value can be another Store, to any depth.

.. code-block:: cpp

   using Tags     = Store<set<std::string>, sorted>;
   using Tagged   = Store<map<Item, Tags>>;                          // a map of sorted sets
   using Families = Store<set<Tags>>;                                       // a set of sets (hashed by contents: Tags is sorted)
   using Trips    = Store<map<std::string, Store<sequence<Point>>>>;    // a map of sequences
   using Layers   = Store<sequence<Store<map<Cell, Store<sequence<Item>>>>>>;  // sequences of maps of sequences

Most rows are held as Stores, each owning its storage. Where the outer request says how rows live (``frozen``,
``fixed_shape``, ``retains``, ``handles``), urd stores them together and hands out a row as a view: see
:doc:`kinds/sequences` and :doc:`kinds/pools`.

Kinds
-----

The kind of a Store is its first argument (:doc:`language/kind`):

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Request
     - Kind
     - Page
   * - ``Store<set<K>, ...>``
     - a set
     - :doc:`kinds/sets`
   * - ``Store<map<K, V>, ...>``
     - a map (a multimap with ``multi``)
     - :doc:`kinds/maps`
   * - ``Store<sequence<V>, ...>``
     - a sequence
     - :doc:`kinds/sequences`
   * - ``Store<multiset<V>, ...>``
     - a multiset: values kept by value
     - :doc:`kinds/sequences`
   * - ``Store<queue<V>, fifo | lifo | by_priority, ...>``
     - a queue, by arrival or by priority
     - :doc:`kinds/queues`
   * - ``Store<pool<V>, ...>``
     - a pool: rows interned as rows, records and values interned whole (``flat``: records stored as bytes)
     - :doc:`kinds/pools`, :doc:`kinds/records`
   * - ``Store<slots<V>, ...>``
     - slots
     - :doc:`kinds/pools`
   * - ``Store<partition<I>>``
     - a partition of indices
     - :doc:`kinds/partitions`

Rules
-----

A request is checked when the Store type is formed (``include/urd/spec.hpp``); each failure is a static
assertion that names what is wrong.

* The first argument is the kind with its element types (``set<K>``, ``map<K, V>``, …); every other is a tag.
* At most one tag per category: ``dense`` and ``sparse`` contradict, as do ``frozen`` and ``monotone``, two
  workloads.
* What you state is what you get, with what it means: a set, and a map without ``multi``, keep each key once; a
  positional set numbers its keys as they come; a ``frozen`` Store has ``stable_refs``. ``frozen`` or ``fixed_shape``
  Stores offer no insert, ``monotone`` or ``nonempty`` ones no erase.
* A queue is reached by ``fifo``, ``lifo`` or ``by_priority``; ``order_by`` orders a ``sorted`` Store or a
  ``by_priority`` queue.
* Every tag answers a question of the request's kind (its request format): ``bidirectional`` means nothing for a
  queue, ``flat`` nothing for a set.
* Any other request no container serves fails in the registry.

Resolution
----------

The request's kind is found first (:doc:`language/kind`); then only that kind's containers are tried
(``declarations_of<Kind>`` in ``include/urd/registry/declarations.hpp``), in an order backed by :doc:`BENCHMARKS`. The
first whose declaration provides every property of the request, whose reliances the request promises (what it
states, and the type traits of its elements, order, values and rows), and whose demands the request meets, is chosen;
where the request is ``frozen`` or ``small``, containers with stable storage come after the others (``prefers`` of ``any_kind``
in ``include/urd/kinds.hpp``). A declared guarantee the request left open is free, with one exception: ``dense``
is also a demand, since a container that stores by index needs keys that are indices, so a dense container serves
only a request that promises ``dense`` (a partition promises it). :doc:`RESOLUTION` is generated from the registry
and checked by a test, and a second test checks that no serving candidate demands what a request did not promise.

A request no container serves fails with a static assertion; its instantiation names the request:

.. code-block:: text

   In instantiation of 'no_representation_for<spec<std::string, void, void, urd::uniqueness::unique, urd::keys::dense>>'
   error: static assertion failed: urd::Store: no container serves this request: drop or change a tag

A call to an operation the properties rule out is a plain compile error: the Store has no such member.

.. _building:

Building and using
------------------

.. code-block:: sh

   cmake -S src -B dependencies-build -DCMAKE_INSTALL_PREFIX="$PWD/dependencies-install" -DCMAKE_INSTALL_LIBDIR=lib
   cmake --build dependencies-build && cmake --install dependencies-build    # Boost, cista, gtl, GoogleTest, Google Benchmark
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j8 && ctest --test-dir build -j8
   cmake --install build --prefix <where>                                    # then find_package(urd) from there

Link ``urd::urd`` and include ``urd/store.hpp``. The target carries Boost 1.89 or later (Container, Unordered,
Hash2), cista, and gtl, whose sharded tables serve ``concurrent`` sets and maps by hashed keys.

``urd/store.hpp`` declares every representation and defines none: it costs the vocabulary and the resolution, and
stays as cheap as containers are added: a request names only the container it resolves to. A file includes the
header of each container its Stores resolve to, ``urd/repr/<kind>/<container>.hpp`` (:doc:`representations` lists
them; the kinds' directories are sequences, values, maps, queues, pools, slots, records and partitions, and wrappers
holds the wrappers),
and a Store whose container's header is missing fails to compile naming the headers to include. No header includes
every container: the record pools alone bring cista's serialization, which only a file holding such a pool should
pay for. The resolution table (``docs/RESOLUTION.md``) names each request's headers. Beside these: ``urd/elements/flat.hpp`` where a record stored flat is written, and ``urd/elements/view.hpp`` where views are made.

This documentation is built with Sphinx:

.. code-block:: sh

   python3 -m venv build/docs-venv && build/docs-venv/bin/pip install -r docs/requirements.txt
   build/docs-venv/bin/sphinx-build -b html docs build/docs/html
