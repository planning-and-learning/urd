Kind
====

*Leaving out a tag you need does not compile: the operation does not exist.*

A request names its kind first, with its element types (``include/urd/kinds.hpp``): ``set<K>`` (keys, each once),
``map<K, V>`` (a value per key), ``multiset<V>`` (values by value, in order), ``sequence<V>`` (values by position),
``queue<V>`` (values by their front or top: ``fifo``, ``lifo`` or ``by_priority``), ``pool<V>`` (each distinct value
once, by handle), ``slots<V>`` (values in slots, by handle) and ``partition<K>`` (keys in disjoint classes). Every
argument after it is a tag.

A kind decides what the Store is: which operations it has (its facets, see the :doc:`kinds </kinds/sets>`), what its
elements are (its element types), and which questions its requests answer (its request format: its own, then those
every kind answers: how it stores, what it costs and how it is used). A tag outside that format does not compile, and
nor does a request its kind's rules call contradictory (a queue reached neither by ``fifo``, ``lifo`` nor
``by_priority``). Answer the questions below first: every other answer is read against them. A kind added outside urd
is a kind like these (:doc:`/extending`).

.. urd-group:: kind

.. urd-category:: any_access

   A queue: reached by ``front`` or ``top``; iterated in the order pushed, or a priority queue in none.

.. urd-category:: any_handle

   A pool: ``insert`` gives an :cpp:struct:`urd::index`; with ``unique`` equal rows get equal handles. On a priority
   queue, ``push`` gives a handle to ``update`` or ``erase`` by.

.. urd-category:: any_grouping

   Union-find classes: ``merge``, ``find``, ``same``.
