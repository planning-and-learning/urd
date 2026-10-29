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
     - A reference to an element stays valid until the element is removed.
   * - layout
     - ``contiguous``
     - Elements lie next to each other in memory: ``data()``.
   * - representation
     - ``canonical``
     - Equal contents have equal representations (a bitset); such a Store is hashable by its contents.
   * - threads
     - ``concurrent``
     - Inserts from several threads at once; for maps also ``update`` and ``read``; for pools the operations they
       offer. Clear, move and destruction need no other operation running.
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
       the storage held where the container can.
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
