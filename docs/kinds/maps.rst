Maps
====

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename K, typename V, typename... Properties>
   using Map = Store<key<K>, value<V>, Properties...>;   // written out as Store<key<K>, value<V>, ...> in code

.. cpp:class:: Map

   A map: ``Store<key<K>, value<V>, Properties...>``. The name ``Map`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Map

A map holds one value per key (``functional``, implied) or many (``multi``: a multimap). Its elements are
``std::pair<const K, V>``; a dense map iterates each key with a view of its value.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Request
     - Representation
   * - ``Store<key<K>, value<V>>``
     - ``boost::unordered_flat_map``
   * - ``Store<key<K>, value<V>, sorted>``
     - ``std::map``; with ``frozen``, ``boost::container::flat_map``
   * - ``Store<key<K>, value<V>, stable_refs>``
     - ``std::unordered_map``
   * - ``Store<key<K>, value<Row>>`` with a ``retains`` row
     - ``Recycling`` hash map (a tree with ``sorted``): erased keys leave their nodes and values for the next keys
   * - ``Store<key<I>, value<V>, dense>``
     - ``IndexMap`` (a slot per index); with ``constant_clear``, with generations; with ``stable_refs``, slots in
       segments that never move (what is stable is the value ``at``, ``operator[]`` and ``update`` reach, and the
       ``.second`` of an iterated entry); with ``constant_clear, concurrent``, a spin lock per slot too. Only a
       request that says ``dense`` gets it: a slot per index demands keys that are indices
   * - ``Store<key<K>, value<V>, concurrent>``
     - ``gtl::parallel_flat_hash_map``; ``update`` and ``read`` under the key's shard lock
   * - ``Store<key<K>, value<V>, insertion_ordered>``
     - ``OrderedMap`` (over a ``SparseSet`` with ``dense``)
   * - ``Store<key<K>, value<V>, multi>``
     - ``PostingMap`` (a flat hash table of value vectors); with ``sorted``, ``std::multimap``
   * - ``Store<key<I>, value<V>, multi, dense, frozen>``
     - ``CsrMultiMap`` (compressed sparse rows); with ``bidirectional`` and ``value<I>``, ``BidirectionalCsr``
   * - ``Store<key<K>, value<V>, scoped>``
     - ``ScopedMap`` (scopes undone by a log)
   * - ``Store<key<K>, value<V>, layered>``
     - ``Layered`` over a flat hash map; with ``dense``, over an ``IndexMap``
   * - ``Store<key<K>, value<V>, sorted, order_by<[](const K&, const V&) { … }>>``
     - ``FlatEntryMap``: found by key, in the order the values give

Template parameters
-------------------

``K``
   As for a set: :cpp:concept:`hashable`, :cpp:concept:`orderable` with ``sorted``, :cpp:concept:`indexable` with
   ``dense``.
``V``
   Any type. A Store as a value is a row: see :ref:`rows`.

Member types
------------

.. list-table::
   :widths: 30 70

   * - ``value_type``
     - ``std::pair<const K, V>``
   * - ``key_type``, ``lookup_type``
     - ``K``
   * - ``mapped_type``
     - ``V``
   * - ``iterator``, ``const_iterator``, ``reference``, ``const_reference``
     - the representation's; a dense map's reference is a pair of the key and a view of its value

Member functions
----------------

Construction, capacity, iterators, modifiers by element (``insert``, ``emplace``, ``erase``, ``clear``,
``erase_if``), layers and comparison are as for :doc:`sets`, over pairs. ``erase(key)`` takes a key, and so do the
lookups:

Lookup
^^^^^^

.. cpp:function:: const_iterator find(const key_type& key) const
.. cpp:function:: size_type count(const key_type& key) const
.. cpp:function:: bool contains(const key_type& key) const
.. cpp:function:: const_iterator lower_bound(const key_type& key) const
.. cpp:function:: const_iterator upper_bound(const key_type& key) const
.. cpp:function:: std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const

   The bounds with ``sorted``, unless the map is ordered by its values, which is searched by key only.

Values by key (one value per key)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. cpp:function:: const mapped_type& at(const key_type& key) const
.. cpp:function:: mapped_type& at(const key_type& key)

   The value; ``std::out_of_range`` where the key has none. The mutable one is not offered on a ``frozen`` map or
   one ordered by its values.

.. cpp:function:: mapped_type& operator[](const key_type& key)

   The value for ``key``, inserted as a default first where there is none (growable, default-constructible value).

.. cpp:function:: template<typename... Args> std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args)
.. cpp:function:: template<typename M> std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& value)

.. cpp:function:: template<typename Update> decltype(auto) update(const key_type& key, Update update)

   Calls ``update(value, held)`` on the value of ``key``, inserting a default one first where there is none
   (``held`` is then ``false``), and returns what ``update`` returns. A map cleared by generation hands over the
   value it kept from before the clear, storage and all, for ``update`` to reuse. With ``concurrent``, threads may
   update at once: a sharded table runs ``update`` under the key's lock.

.. cpp:function:: template<typename Read, typename Result> Result read(const key_type& key, Read read, Result missing) const

   ``read(value)`` where ``key`` has a value, else ``missing``. With ``concurrent``, safe while others update.

.. cpp:function:: template<typename Combine> bool merge(const key_type& key, const mapped_type& value, Combine combine)

   Inserts ``value`` where ``key`` has none, else replaces the held value by ``combine(held, value)`` (a hull of
   intervals, the lower of two costs); whether the map changed, as fixpoints ask.

Values by key (a multimap)
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. cpp:function:: auto values(const key_type& key) const

   The values of ``key``: a span for a posting map or a CSR, a range of values otherwise.

Entries in order (``insertion_ordered``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. cpp:function:: auto keys() const
.. cpp:function:: auto values() const

   The keys and the values as spans, in the order the keys first came.

.. cpp:function:: std::size_t position(const_iterator it) const

Inverse (``bidirectional``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. cpp:function:: auto keys(const mapped_type& value) const

   A range of the keys that map to ``value``: a frozen graph's in-edges.

Scopes (``scoped``)
^^^^^^^^^^^^^^^^^^^

.. cpp:function:: void push_scope()
.. cpp:function:: void pop_scope()

   Opens a scope; undoes every change made since the matching ``push_scope``, latest first, so an inner binding
   shadows an outer one until its scope ends.

.. cpp:function:: std::size_t depth() const

   How many scopes are open. ``clear`` closes every scope.

Layers (``layered``)
^^^^^^^^^^^^^^^^^^^^

As for sets, and in a map a key's own value shadows the parent's: ``insert_or_assign`` writes its own, and ``at()``
on a layer that may change makes an own value before handing it out: a layer over the parent's value where values
layer themselves (a map of layered pools layers through both levels, see :doc:`records`), else a copy of it. Read
through a const Store to avoid that.

Complexity
----------

n elements; U the key range; r a row's length.

.. list-table::
   :header-rows: 1
   :widths: 30 10 14 12 12 12 10

   * - Representation
     - insert
     - lookup
     - erase
     - clear
     - iterate
     - memory
   * - ``boost::unordered_flat_map``
     - O(1)\*~
     - O(1)~
     - O(1)~
     - O(capacity)
     - O(capacity)
     - ~n/0.875 slots + 1 byte each
   * - ``std::unordered_map``
     - O(1)\*~
     - O(1)~
     - O(1)~
     - O(n + buckets)
     - O(n)
     - a node per element
   * - ``std::map``, ``std::multimap``
     - O(log n)
     - O(log n)
     - O(log n)
     - O(n)
     - O(n), in order
     - a node per element
   * - ``Recycling<Map>``
     - as Map, a kept node first
     - as Map
     - as Map; keeps the node
     - O(n)
     - as Map
     - erased nodes and values kept
   * - ``boost::container::flat_map``
     - O(n)
     - O(log n)
     - O(n)
     - O(n)
     - O(n), contiguous
     - n entries
   * - ``IndexMap``
     - O(1)\*
     - O(1)
     - O(1)
     - O(U); O(1) with generations
     - O(U), in key order
     - a slot per index (+ a lock byte, thread-safe)
   * - ``OrderedMap``
     - as its key set
     - as its key set
     - O(n)
     - as its key set
     - O(n); keys and values as spans
     - key set + n values
   * - ``FlatEntryMap``
     - O(n)
     - O(n)
     - O(n)
     - O(n)
     - O(n), in the given order
     - n entries; for few
   * - ``PostingMap``
     - O(1)\*~
     - O(1)~; ``values(k)`` a span
     - O(r) per value
     - O(capacity + n)
     - O(n)
     - a vector per key
   * - ``CsrMultiMap``
     - build O(n + U)
     - O(1); ``values(k)`` a span
     - —
     - —
     - O(n + U), in key order
     - n values + U + 1 offsets
   * - ``ScopedMap``
     - O(1)\*~
     - O(1)~
     - O(1)~
     - O(capacity)
     - O(capacity)
     - + a log entry per change; ``pop_scope`` O(changes)
   * - ``gtl::parallel_flat_hash_map``
     - O(1)\*~ under a shard's lock
     - O(1)~ under it
     - O(1)~
     - O(capacity)
     - O(capacity)
     - 16 shards

.. _rows:

Rows: Stores as values and elements
-----------------------------------

A Store of Stores is written as one: an element, key or value can be another Store, to any depth. Most rows are
held as Stores, each owning its storage. Where the outer request says how rows live, urd stores them together and
hands out a row as a view (``Rows::reference`` names what a row is handed out as):

.. list-table::
   :header-rows: 1
   :widths: 24 40 18 18

   * - Rows
     - Request
     - Container
     - A row is
   * - never change
     - ``Store<value<Store<value<T>, frozen>>>`` (any depth)
     - ``Jagged``: CSR, one array per level
     - ``std::span<const T>``, or a range of the rows below
   * - fixed lengths, changing contents
     - ``Store<value<Store<value<T>>>, fixed_shape>``
     - ``Jagged`` built from :cpp:struct:`row_lengths`
     - ``std::span<T>``
   * - … of dense indices
     - ``Store<value<Store<key<I>, dense>>, fixed_shape>``
     - ``Jagged`` over one word array
     - a ``BitsetSpan`` (one bit per index)
   * - reused after removal
     - ``Store<value<Row>, retains>`` with a ``retains`` row
     - ``RowPool``
     - the row Store, cleared, not freed
   * - map values reused after erase
     - ``Store<key<K>, value<Row>>`` with a ``retains`` row
     - ``Recycling`` over the map
     - the value Store, cleared, kept with its node
   * - interned, by handle
     - ``Store<value<Row>, handles, unique>``
     - ``RowInterner``, tree pools
     - the row, read by handle (:doc:`pools`)
