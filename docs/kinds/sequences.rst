Sequences
=========

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename T, typename... Properties>
   using Sequence = Store<value<T>, Properties...>;   // written out as Store<value<T>, ...> in code

.. cpp:class:: Sequence

   A sequence: ``Store<value<T>, Properties...>``. The name ``Sequence`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Sequence

A sequence identifies its elements by position: positions are 0..n-1.
With ``sorted`` it keeps its elements in order and takes ``insert`` instead of ``push_back``.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Request
     - Representation
   * - ``Store<value<T>>``
     - ``std::vector``
   * - ``Store<value<T>, sorted>``
     - ``boost::container::flat_multiset``
   * - ``Store<value<T>, monotone, stable_refs>``
     - ``SegmentedVector`` (thread-safe with ``concurrent``)
   * - ``Store<value<Store<value<T>, frozen>>>``
     - ``Jagged`` (compressed rows, any depth)
   * - ``Store<value<Store<...>>, fixed_shape>``
     - ``Jagged`` built from :cpp:struct:`row_lengths`
   * - ``Store<value<Row>, retains>`` with a ``retains`` row
     - ``RowPool``

Template parameters
-------------------

``T``
   Any type; :cpp:concept:`orderable` with ``sorted``. A Store as element is a row (:ref:`rows`).

Member types
------------

``value_type`` is ``T``; ``iterator`` and ``reference`` are the representation's and are mutable where the
elements change in place (not ``sorted``, not ``monotone``, not rows handed out as views).

Member functions
----------------

Construction, capacity (``size``, ``empty``, ``reserve``), iterators (``begin``, ``end``, ``rbegin``, ``rend``) and
comparison (``==``, ``<=>``) are as for :doc:`sets`.

Element access
^^^^^^^^^^^^^^

.. cpp:function:: reference operator[](std::size_t pos)
.. cpp:function:: const_reference operator[](std::size_t pos) const
.. cpp:function:: reference at(std::size_t pos)
.. cpp:function:: const_reference at(std::size_t pos) const
.. cpp:function:: reference front()
.. cpp:function:: reference back()
.. cpp:function:: std::size_t position(const_iterator it) const
.. cpp:function:: auto data()

   A pointer to the elements, with ``contiguous``.

Modifiers
^^^^^^^^^

Growable (not ``frozen``, not ``fixed_shape``):

.. cpp:function:: void push_back(const value_type& element)
.. cpp:function:: void push_back(value_type&& element)
.. cpp:function:: template<typename... Args> reference emplace_back(Args&&... args)

   Unsorted sequences. The new element, changeable in place where the elements are.

.. cpp:function:: iterator insert(const value_type& element)
.. cpp:function:: template<typename... Args> iterator emplace(Args&&... args)

   Sorted sequences: into place.

.. cpp:function:: iterator insert(const_iterator pos, const value_type& element)
.. cpp:function:: template<typename... Args> iterator emplace(const_iterator pos, Args&&... args)

   At a position (not monotone, unsorted).

Shrinkable (neither ``frozen`` nor ``monotone``):

.. cpp:function:: iterator erase(const_iterator pos)
.. cpp:function:: iterator erase(const_iterator first, const_iterator last)
.. cpp:function:: void pop_back()
.. cpp:function:: void clear()
.. cpp:function:: friend std::size_t erase(Sequence& store, const value_type& element)
.. cpp:function:: template<typename Pred> friend std::size_t erase_if(Sequence& store, Pred pred)

   As ``std::erase`` and ``std::erase_if``.

Shape (``fixed_shape``)
^^^^^^^^^^^^^^^^^^^^^^^

.. cpp:function:: explicit Sequence(const row_lengths& lengths)

   Rows of these lengths, empty: a fixed-shape Store's only shape. Rows of rows take a vector of row lengths per
   row, at any depth. With ``nonempty``, at least one row (a row of length 0 is a row), asserted.

.. cpp:function:: void reset()

   Every element back to its default, every bit unset; with ``proportional_clear``, in the rows handed out to
   change since the last reset only.

Complexity
----------

.. urd-complexity:: sequences

Example
-------

.. code-block:: cpp

   using Log = Store<value<Event>, monotone>;                       // only grows
   using Rows = Store<value<Store<value<int>, frozen>>>;             // compressed rows: one array, one of offsets
   auto rows = Rows { { 1, 2, 3 }, { 4 } };
   std::span<const int> first = rows[0];
   using Bits = Store<value<Store<key<Vertex>, dense>>, fixed_shape>;   // a bit matrix
   auto bits = Bits(row_lengths { 8, 8, 8 });
   bits[1].insert(Vertex { 3 });
