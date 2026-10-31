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

   using Tags     = Store<key<std::string>, sorted>;
   using Tagged   = Store<key<Item>, value<Tags>>;                          // a map of sorted sets
   using Families = Store<key<Tags>>;                                       // a set of sets (hashed by contents: Tags is sorted)
   using Trips    = Store<key<std::string>, value<Store<value<Point>>>>;    // a map of sequences
   using Layers   = Store<value<Store<key<Cell>, value<Store<value<Item>>>>>>;  // sequences of maps of sequences

Most rows are held as Stores, each owning its storage. Where the outer request says how rows live (``frozen``,
``fixed_shape``, ``retains``, ``handles``), urd stores them together and hands out a row as a view: see
:doc:`kinds/sequences` and :doc:`kinds/pools`.

Kinds
-----

The kind of a Store follows from its arguments:

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Request
     - Kind
     - Page
   * - ``Store<key<K>, ...>``
     - a set
     - :doc:`kinds/sets`
   * - ``Store<key<K>, value<V>, ...>``
     - a map (a multimap with ``multi``)
     - :doc:`kinds/maps`
   * - ``Store<value<V>, ...>``
     - a sequence
     - :doc:`kinds/sequences`
   * - ``Store<value<V>, fifo | lifo | by_priority, ...>``
     - a queue
     - :doc:`kinds/queues`
   * - ``Store<value<Row>, handles, ...>``
     - a pool of rows (``unique``: interned) or of slots
     - :doc:`kinds/pools`
   * - ``Store<value<Record>, handles, unique, flat, ...>``
     - a pool of records stored flat
     - :doc:`kinds/records`
   * - ``Store<key<I>, partition>``
     - a partition of indices
     - :doc:`kinds/partitions`

Rules
-----

A request is checked when the Store type is formed (``include/urd/properties.hpp``); each failure is a static
assertion that names what is wrong.

* Every argument is a property; at most one ``key``, one ``value``, one ``order_by``.
* At most one tag per category: ``dense`` and ``sparse`` contradict, as do ``frozen`` and ``monotone``, two
  workloads.
* What you state is what you get, with what it means: a set, and a map without ``multi``, keep each key once; a
  positional set numbers its keys as they come; a ``frozen`` Store has ``stable_refs``. ``frozen`` or ``fixed_shape``
  Stores offer no insert, ``monotone`` or ``nonempty`` ones no erase.
* A pool (``handles``) holds ``value<V>`` rows reached by handle, not by key; a ``partition`` groups keys, without
  values; ``order_by`` on unique keys needs orderable keys, to break ties.
* Any other request no container serves fails in the registry.

Resolution
----------

The registry (``include/urd/registry/declarations.hpp``) lists the containers in an order backed by :doc:`BENCHMARKS`; the first
whose declared traits satisfy every property of the request, whose demands the request meets, and whose interface
matches the request's kind (kept by value where the request is, unique, by handle, flat, a map, a queue exactly
where the request is), is chosen. A declared guarantee the request left open is free, with one exception: ``dense``
is also a demand, since a container that stores by index needs keys that are indices, so a dense container serves
only a request that promises ``dense`` (a partition promises it). :doc:`RESOLUTION` is generated from the registry
and checked by a test, and a second test checks that no serving candidate demands what a request did not promise.

A request no container serves fails with a static assertion; its instantiation names the request:

.. code-block:: text

   In instantiation of 'no_representation_for<spec<std::string, void, ..., urd::tag::dense, ...>>'
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
header of each container its Stores resolve to, ``urd/repr/<container>.hpp`` (:doc:`representations` lists them),
and a Store whose container's header is missing fails to compile naming the headers to include. No header includes
every container: the record pools alone bring cista's serialization, which only a file holding such a pool should
pay for. The resolution table (``docs/RESOLUTION.md``) names each request's headers. Beside these: ``urd/flat.hpp`` where a record stored flat is written, and ``urd/view.hpp`` where views are made.

This documentation is built with Sphinx:

.. code-block:: sh

   python3 -m venv build/docs-venv && build/docs-venv/bin/pip install -r docs/requirements.txt
   build/docs-venv/bin/sphinx-build -b html docs build/docs/html
