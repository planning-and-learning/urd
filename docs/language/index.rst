How the language is organized
=============================

A request is a list of tags. Each tag answers a question about the data, and the questions are grouped in two
levels:

- **A theme** is an area you may care about: what the Store is, what the data is like, order, change over time,
  memory and layout, threads. Themes only organize; they have no rules.
- **A category** is one question within a theme, such as *Which keys?* (theme: what the data is like). A request
  answers each category at most once: ``dense`` and ``sparse`` both answer *Which keys?*, so a request holds one of
  them. A category left unanswered stays at its **root**, which promises nothing: urd may pick anything there.
- **A tag** is one answer to a category. Within a category some answers promise more than others:
  ``constant_clear`` (clear in O(1)) is at least ``proportional_clear`` (clear in O(elements present)), so a
  container that clears in O(1) also serves a request for ``proportional_clear``. Each category's diagram draws
  this: an arrow points to an answer that promises at least as much.

Take ``Distance`` from the example on the :doc:`front page </index>`:

.. code-block:: cpp

   using Distance = Store<key<User>, value<int>, dense, constant_clear>;

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Theme
     - Category
     - Answer
   * - What it is
     - *Values per key?*
     - ``functional``, implied: a map without ``multi`` holds one value per key
   * - What the data is like
     - *Which keys?*
     - ``dense``: the users are ids 0..n-1
   * - Change over time
     - *How is it cleared?*
     - ``constant_clear``: in O(1)
   * - Every other category
     - (unanswered)
     - its root: no promise

urd picks the cheapest container that keeps every answer: here a map with a generation counter per slot
(``generation_map``). A Store has the properties it was asked for, not those of its representation: a set that
``reclaims`` resolves to a tree, which is sorted, but was not asked to be, so it is not.

Who keeps an answer
-------------------

Tags also differ in who makes the promise, shown on each tag's card:

- a **guarantee** is kept by the container (``sorted``, ``stable_refs``); containers declare their guarantees in
  ``container_traits``, and the typed tests check them against a reference model;
- a **demand** is a promise you make about the data (``dense``: the keys really are indices);
- a **kind** makes the Store a queue, a pool or a partition (``fifo``, ``handles``, ``partition``);
- a **request** only a request states (``frozen``, ``positional``);
- a **hint** (``small``) changes the choice, never a guarantee.

The themes
----------

.. urd-themes::

Each theme's page holds its categories, and each category its tags. To start from a need in plain words instead,
see :doc:`needs`.

.. toctree::
   :maxdepth: 1

   kind
   data
   order
   change
   memory
   threads
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
grid of requests (a ``none`` row carries a note saying what is missing). The third is a contract no representation
can keep: ``Store`` rejects it before resolution with a message naming the rule, and a compile-fail case in
``tests/compile_fail_store.cpp`` pins each message (``urd_compile_fail_expects`` in ``CMakeLists.txt``). The rules
are the ``static_assert`` s of ``to_spec`` in ``include/urd/properties.hpp``.

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
     - 10
   * - Two ``key<K>`` (or two ``value<V>``)
     - A Store has one key type and one value type.
     - A nested Store, or a tuple key.
     - 9
   * - Two tags of one category: ``dense, sparse``; ``functional, multi``; ``retains, reclaims``; ``frozen, monotone``;
       two workloads
     - One question, two answers.
     - Keep one.
     - 4, 5, 14
   * - ``key<K>, value<V>, positional``
     - Positions identify a sequence's or set's elements; a map's are identified by key.
     - Drop ``positional`` (a map), or drop ``value<V>`` (an interner).
     - 7
   * - ``contiguous, stable_refs`` without ``frozen``
     - Contiguous storage moves its elements when it grows.
     - Add ``frozen``, or drop ``contiguous`` (a node or segmented container).
     - 6
   * - ``value<V>, stable_refs`` or ``positional, stable_refs`` without ``monotone``
     - Erasing or inserting at a position moves the elements after it.
     - Add ``monotone`` (a segmented vector, an interner over segments) or ``frozen``.
     - 13, 50
   * - ``sorted, concurrent`` without ``dense``
     - No concurrent sorted container: threads share a hashed table or a dense Store (a slot per index is in key
       order).
     - Drop ``sorted`` and synchronize outside, or ask for ``dense``.
     - 48
   * - A queue with ``concurrent``
     - A queue is reached by its front or top, which ``pop`` moves.
     - Guard the queue with a mutex, or share a ``positional, monotone`` set.
     - 49
   * - ``frozen, concurrent``
     - A frozen Store never changes, so any number of threads read it; ``concurrent`` asks for inserts from threads
       at once, which it has none of.
     - Drop ``concurrent``.
     - 51
   * - ``nonempty`` on a queue or a pool
     - ``nonempty`` is a promise about iterated elements; a queue empties by ``pop``, a pool by ``erase(handle)``.
     - Drop it; check ``empty()`` before ``pop``.
     - 44
   * - A queue (``fifo``, ``lifo``, ``by_priority``) with ``frozen`` or ``positional``
     - A queue changes by push and pop and is reached by its front or top.
     - A sorted set or a sequence.
     - 24
   * - ``monotone_priority`` without ``by_priority``
     - It says how a priority queue is pushed to.
     - Add ``by_priority``.
     - 46
   * - ``handles`` with ``key<K>``, ``positional`` or ``frozen``, or on a ``fifo``/``lifo`` queue
     - A pool holds ``value<V>`` rows reached by handle; a key, a position or an order would identify them otherwise.
     - ``key<K>, positional, monotone`` for an interner of keys; ``by_priority, handles`` for an addressable queue.
     - 27
   * - ``flat`` outside ``handles, unique``
     - ``flat`` is how an interning pool stores records.
     - Add ``handles, unique``, or drop ``flat``.
     - 37
   * - ``handles, unique`` over a record that is no range, without ``flat``
     - A pool of records stores them flat; the request must say so.
     - Add ``flat``.
     - 36
   * - ``partition`` with ``value<V>`` or ``frozen``
     - A partition groups keys and changes by ``insert`` and ``merge``.
     - A map from element to group, or drop ``frozen``.
     - 45
   * - ``fixed_shape`` on a set, a map, a ``monotone`` sequence, or rows that are no Store
     - A fixed shape is a sequence of rows (``value<Store<…>>``) whose number and lengths never change.
     - ``value<Store<…>>``; drop ``monotone``.
     - 47
   * - ``order_by<P>`` without ``sorted`` or ``by_priority``
     - There is no order to change.
     - Add ``sorted``.
     - 28
   * - ``order_by<P>`` on unique keys that are not orderable
     - Ties under ``P`` are broken by the keys' own order; without one, two keys with equal ``P(key)`` would be one.
     - Give the key an order, or ask for ``multi``.
     - 29
   * - ``order_by<P>`` with a ``P`` that fits neither ``P(element)`` nor a map's ``P(key, value)``
     - ``P`` takes the element (a map's key), or a map's key and value.
     - Fix ``P``'s signature.
     - 31
   * - ``dense`` with ``order_by<P>``
     - Dense keys are stored and iterated by index; no other order applies.
     - Drop ``dense``, or order the index type itself.
     - 54
   * - A map ordered by its values (``order_by<P(key, value)>``) without ``small`` or ``frozen``
     - It is a flat scan: O(n) insert, lookup by key.
     - Add ``small`` or ``frozen``, or order by key.
     - 55
   * - ``dense, reclaims``
     - A slot per index: erase returns no storage.
     - ``sparse`` keys (a tree or node table), or ``retains``.
     - 52
   * - ``constant_clear`` on a set or map without ``dense``
     - A constant clear is a generation counter over a slot per index; a hash table or tree clears its slots.
     - Add ``dense``, or accept ``proportional_clear``.
     - 53
   * - ``layered, sorted``
     - A layer iterates its own elements, not its parent's; no order holds over both.
     - Drop ``sorted``; iterate the parent separately.
     - 56
   * - ``partition, concurrent``
     - A merge rewrites parents along two paths.
     - Single-threaded; one partition per thread, merged after.
     - 57

A second group is rejected by the registry rather than a rule: the request is well formed, but no representation
serves it and none could. The diagnosis names the culprits, the stated tags whose removal would make the request
servable.

.. list-table::
   :header-rows: 1
   :widths: 30 42 20 8

   * - Request
     - Why
     - Instead
     - Case
   * - ``key<std::string>, dense``
     - Dense keys are indices; a string is none.
     - A key with ``index_of``, or ``sparse``.
     - 8
   * - ``key<Store<…>>`` where the inner Store is unordered, or ``dense`` without ``sorted``
     - An unordered Store iterates in an order its contents do not fix, so it has no hash.
     - ``sorted`` or ``canonical`` on the inner Store.
     - 32, 33
   * - ``handles, layered`` on a pool of slots
     - Slots are recycled; nothing continues their handles over a parent.
     - ``handles, unique, monotone, layered`` (an interning pool).
     - 40
   * - ``handles, unique, structural_sharing, layered``
     - A tree pool numbers nodes, not rows; no layer continues its handles.
     - Drop ``structural_sharing``.
     - 41
   * - ``dense, layered, concurrent`` maps
     - Missing, not incompatible: the concurrent slot directory is not written yet (see the note in :doc:`/RESOLUTION`).
     - ``dense, layered`` single-threaded, or ``dense, constant_clear, concurrent`` without layers.
     - 43
   * - ``key<int>, value<int>, concurrent, retains``
     - The concurrent tables (gtl) return their storage on ``clear``; no concurrent table retains.
     - Drop ``retains``, or share a ``dense, constant_clear, concurrent`` map.
     - grid
