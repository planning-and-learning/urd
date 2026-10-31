How the language is organized
=============================

A request is a list of tags. Each tag answers a question about the data, and the questions fall into four groups,
told apart by one test: **what does leaving out a tag you need cause?**

.. list-table::
   :header-rows: 1
   :widths: 18 52 30

   * - Group
     - Leaving out a tag you need
     - For example
   * - :doc:`kind`
     - does not compile: the operation does not exist
     - ``fifo``, ``handles``, ``partition``
   * - :doc:`semantics`
     - is a bug: the program relies on something nobody keeps
     - ``sorted``, ``stable_refs``, ``dense``, ``frozen``
   * - :doc:`costs`
     - is slower by a complexity class
     - ``constant_clear``, ``retains``
   * - :doc:`hints`
     - is slower by a constant factor, and never changes a guarantee
     - ``small``

The test is about leaving a tag out. Asking for a tag you do not need only costs speed, whatever its group. Within
the groups:

- **A category** is one question, such as *Which keys?* (semantics). A request answers each category at most once:
  ``dense`` and ``sparse`` both answer *Which keys?*, so a request holds one of them. A category left unanswered
  stays at its **root**, which promises nothing: urd may pick anything there.
- **A tag** is one answer to a category. Within a category some answers promise more than others:
  ``constant_clear`` (clear in O(1)) is at least ``proportional_clear`` (clear in O(elements present)), so a
  container that clears in O(1) also serves a request for ``proportional_clear``. Each category's diagram draws
  this: an arrow points to an answer that promises at least as much.

Take ``Distance`` from the example on the :doc:`front page </index>`:

.. code-block:: cpp

   using Distance = Store<key<User>, value<int>, dense, constant_clear>;

.. list-table::
   :header-rows: 1
   :widths: 18 32 50

   * - Group
     - Category
     - Answer, and what leaving it out would cause
   * - Kind
     - (from the arguments)
     - ``key<User>, value<int>``: a map. Without ``value<int>`` it is a set, and ``distance[u]`` does not compile
   * - Semantics
     - *Which keys?*
     - ``dense``: the users are ids 0..n-1, which you promise. Left out, the Store is still correct, only no longer a
       slot per user; stated falsely, it is a bug
   * - Costs
     - *How is it cleared?*
     - ``constant_clear``: in O(1). Left out, ``clear`` between searches may take time in proportion to the users
       reached, a complexity class more over many searches
   * - Hints
     - (none)
     - nothing to say about the workload
   * - Every other category
     - (unanswered)
     - its root: no promise

urd picks the cheapest container that keeps every answer: here a map with a generation counter per slot
(``generation_map``). A Store has the properties it was asked for, not those of its representation: a set that
``reclaims`` resolves to a tree, which is sorted, but was not asked to be, so it is not.

Who keeps an answer
-------------------

Tags also differ in who makes the promise, shown on each tag's card. The role follows from the group, and in
semantics from who keeps the tag:

- a **kind** makes the Store a queue, a pool or a partition (``fifo``, ``handles``, ``partition``);
- a **guarantee** is kept by the container (``sorted``, ``stable_refs``, and the costs); containers declare their
  guarantees in ``container_traits``, and the typed tests check them against a reference model;
- a **demand** is a promise you make about the data (``dense``: the keys really are indices);
- a **request** only a request states, and the Store's interface enforces it (``frozen``, ``positional``);
- a **hint** (``small``) changes the choice, never a guarantee.

A request is what it states, and what that means (``urd::meaning`` in ``include/urd/tags.hpp``): a set, and a map
without ``multi``, keep each key once; a positional set numbers its keys as they come; a frozen Store's references
are stable. What the Store's interface offers follows from ``frozen``, ``monotone``, ``nonempty`` and
``fixed_shape``. Each container declares everything it guarantees, and what it relies on: one that never erases relies
on ``monotone``, one built once on ``frozen``.

The groups
----------

.. urd-groups::

Each group's page holds its categories, and each category its tags. To start from a need in plain words instead,
see :doc:`needs`.

.. toctree::
   :maxdepth: 1

   kind
   semantics
   costs
   hints
   needs

Order by a projection
---------------------

``order_by<P>`` is no tag: it orders a ``sorted`` Store or a ``by_priority`` queue by ``P(element)`` (``P(key)`` in
a map), or a map by ``P(key, value)``. ``P`` is a member pointer or a stateless lambda; ties are broken by the
element's (the key's) own order. Ordered by values, a map's values change only through ``update``, ``merge`` and
``insert_or_assign``, and the request says ``small`` or ``frozen`` too (a flat scan). See :cpp:class:`urd::order_by`.

Incompatible combinations
-------------------------

A request is one of three things: served, missing, or incompatible. :doc:`/RESOLUTION` shows the first two for a
grid of requests (a ``none`` row carries a note saying what is missing). ``to_spec`` rejects only what is
ill-formed whatever container would hold it; the ``handles``, ``partition`` and ``order_by`` ties rows share one
message (``well_formed`` in ``include/urd/tags.hpp``). A compile-fail case in ``tests/compile_fail_store.cpp`` pins
each message (``urd_compile_fail_expects`` in ``CMakeLists.txt``).

.. list-table::
   :header-rows: 1
   :widths: 30 42 20 8

   * - Request
     - Why
     - Instead
     - Case
   * - An argument that is no property
     - Every argument is a ``key<K>``, a ``value<V>``, a tag or ``order_by<P>``.
     - Wrap the type in ``key`` or ``value``.
     - 9
   * - Two ``key<K>`` (or two ``value<V>``)
     - A Store has one key type and one value type.
     - A nested Store, or a tuple key.
     - 8
   * - Two tags of one category: ``dense, sparse``; ``retains, reclaims``; ``frozen, monotone``;
       two workloads
     - One question, two answers.
     - Keep one.
     - 4, 5, 13
   * - ``handles`` with ``key<K>``
     - A pool holds ``value<V>`` rows reached by handle, not by key.
     - ``key<K>, positional, monotone`` for an interner of keys.
     - 24
   * - ``partition`` with ``value<V>``
     - A partition groups keys.
     - A map from element to group.
     - 39
   * - ``order_by<P>`` on unique keys that are not orderable
     - Ties under ``P`` are broken by the keys' own order; without one, two keys with equal ``P(key)`` would be one.
     - Give the key an order, or ask for ``multi``.
     - 25
   * - ``order_by<P>`` with a ``P`` that fits neither ``P(element)`` nor a map's ``P(key, value)``
     - ``P`` takes the element (a map's key), or a map's key and value.
     - Fix ``P``'s signature.
     - 27

Every other request no container serves is rejected by the registry, with one message: "no container serves
this request: drop or change a tag". Some examples:

.. list-table::
   :header-rows: 1
   :widths: 30 42 20 8

   * - Request
     - Why
     - Instead
     - Case
   * - ``contiguous, stable_refs`` without ``frozen``
     - Contiguous storage moves its elements when it grows.
     - Add ``frozen``, or drop ``contiguous``.
     - 6
   * - ``sorted, concurrent``
     - No concurrent sorted container is registered.
     - Drop ``sorted`` and synchronize outside, or ask for ``dense``.
     - 41
   * - ``key<std::string>, dense``
     - Dense keys are indices; a string is none.
     - A key with ``index_of``, or ``sparse``.
     - 7
   * - ``key<Store<…>>`` where the inner Store is unordered, or ``dense`` without ``sorted``
     - An unordered Store iterates in an order its contents do not fix, so it has no hash.
     - ``sorted`` or ``canonical`` on the inner Store.
     - 28, 29
   * - ``handles, layered`` on a pool of slots
     - Slots are recycled; nothing continues their handles over a parent.
     - ``handles, unique, monotone, layered`` (an interning pool).
     - 36
   * - ``handles, unique, structural_sharing, layered``
     - A tree pool numbers nodes, not rows; no layer continues its handles.
     - Drop ``structural_sharing``.
     - 37
   * - ``dense, layered, concurrent`` maps
     - Missing, not incompatible: the concurrent slot directory is not written yet (see the note in :doc:`/RESOLUTION`).
     - ``dense, layered`` single-threaded, or ``dense, constant_clear, concurrent`` without layers.
     - 38
   * - ``key<int>, value<int>, concurrent, retains``
     - The concurrent tables (gtl) return their storage on ``clear``; no concurrent table retains.
     - Drop ``retains``, or share a ``dense, constant_clear, concurrent`` map.
     - grid
