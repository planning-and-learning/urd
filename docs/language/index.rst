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
     - ``set<K>``, ``queue<V>``, ``fifo``
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
  stays at its **none**, which promises nothing: urd may pick anything there. In code a category is an enum, none
  first (``urd::keys { none, dense, sparse }``), and each answer has a tag type (``urd::dense``). Categories are open:
  another enum with tag types is another category (:doc:`/extending`).
- **A kind** answers a set of categories, its *request format*: a queue has no ``bidirectional``, a set no
  ``fifo``. A tag outside the format of the request's kind does not compile (*a property means nothing for this kind
  of Store*). Each category's page lists the kinds that answer it.
- **A tag** is one answer to a category. Within a category some answers promise more than others:
  ``constant_clear`` (clear in O(1)) is at least ``proportional_clear`` (clear in O(elements present)), so a
  container that clears in O(1) also serves a request for ``proportional_clear``. Each category's diagram draws
  this: an arrow points to an answer that promises at least as much.

Take ``Distance`` from the example on the :doc:`front page </index>`:

.. code-block:: cpp

   using Distance = Store<map<User, int>, dense, constant_clear>;

.. list-table::
   :header-rows: 1
   :widths: 18 32 50

   * - Group
     - Category
     - Answer, and what leaving it out would cause
   * - Kind
     - (the first argument)
     - ``map<User, int>``: a map. As ``set<User>`` it is a set, and ``distance[u]`` does not compile
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
     - its none: no promise

urd picks the cheapest container that keeps every answer: here a map with a generation counter per slot
(``generation_map``). A Store has the properties it was asked for, not those of its representation: a set that
``reclaims`` resolves to a tree, which is sorted, but was not asked to be, so it is not.

Who keeps an answer
-------------------

Tags also differ in who makes the promise, shown on each tag's card. The role follows from the group, and in
semantics from who keeps the tag:

- a **kind** makes the Store a set, a queue, a pool or a partition, named first (``queue<V>``), and its tags say how
  (``fifo``);
- a **guarantee** is kept by the container (``sorted``, ``stable_refs``, and the costs); a container declares its
  guarantees in its declaration's ``provides`` (:doc:`/representations`), and the typed tests check them against a
  reference model;
- a **demand** is a promise you make about the data (``dense``: the keys really are indices);
- a **request** only a request states, and the Store's interface enforces it (``frozen``, ``positional``);
- a **hint** (``small``) changes the choice, never a guarantee.

A request is what it states, and what that means (each kind's ``meaning``, ``include/urd/kinds.hpp``): a set, and a
map without ``multi``, keep each key once; a positional set numbers its keys as they come; a frozen Store's
references are stable (``any_kind::meaning``, whatever the kind).

Tags are what a request states. What its data *is* (whether a key can be hashed, is an index, copies as bytes, ...)
is not stated but derived from the types: these are **type traits** (``urd::trait::can_hash``, ...,
``include/urd/registry/facts.hpp``), which containers rely on by role (the elements, the order, a map's values, the
rows). They never appear in a request; see :doc:`/semantics` for the concepts they are derived from. What the Store's interface offers follows from ``frozen``, ``monotone``, ``nonempty`` and
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

Order by a comparator
---------------------

``order_by<C>`` is a tag that carries a comparator ``C``: it orders a ``sorted`` Store or a ``by_priority`` queue by
``C`` instead of :cpp:class:`urd::Less`, as ``std::set``'s comparator does. ``C`` compares two elements, in a map two
keys. :cpp:class:`urd::by` compares by a projection: ``order_by<by<&Job::deadline>>``. The order is ``C`` and nothing
else: elements ``C`` does not separate are equivalent, so a unique Store keeps one of them. See
:cpp:class:`urd::order_by`.

Incompatible combinations
-------------------------

A request is one of three things: served, missing, or incompatible. :doc:`/RESOLUTION` shows the first two for a
grid of requests (a ``none`` row carries a note saying what is missing). ``to_spec`` rejects only what is
ill-formed whatever container would hold it: a first argument that is no kind, an argument after it that is no tag,
a tag its kind's format does not ask, and the rules its kind states (``well_formed`` in ``include/urd/kinds.hpp``: a
queue is reached by ``fifo``, ``lifo`` or ``by_priority``, ``monotone_priority`` needs ``by_priority``, ``order_by``
needs ``sorted`` or ``by_priority``), each with its message. A compile-fail case in ``tests/compile_fail_store.cpp`` pins
each message (``urd_compile_fail_expects`` in ``CMakeLists.txt``).

.. list-table::
   :header-rows: 1
   :widths: 30 42 20 8

   * - Request
     - Why
     - Instead
     - Case
   * - A first argument that is no kind: ``Store<int, sorted>``
     - The kind comes first, with its element types.
     - ``Store<set<int>, sorted>``.
     - 8
   * - An argument after the kind that is no tag
     - Every argument after the kind is a tag, ``order_by<C>`` among them.
     - Move the type into the kind: ``map<K, V>``, or a tuple element.
     - 9
   * - Two tags of one category: ``dense, sparse``; ``retains, reclaims``; ``frozen, monotone``;
       two workloads
     - One question, two answers.
     - Keep one.
     - 4, 5, 13
   * - ``set<K>, handles``
     - Handles are a pool's (or a queue's); a set reaches its keys by themselves.
     - ``pool<V>``, or ``set<K>, positional, monotone`` for an interner of keys.
     - 24
   * - ``partition<K>, multi``
     - A partition groups keys, each once.
     - A ``map<K, V>, multi``.
     - 39
   * - ``order_by<C>`` on a set without ``sorted``
     - ``order_by`` orders a sorted Store (or a ``by_priority`` queue); an unsorted set has no order to give.
     - Add ``sorted``.
     - 25
   * - ``order_by<C>`` with a ``C`` that does not compare two elements
     - ``C`` compares two elements, in a map two keys.
     - Fix ``C``, or compare by a projection with ``by<P>``.
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
   * - ``set<std::string>, dense``
     - Dense keys are indices; a string is none.
     - A key with ``index_of``, or ``sparse``.
     - 7
   * - ``set<Store<…>>`` where the inner Store is unordered, or ``dense`` without ``sorted``
     - An unordered Store iterates in an order its contents do not fix, so it has no hash.
     - ``sorted`` or ``canonical`` on the inner Store.
     - 28, 29
   * - ``slots<V>, layered``
     - Slots are recycled; nothing continues their handles over a parent.
     - ``pool<V>, monotone, layered`` (an interning pool).
     - 36
   * - ``pool<V>, structural_sharing, layered``
     - A tree pool numbers nodes, not rows; no layer continues its handles.
     - Drop ``structural_sharing``.
     - 37
   * - ``dense, layered, concurrent`` maps
     - Missing, not incompatible: the concurrent slot directory is not written yet (see the note in :doc:`/RESOLUTION`).
     - ``dense, layered`` single-threaded, or ``dense, constant_clear, concurrent`` without layers.
     - 38
   * - ``map<int, int>, concurrent, retains``
     - The concurrent tables (gtl) return their storage on ``clear``; no concurrent table retains.
     - Drop ``retains``, or share a ``dense, constant_clear, concurrent`` map.
     - grid
