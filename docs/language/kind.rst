Kind
====

*Leaving out a tag you need does not compile: the operation does not exist.*

The kind of a Store is the top of a tree of kinds (``include/urd/kinds.hpp``). Each kind *claims* the requests it
serves, tried in order: a ``partition`` claims keys grouped into classes, a queue anything with ``fifo``, ``lifo`` or
``by_priority``, a pool ``handles`` with ``unique``, slots ``handles`` alone; then ``key<K>, value<V>`` is a map,
``key<K>`` a set, ``value<V>`` with ``sorted``, ``unique`` or ``multi`` a multiset, and any other ``value<V>`` a
sequence. A kind may split further: a queue is either by arrival (``fifo``, ``lifo``) or by priority. The leaf a
request reaches is its kind.

A kind decides what the Store is: which operations it has (its facets, see the :doc:`kinds </kinds/sets>`), what its
elements are (its data, from the type arguments it takes), and which questions its requests answer (its request
format: its own, then those every kind answers: how it stores, what it costs and how it is used). Each kind says
only what it adds below the root of the tree (:doc:`/extending`). A tag outside that format does not compile, and nor does a
request its kind's rules call contradictory (a partition with ``value<V>``). Answer the questions below first: every other answer is read against them. A kind added outside urd plugs
into the same tree (:doc:`/extending`).

.. urd-group:: kind

.. urd-category:: any_access

   A queue: reached by ``front`` or ``top``; iterated in the order pushed, or a priority queue in none.

.. urd-category:: any_handle

   A pool: ``insert`` gives an :cpp:struct:`urd::index`; with ``unique`` equal rows get equal handles. On a priority
   queue, ``push`` gives a handle to ``update`` or ``erase`` by.

.. urd-category:: any_grouping

   Union-find classes: ``merge``, ``find``, ``same``.
