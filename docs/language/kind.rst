What it is
==========

The kind of a Store follows from its arguments: ``key<K>`` is a set, ``key<K>, value<V>`` a map, ``value<V>`` a
sequence. The questions below turn a value Store into a queue or a pool and a key Store into a partition, and say
how a map's keys meet its values. Answer them first: they decide which operations the Store has at all (see the
:doc:`kinds </kinds/sets>`).

.. urd-theme:: kind

.. urd-category:: any_access

   A queue: reached by ``front`` or ``top``; iterated in the order pushed, or a priority queue in none.

.. urd-category:: any_handle

   A pool: ``insert`` gives an :cpp:struct:`urd::index`; with ``unique`` equal rows get equal handles. On a priority
   queue, ``push`` gives a handle to ``update`` or ``erase`` by.

.. urd-category:: any_grouping

   Union-find classes: ``merge``, ``find``, ``same``.

.. urd-category:: any_multiplicity

   One value per key, or many (a multimap). Maps also declare ``functional``, which selection reads.

.. urd-category:: any_shape

   Rows whose number and lengths are set at construction (:cpp:struct:`urd::row_lengths`, or one per row for rows
   of rows); their contents change in place, and ``reset()`` sets them back to defaults (with ``proportional_clear``,
   only rows handed out to change since).

.. urd-category:: any_direction

   A map answering ``keys(value)`` too.
