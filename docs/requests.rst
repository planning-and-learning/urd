The request language
====================

.. cpp:namespace:: urd

A Store is a request: the element types and a list of property tags, in any order.

.. cpp:class:: template<typename... Properties> Store

   An alias (``urd/store.hpp``) for the Store class resolved from the request ``Properties``: among them exactly
   one :cpp:class:`key` or :cpp:class:`value`, or both, and property tags from :doc:`properties`, at most one per
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
* At most one tag per category: ``dense`` and ``sparse`` contradict, as do ``functional`` and ``multi``, ``frozen``
  and ``monotone``, two workloads.
* The rules add what the request implies: a sequence is ``positional`` and ``dense`` (positions are 0..n-1); a
  set, and a map without ``multi``, is ``unique``; a ``frozen`` Store has ``stable_refs``; ``frozen`` or
  ``fixed_shape`` makes a Store immutable, ``monotone`` or ``nonempty`` makes it growable, else it is shrinkable.
* A pool (``handles``) holds ``value<V>`` rows, is reached by handle, and cannot be ``positional`` or ``frozen``; a
  pool of records that are no range must say ``flat``; ``flat`` goes with ``handles, unique`` only.
* A queue changes by ``push`` and ``pop`` and cannot be ``frozen`` or ``positional``; ``monotone_priority`` needs
  ``by_priority``.
* ``fixed_shape`` is a sequence of rows (``value<Store<...>>``) that is not ``monotone``.
* ``contiguous`` with ``stable_refs`` needs ``frozen``: contiguous storage moves its elements when it grows; a
  sequence or a ``positional`` set with ``stable_refs`` needs ``monotone`` or ``frozen``: erasing or inserting at a
  position moves the elements after it.
* ``nonempty`` is a promise about iterated elements: sets, maps and sequences, not queues or pools.
* ``concurrent`` goes with hashed or ``dense`` Stores: no concurrent sorted container, no concurrent queue (its
  front moves with ``pop``), and a ``frozen`` Store is read by any number of threads without it.
* ``order_by`` needs ``sorted`` or ``by_priority``; on unique keys the keys must be orderable, to break ties.

Resolution
----------

The registry (``include/urd/registry.hpp``) lists the containers in an order backed by :doc:`BENCHMARKS`; the first
whose declared traits satisfy every property of the request, whose demands the request meets, and whose interface
matches the request's kind (kept by value where the request is, unique, by handle, flat, a map, a queue exactly
where the request is), is chosen. A declared guarantee the request left open is free, with one exception: ``dense``
is also a demand, since a container that stores by index needs keys that are indices, so a dense container serves
only a request that promises ``dense`` (a partition promises it). :doc:`RESOLUTION` is generated from the registry
and checked by a test, and a second test checks that no serving candidate demands what a request did not promise.

A request no container serves fails with a static assertion whose instantiation names the culprits: the stated
tags whose removal, one at a time, would make it servable, and whether the key type is hashable, orderable and
indexable.

.. code-block:: text

   In instantiation of 'no_representation_for<spec<std::string, void, ..., urd::tag::dense, ...>, culprits<urd::tag::dense>, true, true, false>'
   error: static assertion failed: urd::Store: no representation provides this combination of properties. ...

A call to an operation the properties rule out names the property:

.. code-block:: text

   error: no matching function for call to 'insert'
   note: constraints not satisfied: 'denied<"insert: the Store is frozen; it changes only by assign(first, last)">'

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
Hash2), cista, and with ``URD_WITH_GTL`` (on by default) gtl, whose sharded tables serve ``concurrent`` sets and maps
by hashed keys. ``urd/flat.hpp`` is included where records stored flat are defined, ``urd/view.hpp`` where views
are made, ``urd/compose/family.hpp`` for families.

This documentation is built with Sphinx:

.. code-block:: sh

   python3 -m venv build/docs-venv && build/docs-venv/bin/pip install -r docs/requirements.txt
   build/docs-venv/bin/sphinx-build -b html docs build/docs/html
