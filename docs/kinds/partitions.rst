Partitions
==========

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename I>
   using Partition = Store<key<I>, partition>;      // DisjointSets (union-find)

.. cpp:class:: Partition

   A partition: ``Store<key<I>, partition>``. The name ``Partition`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Partition

A partition groups indices into disjoint classes, reached by ``merge`` and ``find``. It is not iterated.

Template parameters
-------------------

``I``
   :cpp:concept:`indexable`.

Member functions
----------------

.. cpp:function:: void insert(const key_type& element)

   Makes ``element`` an element (and, for dense keys, every smaller index not yet one).

.. cpp:function:: bool merge(const key_type& lhs, const key_type& rhs)

   Joins the classes; ``false`` where they were one already.

.. cpp:function:: key_type find(const key_type& element) const

   The element that names the class of ``element``.

.. cpp:function:: bool same(const key_type& lhs, const key_type& rhs) const
.. cpp:function:: std::size_t class_size(const key_type& element) const
.. cpp:function:: std::size_t classes() const
.. cpp:function:: std::size_t size() const
.. cpp:function:: void clear()

Complexity
----------

``insert`` O(1)\*; ``find``, ``same``, ``merge`` O(α(n))\* with path compression and union by size; ``clear`` O(1);
memory n parents + n sizes.
