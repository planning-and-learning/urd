Pools
=====

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename Row, typename... Properties>
   using Interned = Store<value<Row>, handles, unique, Properties...>;   // rows, each stored once, by handle
   template<typename T, typename... Properties>
   using Slots = Store<value<T>, handles, Properties...>;                // elements in slots that never move

.. cpp:class:: Pool

   A pool: ``Store<value<Row>, handles, Properties...>``, interning with ``unique``. The name ``Pool`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Pool

A pool is reached by handle, neither iterated nor counted (an interning pool may share parts of its rows). A handle
is an :cpp:struct:`index\<Row\> <urd::index>`: a pool of rows kept in full numbers them in the order first inserted; a tree pool
keeps a handle of its own.

.. list-table::
   :header-rows: 1
   :widths: 50 50

   * - Request
     - Representation
   * - ``Store<value<Row>, handles, unique>``
     - ``RowInterner``: each distinct row once, in full; rows of dense indices as bitset words
   * - ``Store<value<Row>, handles, unique, structural_sharing>``
     - ``TreeVectorSet`` (sequence rows), ``TreeBitsetSet`` (rows of dense indices): equal subtrees stored once
   * - ``Store<value<T>, handles>``
     - ``SlotVector``: stable slots, freed ones reused
   * - any of them with ``concurrent``
     - the same, thread-safe
   * - ``Store<value<Row>, handles, unique, layered>``
     - ``LayeredPool`` over the row interner (:doc:`records`)

Template parameters
-------------------

``Row``
   A Store (``Store<value<T>, frozen>``, ``Store<key<I>, dense, frozen>``), or any range constructible from its
   elements. The Row's own rules decide which rows are equal: a set's in any order, once.

Member types
------------

.. list-table::
   :widths: 30 70

   * - ``row_type``
     - ``Row``
   * - ``handle_type``
     - :cpp:struct:`index\<Row\> <urd::index>`, or the tree pools' handle
   * - ``write_type``
     - ``Write<element>``: a change to a sequence row, its value at a position

Member functions: interning pools (``handles, unique``)
--------------------------------------------------------

.. cpp:function:: template<std::ranges::input_range Elements> handle_type insert(const Elements& elements)
.. cpp:function:: handle_type insert(std::initializer_list<element> elements)

   The handle of the row with these elements: an earlier handle where an equal row is stored.

.. cpp:function:: handle_type insert(handle_type base, std::span<const write_type> writes)

   Sequence rows: the row ``base`` with ``writes`` applied (sorted by position, each once and within the row). A
   pool that shares structure stores only what changed.

.. cpp:function:: handle_type insert(handle_type base, std::span<const element> added, std::span<const element> removed)

   Rows that are sets: ``base`` with ``added`` put in and then ``removed`` taken out.

.. cpp:function:: bool contains(handle_type handle, const element& value) const

   Rows that are sets; a tree pool reads one path, not the row.

.. cpp:function:: row_type get(handle_type handle) const
.. cpp:function:: std::size_t row_size(handle_type handle) const
.. cpp:function:: void read(handle_type handle, std::span<element> out) const

   The row's elements into ``out``, which must hold ``row_size(handle)`` of them; allocates nothing.

.. cpp:function:: auto view(handle_type handle) const

   The stored row without copying, where the pool keeps rows in full: a span of the elements, or for a dense set its
   bits (``BitsetSpan``). Valid until the pool is cleared.

.. cpp:function:: std::size_t memory_usage() const
.. cpp:function:: void clear()

   Forgets every row; the handles given so far mean nothing after. Shrinkable pools.

Member functions: slots (``handles`` without ``unique``)
--------------------------------------------------------

.. cpp:namespace:: urd

.. cpp:class:: Slots

   A pool of slots: ``Store<value<T>, handles, Properties...>``. The name ``Slots`` stands for the resolved Store
   type below.

.. cpp:namespace:: urd::Slots

.. cpp:function:: handle_type insert(const value_type& element)
.. cpp:function:: handle_type insert(value_type&& element)
.. cpp:function:: template<typename... Args> handle_type emplace(Args&&... args)

   A slot holding the element: an erased one where there is one, else a new one.

.. cpp:function:: handle_type acquire()

   A slot to use as it is: an erased element with what it held and the storage it kept, else a new one.

.. cpp:function:: const value_type& operator[](handle_type handle) const
.. cpp:function:: value_type& operator[](handle_type handle)
.. cpp:function:: std::size_t size() const
.. cpp:function:: bool empty() const
.. cpp:function:: void erase(handle_type handle)

   Frees the slot for reuse; the handle and references to the element mean nothing after.

.. cpp:function:: void clear()

Complexity
----------

r a row's length; k the changes in a delta; A the tree arity (4).

.. list-table::
   :header-rows: 1
   :widths: 20 14 18 18 12 18

   * - Representation
     - insert a row
     - insert a delta
     - read a row
     - ``contains``
     - memory
   * - ``RowInterner``
     - O(r)~
     - O(r)~ (copies the row)
     - O(r); ``view(h)`` O(1)
     - O(r); O(1) for dense rows
     - each distinct row once, in full
   * - ``TreeVectorSet``
     - O(r)~
     - O(k log\ :sub:`A` r)~
     - O(r)
     - —
     - nodes shared across rows: a delta adds about k·log\ :sub:`A` r nodes
   * - ``TreeBitsetSet``
     - O(r/32)~
     - O(k log\ :sub:`A` (r/32))~
     - O(r/32 + set bits)
     - O(log\ :sub:`A` (r/32))
     - as above; all-zero subtrees take nothing
   * - ``SlotVector``
     - O(1)\* (a freed slot first)
     - —
     - O(1) by handle
     - —
     - stable slots; erased elements keep their storage

Measured, not only bounded: for 20,000 versions of a row that each differ from an earlier one in 3 positions, a
tree pool takes 136–271 bytes per version against 447–4,409 for rows stored in full (:doc:`../BENCHMARKS`).

Example
-------

.. code-block:: cpp

   using States = Store<value<Store<key<Atom>, dense, frozen>>, handles, unique, structural_sharing>;
   auto states = States();
   const auto initial = states.insert(facts);                                 // index<...>
   const auto next = states.insert(initial, added, removed);                  // a delta: only the changed path is new
   states.contains(next, atom);
