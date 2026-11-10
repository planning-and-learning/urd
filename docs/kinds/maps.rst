Maps
====

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename K, typename V, typename... Properties>
   using Map = Store<map<K, V>, Properties...>;   // written out as Store<map<K, V>, ...> in code

.. cpp:class:: Map

   A map: ``Store<map<K, V>, Properties...>``. The name ``Map`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Map

A map holds one value per key, or many (``multi``: a multimap). Its elements are
``std::pair<const K, V>``; a dense map iterates each key with a view of its value.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Request
     - Representation
   * - ``Store<map<K, V>>``
     - ``gtl::flat_hash_map``
   * - ``Store<map<K, V>, sorted>``
     - ``std::map``; with ``frozen``, ``boost::container::flat_map``
   * - ``Store<map<K, V>, stable_refs>``
     - ``std::unordered_map``
   * - ``Store<map<K, Row>>`` with a ``retains`` row
     - ``Recycling`` hash map (a tree with ``sorted``): erased keys leave their nodes and values for the next keys
   * - ``Store<map<I, V>, dense>``
     - ``IndexMap`` (a slot per index); with ``constant_clear``, with generations; with ``stable_refs``, slots in
       segments that never move (what is stable is the value ``at``, ``operator[]`` and ``update`` reach, and the
       ``.second`` of an iterated entry); with ``constant_clear, concurrent``, a spin lock per slot too. Only a
       request that says ``dense`` gets it: a slot per index demands keys that are indices
   * - ``Store<map<K, V>, concurrent>``
     - ``gtl::parallel_flat_hash_map``; ``update`` and ``read`` under the key's shard lock; with ``stable_refs``,
       ``parallel_node_hash_map`` (a node per entry)
   * - ``Store<map<K, V>, insertion_ordered>``
     - ``OrderedMap`` (over a ``SparseSet`` with ``dense``)
   * - ``Store<map<K, V>, multi>``
     - ``PostingMap`` (a flat hash table of value vectors); with ``retains``, ``ChainedPostingMap`` (its values in
       one pool of slots, so churning keys and clearing allocate nothing once warm); with ``sorted``, ``std::multimap``
   * - ``Store<map<I, V>, multi, dense, frozen>``
     - ``CsrMultiMap`` (compressed sparse rows); with ``bidirectional`` and values of ``I`` (``map<I, I>``), ``BidirectionalCsr``
   * - ``Store<map<I, V>, multi, dense>``
     - ``DenseMultiMap``: a vector of values per index, growing; erase and clear keep the rows
   * - ``Store<map<K, V>, scoped>``
     - ``Scoped`` over a flat hash map (scopes undone by a log); over a tree with ``sorted``, an ``IndexMap`` with
       ``dense``
   * - ``Store<map<I, V>, dense, layered>``
     - ``Layered`` over an ``IndexMap``; with ``concurrent`` (or ``constant_clear``), over the thread-safe generation
       map. Keys that are no indices have no layer.

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
   * - ``key_type``
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

   The value for ``key``, inserted as a default first where there is none (not frozen, default-constructible value).

.. cpp:function:: template<typename... Args> std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args)
.. cpp:function:: template<typename M> std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& value)

.. cpp:function:: template<typename Update> decltype(auto) update(const key_type& key, Update update)

   Calls ``update(value, held)`` on the value of ``key``, inserting a default one first where there is none
   (``held`` is then ``false``), and returns what ``update`` returns. A map cleared by generation hands over the
   value it kept from before the clear, storage and all, for ``update`` to reuse. With ``concurrent``, threads may
   update at once: a sharded table runs ``update`` under the key's lock. Only where the representation has it
   (maps by index, sharded tables, layers); any other map is updated through ``try_emplace``.

.. cpp:function:: template<typename F, typename Result> Result visit(const key_type& key, F f, Result missing) const

   ``f(value)`` where ``key`` has a value, else ``missing`` (the visit of Boost's concurrent maps). With ``concurrent``, safe while others update.
   Only where the representation has it, as ``update``; any other map is read through ``find``.

.. cpp:function:: template<typename Combine> bool insert_or_combine(const key_type& key, const mapped_type& value, Combine combine)

   Inserts ``value`` where ``key`` has none, else replaces the held value by ``combine(held, value)`` (a hull of
   intervals, the lower of two costs); whether the map changed, as fixpoints ask.

Values by key (a multimap)
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. cpp:function:: auto values(const key_type& key) const

   The values of ``key``: a span for a posting map or a CSR. The standard's multimaps (``sorted``,
   ``stable_refs``) have none: their entries of a key are its ``equal_range``.

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
   shadows an outer one until its scope ends. Two scoped maps compare equal by the bindings in force, whatever scopes
   are open.

.. cpp:function:: std::size_t depth() const

   How many scopes are open. ``clear`` closes every scope.

.. cpp:function:: bool bound_in_scope(const key_type& key) const

   Whether ``key`` is bound and the innermost open scope bound it; with no scope open, whether it is bound at all.
   A redeclaration check: ``bound_in_scope(name)`` before declaring ``name`` in the current block.

Layers (``layered``)
^^^^^^^^^^^^^^^^^^^^

A layer is over a map by index (``dense``): a repository's per-predicate directory over a parent's.

.. cpp:function:: void layer_over(const Map* parent)

   Lookups find ``parent``'s values too, from now on; ``nullptr`` ends it. The Store holds, iterates and erases only
   its own values; inserting a key the parent holds inserts nothing. The parent is not owned: it must outlive the
   layer and stay unchanged. A layered Store has no ``==``: its contents include its parent's.

.. cpp:function:: const Map* parent() const noexcept

.. cpp:function:: const Map& owner(const key_type& key) const

   The Store along the chain whose own values hold ``key``: this one, or an ancestor. Some layer must hold it
   (``contains(key)``).

A key's own value shadows the parent's. ``insert_or_assign`` writes its own; ``at()``, ``update()`` and
``try_emplace(key, args...)`` on a layer that may change make an own value where the layer has none, once: where
values layer themselves (a map of layered pools layers through both levels, see :doc:`records`), one built from
``try_emplace``'s arguments (a pool's ``urd::bit_width`` or ``urd::column_widths``), or default, layered over the parent's value; else one built
from the arguments, or else a copy of the parent's. Read through a const Store (``at``, ``read``) to make nothing.

With ``concurrent``, ``at``, ``update``, ``read``, ``try_emplace``, ``contains`` and ``owner`` run from threads at
once: each own value is made and layered once, under its slot's lock, and threads then insert into it as the value
allows (a ``concurrent`` pool). ``clear``, ``erase``, ``insert_or_assign`` and iteration need no other operation
running.

Complexity
----------

.. urd-complexity:: maps

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
     - ``Store<sequence<Store<sequence<T>, frozen>>>`` (any depth)
     - ``Jagged``: CSR, one array per level
     - ``std::span<const T>``, or a range of the rows below
   * - fixed lengths, changing contents
     - ``Store<sequence<Store<sequence<T>>>, fixed_shape>``
     - ``Jagged`` built from :cpp:struct:`row_lengths`
     - ``std::span<T>``
   * - … of dense indices
     - ``Store<sequence<Store<set<I>, dense>>, fixed_shape>``
     - ``Jagged`` over one word array
     - a ``BitsetSpan`` (one bit per index)
   * - reused after removal
     - ``Store<sequence<Row>, retains>`` with a ``retains`` row
     - ``RowPool``
     - the row Store, cleared, not freed
   * - map values reused after erase
     - ``Store<map<K, Row>>`` with a ``retains`` row
     - ``Recycling`` over the map
     - the value Store, cleared, kept with its node
   * - interned, by handle
     - ``Store<pool<Row>>``
     - ``RowInterner``, tree pools
     - the row, read by handle (:doc:`pools`)
