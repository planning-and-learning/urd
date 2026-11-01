The request language
====================

.. cpp:namespace:: urd

A Store is a request: the element types and a list of property tags, in any order.

.. cpp:class:: template<typename... Properties> Store

   An alias (``urd/store.hpp``) for the Store class resolved from the request ``Properties``: among them exactly
   one :cpp:class:`key` or :cpp:class:`value`, or both, and property tags from :doc:`language/index`, at most one per
   category. Equal requests, in whatever order, are the same type. ``Store<...>::representation`` names the
   container chosen, for tests that pin it; code that uses a Store never needs it.

.. cpp:class:: template<typename K> key

   The elements are identified by keys of type ``K``. Without :cpp:class:`value`, the keys are the contents: a
   set. ``K`` must be :cpp:concept:`hashable` (the default), :cpp:concept:`orderable` (with ``sorted``) or
   :cpp:concept:`indexable` (with ``dense``); see :doc:`semantics`.

.. cpp:class:: template<typename V> value

   Each element carries a value of type ``V``. Without :cpp:class:`key`, values are identified by position (a
   sequence), by the order they leave (a queue), or by a handle (a pool). ``V`` has no requirements of its own.

.. cpp:class:: template<auto P> order_by

   Orders a ``sorted`` Store or a ``by_priority`` queue by ``P(element)`` (``P(key)`` in a map) instead of the
   element, or a map by ``P(key, value)``: a member pointer (``&Job::deadline``) or a stateless lambda. Ties fall
   back to the element's own order where it has one. A map ordered by its values changes its values only
   through ``update``, ``merge`` and ``insert_or_assign``.

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

The kind of a Store is the first kind of ``include/urd/kinds.hpp`` that claims its request (:doc:`language/kind`):

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
   * - ``Store<sequence<V>, sorted | unique | multi, ...>``
     - a multiset: values kept by value
     - :doc:`kinds/sequences`
   * - ``Store<sequence<V>, fifo | lifo, ...>``
     - a queue by arrival
     - :doc:`kinds/queues`
   * - ``Store<queue<V>, by_priority, ...>``
     - a priority queue
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
* A queue is reached by ``fifo``, ``lifo`` or ``by_priority``; ``order_by`` on unique keys needs orderable keys, to
  break ties.
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
