Kind
====

*Leaving out a tag you need does not compile: the operation does not exist.*

The kind of a Store follows from its arguments: ``key<K>`` is a set, ``key<K>, value<V>`` a map, ``value<V>`` a
sequence. The questions below turn a value Store into a queue or a pool and a key Store into a partition. Answer them
first: they decide which operations the Store has at all (see the :doc:`kinds </kinds/sets>`), and every other answer
is read against them.

.. urd-group:: kind

.. urd-category:: any_access

   A queue: reached by ``front`` or ``top``; iterated in the order pushed, or a priority queue in none.

.. urd-category:: any_handle

   A pool: ``insert`` gives an :cpp:struct:`urd::index`; with ``unique`` equal rows get equal handles. On a priority
   queue, ``push`` gives a handle to ``update`` or ``erase`` by.

.. urd-category:: any_grouping

   Union-find classes: ``merge``, ``find``, ``same``.
