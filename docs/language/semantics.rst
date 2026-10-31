Semantics
=========

*Leaving out a tag you need is a bug: the program relies on something nobody keeps.*

What holds of the elements. Most of these the container keeps (a **guarantee**: ``sorted``, ``stable_refs``); a few
you keep about your data (a **demand**: ``dense``), and urd picks a container that relies on it, so state one only
where it holds. Each tag's card says which. Saying less buys cheaper containers: leave a question open where nothing
reads the answer.

.. urd-group:: semantics

What you promise about your data
--------------------------------

.. urd-heading:: promise

.. urd-category:: any_keys

   Keys are indices 0..n-1 (:cpp:struct:`urd::index_of`), stored by position; or few in their range.

.. urd-category:: any_priority

   A priority queue never pushed below the last popped: integral priorities go to buckets. Stating it makes the Store
   a priority queue, so ``by_priority`` need not be stated with it.

What holds
----------

.. urd-heading:: holds

.. urd-category:: any_order

   Iteration ascending by :cpp:class:`urd::Less`, or in the order elements first came. ``sorted`` adds
   ``lower_bound``, ``upper_bound``, ``equal_range`` and ``operator<=>``; either adds ``rbegin``, ``rend``. To order
   by something else, see ``order_by<P>`` in :doc:`index`.

.. urd-category:: by_key

   Elements identified by position: sequences, and sets with positions (an interner with ``monotone``). Such
   containers also declare it, which selection reads.

.. urd-category:: duplicates

   No two elements equal. Implied for sets, and for maps without ``multi``; stated for pools that intern.

.. urd-category:: any_multiplicity

   One value per key, or many (a multimap).

.. urd-category:: may_change

   Only grows (no ``erase``, ``clear``, ``pop_back``), or changes only by ``assign(first, last)``, a rebuild in the
   storage held where the container can; a frozen Store is read by any number of threads between rebuilds. A
   monotone queue keeps ``pop``: pop is how a queue is consumed, and ``monotone`` takes only ``clear`` away.

.. urd-category:: maybe_empty

   At least one element: no default constructor, and construction from elements asserts one; a fixed-shape Store
   asserts a row. A promise about iterated elements: sets, maps and sequences only.

.. urd-category:: single_threaded

   Threads share the Store: inserts from threads at once, and beside them reads of published elements (inserted
   before the reading thread started, or by it); for maps also ``update`` and ``read`` on any keys, each value under
   its own lock, ``at`` only for keys no thread writes; for pools inserts and reads by handle. A lookup beside
   inserts is ``contains`` or ``count`` (``find``'s iterator compared with ``end()`` races with the size). Erase of
   another thread's element, ``clear``, iteration, ``reserve``, copy, move and destruction need no other operation
   running.

.. urd-category:: moving_refs

   A reference to a stored element stays valid until that element is removed, by whichever path hands it out: an
   iterator's element, a map's value (``at``, ``operator[]``, ``update``, an entry's ``.second``), a sequence's
   ``operator[]``, a queue's ``front`` or ``top``, a pool's ``[handle]``. Insert moves no other element; ``erase`` and
   ``clear`` end only the references to what they remove.

.. urd-category:: any_representation

   Equal contents have equal representations (a bitset); such a Store is hashable by its contents.

What more it can do
-------------------

.. urd-heading:: more

.. urd-category:: any_scope

   Changes undone by ``pop_scope``.

.. urd-category:: any_layering

   Lookups fall through to a parent; on pools, indices continue after the parent's.

.. urd-category:: any_tracking

   ``added()`` and ``removed()`` since ``clear_changes()``.

.. urd-category:: any_direction

   A map answering ``keys(value)`` too.

.. urd-category:: any_shape

   Rows whose number and lengths are set at construction (:cpp:struct:`urd::row_lengths`, or one per row for rows
   of rows); their contents change in place, and ``reset()`` sets them back to defaults (with ``proportional_clear``,
   only rows handed out to change since).

How it hands out memory
-----------------------

.. urd-heading:: memory

.. urd-category:: any_layout

   Elements lie next to each other in memory: ``data()``.

.. urd-category:: any_packing

   Each record of an interning pool is one self-contained block of bytes (trivially copyable, or serialized by
   cista), frozen once stored and read by :cpp:struct:`urd::index`.
