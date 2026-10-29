Properties
==========

Each property answers one question (a category) with a tag. A tag derived from another promises at least as much,
and the root of a category promises nothing. Containers declare the guarantees in ``container_traits``; the rest
only a request states. A Store has the properties it was asked for, not those of its representation: a set that
``reclaims`` resolves to a tree, which is sorted, but was not asked to be, so it is not.

Guarantees
----------

What a container declares and keeps, checked by the typed tests against a reference model.

.. list-table::
   :header-rows: 1
   :widths: 14 22 64

   * - Category
     - Tags (later ones promise more)
     - Guarantee, and what it switches on
   * - order
     - ``sorted``, ``insertion_ordered``
     - Iteration ascending by :cpp:class:`urd::Less`, or in the order elements first came. ``sorted`` adds
       ``lower_bound``, ``upper_bound``, ``equal_range`` and ``operator<=>``; either adds ``rbegin``, ``rend``.
   * - uniqueness
     - ``unique``
     - No two elements equal. Implied for sets, and for maps without ``multi``; stated for pools that intern.
   * - keys
     - ``dense``, ``sparse``
     - Keys are indices 0..n-1 (:cpp:struct:`urd::index_of`), stored by position; or few in their range.
   * - references
     - ``stable_refs``
     - A reference to a stored element stays valid until that element is removed, by whichever path hands it out:
       an iterator's element, a map's value (``at``, ``operator[]``, ``update``, an entry's ``.second``), a
       sequence's ``operator[]``, a queue's ``front`` or ``top``, a pool's ``[handle]``. Insert moves no other
       element; ``erase`` and ``clear`` end only the references to what they remove.
   * - layout
     - ``contiguous``
     - Elements lie next to each other in memory: ``data()``.
   * - representation
     - ``canonical``
     - Equal contents have equal representations (a bitset); such a Store is hashable by its contents.
   * - threads
     - ``concurrent``
     - Threads share the Store: inserts from threads at once, and beside them reads of published elements (inserted
       before the reading thread started, or by it); for maps also ``update`` and ``read`` on any keys, each value
       under its own lock, ``at`` only for keys no thread writes; for pools inserts and reads by handle. A lookup
       beside inserts is ``contains`` or ``count`` (``find``'s iterator compared with ``end()`` races with the
       size). Erase of another thread's element, ``clear``, iteration, ``reserve``, copy, move and destruction need
       no other operation running.
   * - memory
     - ``retains``, ``reclaims``
     - Storage freed by erase and clear is kept for reuse, or returned.
   * - clear
     - ``proportional_clear`` → ``constant_clear``
     - ``clear`` costs O(elements present), or O(1).
   * - access
     - ``fifo``, ``lifo``, ``by_priority``
     - A queue: reached by ``front`` or ``top``, not iterated.
   * - handles
     - ``handles``
     - A pool: ``insert`` gives an :cpp:struct:`urd::index`; with ``unique`` equal rows get equal handles; on a
       priority queue, ``push`` gives a handle to ``update`` or ``erase`` by.
   * - sharing
     - ``structural_sharing``
     - Equal parts of different rows are stored once (tree pools).
   * - scopes
     - ``scoped``
     - Changes undone by ``pop_scope``.
   * - layering
     - ``layered``
     - Lookups fall through to a parent; on pools, indices continue after the parent's.
   * - tracking
     - ``change_log``
     - ``added()`` and ``removed()`` since ``clear_changes()``.
   * - grouping
     - ``partition``
     - Union-find classes: ``merge``, ``find``, ``same``.
   * - direction
     - ``bidirectional``
     - A map answering ``keys(value)`` too.
   * - shape
     - ``fixed_shape``
     - Rows whose number and lengths are set at construction (:cpp:struct:`urd::row_lengths`); their contents
       change in place, and ``reset()`` sets them back to defaults.
   * - packing
     - ``flat``
     - Each record of an interning pool is one self-contained block of bytes (trivially copyable, or serialized by
       cista), frozen once stored and read by :cpp:struct:`urd::index`.

Requested only
--------------

What a request states and no container declares.

.. list-table::
   :header-rows: 1
   :widths: 14 22 64

   * - Category
     - Tags
     - Effect
   * - lifecycle
     - ``monotone`` → ``frozen``
     - Only grows (no ``erase``, ``clear``, ``pop_back``), or changes only by ``assign(first, last)``, a rebuild in
       the storage held where the container can; a frozen Store is read by any number of threads between rebuilds.
       A monotone queue keeps ``pop``: pop is how a queue is consumed, and ``monotone`` takes only ``clear`` away.
   * - emptiness
     - ``nonempty``
     - At least one element: no default constructor, and construction from elements asserts one; a fixed-shape
       Store asserts a row. A promise about iterated elements: sets, maps and sequences only.
   * - multiplicity
     - ``functional``, ``multi``
     - One value per key, or many (a multimap).
   * - identity
     - ``positional``
     - Elements identified by position: sequences, and sets with positions (an interner with ``monotone``).
   * - workload
     - ``small``
     - A few elements: picks compact containers, never changes a guarantee. Must be backed by a benchmark.
   * - priority
     - ``monotone_priority``
     - A priority queue never pushed below the last popped: integral priorities go to buckets.
   * - order
     - ``order_by<P>``
     - See :cpp:class:`urd::order_by`.

Incompatible combinations
-------------------------

A request is one of three things: served, missing, or incompatible. :doc:`RESOLUTION` shows the first two for a
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
     - Missing, not incompatible: the concurrent slot directory is not written yet (see the note in :doc:`RESOLUTION`).
     - ``dense, layered`` single-threaded, or ``dense, constant_clear, concurrent`` without layers.
     - 43
   * - ``key<int>, value<int>, concurrent, retains``
     - The concurrent tables (gtl) return their storage on ``clear``; no concurrent table retains.
     - Drop ``retains``, or share a ``dense, constant_clear, concurrent`` map.
     - grid
