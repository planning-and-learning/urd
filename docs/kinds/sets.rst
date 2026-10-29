Sets
====

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename K, typename... Properties>
   using Set = Store<key<K>, Properties...>;        // written out as Store<key<K>, ...> in code

.. cpp:class:: Set

   A set: ``Store<key<K>, Properties...>``. The name ``Set`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Set

A set holds each key once (``unique`` is implied). Its order, keys, references, memory, threads and lifecycle are
what the request says; the registry picks the container (:doc:`../RESOLUTION`).

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Request
     - Representation
   * - ``Store<key<K>>``
     - ``boost::unordered_flat_set`` (open addressing)
   * - ``Store<key<K>, sorted>``
     - ``std::set``
   * - ``Store<key<K>, sorted, frozen>``
     - ``boost::container::flat_set`` (a sorted vector)
   * - ``Store<key<K>, stable_refs>``
     - ``std::unordered_set``
   * - ``Store<key<K>, stable_refs, retains>``
     - ``RecyclingSet`` over ``std::unordered_set`` (over ``std::set`` with ``sorted``): erased nodes are kept for the
       next elements
   * - ``Store<key<I>, dense>``
     - ``DenseSet`` (a bitset), with ``proportional_clear`` one that clears only the words it set
   * - ``Store<key<I>, dense, constant_clear>``
     - ``SparseSet`` (Briggs–Torczon)
   * - ``Store<key<K>, insertion_ordered>``, ``positional``
     - ``IndexedSet`` (a vector plus a hash table of positions); with ``monotone``, an interner
   * - ``Store<key<K>, positional, monotone, stable_refs>``
     - ``StableIndexedSet``: the elements in segments that never move, no lock
   * - ``Store<key<K>, concurrent>``
     - ``gtl::parallel_flat_hash_set`` (16 shards, a mutex each); with ``stable_refs``, ``parallel_node_hash_set``
       (a node per element); ``positional, monotone, concurrent``: ``ConcurrentIndexedSet``
   * - ``Store<key<K>, layered>``
     - ``Layered`` over a flat hash set; over a node set with ``stable_refs``
   * - ``Store<key<K>, change_log>``
     - ``ChangeLogged`` over a flat hash set; over a tracked bitset with ``dense``, a tree with ``sorted``, a node
       set with ``stable_refs``

Template parameters
-------------------

``K``
   The key type. :cpp:concept:`hashable` by default, :cpp:concept:`orderable` with ``sorted``,
   :cpp:concept:`indexable` with ``dense``. A Store whose contents fix its order (``sorted``, ``canonical``, a
   sequence) is a key; an unordered Store is not.

Member types
------------

.. list-table::
   :widths: 30 70

   * - ``representation``
     - the container chosen
   * - ``value_type``, ``key_type``, ``lookup_type``
     - ``K``
   * - ``mapped_type``
     - ``void``
   * - ``size_type``, ``difference_type``
     - ``std::size_t``, ``std::ptrdiff_t``
   * - ``iterator``, ``const_iterator``, ``reference``, ``const_reference``
     - the representation's; a set's elements never change in place, so ``iterator`` is ``const_iterator``
   * - ``index_type``
     - :cpp:struct:`index\<K\> <urd::index>` (``positional`` only)

Member functions
----------------

Construction and assignment
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. cpp:function:: Set()

   Empty. Not offered with ``nonempty``.

.. cpp:function:: template<std::input_iterator InputIt> Set(InputIt first, InputIt last)
.. cpp:function:: Set(std::initializer_list<value_type> elements)

   From any elements, each kept once. With ``nonempty``, asserts one.

.. cpp:function:: template<std::input_iterator InputIt> void assign(InputIt first, InputIt last)

   Replaces the contents by what a Store built from these elements would hold: the one way a ``frozen`` Store
   changes. A representation that rebuilds in place keeps its storage.

.. cpp:function:: void swap(Set& other) noexcept

Capacity
^^^^^^^^

.. cpp:function:: size_type size() const
.. cpp:function:: bool empty() const
.. cpp:function:: void reserve(std::size_t count)

   Hints how many elements will be inserted; does nothing where the representation cannot reserve. Growable
   Stores only (not ``frozen``).

Iterators
^^^^^^^^^

.. cpp:function:: const_iterator begin() const
.. cpp:function:: const_iterator end() const
.. cpp:function:: const_iterator cbegin() const
.. cpp:function:: const_iterator cend() const

   In the set's order: ascending with ``sorted`` (and for a ``dense`` bitset), in insertion order with
   ``insertion_ordered``, unspecified otherwise.

.. cpp:function:: auto rbegin() const
.. cpp:function:: auto rend() const

   With an order (``sorted`` or ``insertion_ordered``).

Lookup
^^^^^^

.. cpp:function:: const_iterator find(const key_type& key) const
.. cpp:function:: size_type count(const key_type& key) const
.. cpp:function:: bool contains(const key_type& key) const

   O(1) expected for hashed keys, O(log n) sorted, O(1) dense (a key that is no index finds nothing).

.. cpp:function:: const_iterator lower_bound(const key_type& key) const
.. cpp:function:: const_iterator upper_bound(const key_type& key) const
.. cpp:function:: std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const

   ``sorted`` only.

Modifiers
^^^^^^^^^

Growable Stores (not ``frozen``):

.. cpp:function:: std::pair<iterator, bool> insert(const value_type& element)
.. cpp:function:: std::pair<iterator, bool> insert(value_type&& element)
.. cpp:function:: iterator insert(const_iterator hint, const value_type& element)
.. cpp:function:: template<std::input_iterator InputIt> void insert(InputIt first, InputIt last)
.. cpp:function:: void insert(std::initializer_list<value_type> elements)
.. cpp:function:: template<typename... Args> std::pair<iterator, bool> emplace(Args&&... args)

   Inserts the element unless an equal one is held; the iterator to it and whether it was inserted. In an interner
   (``positional, monotone``) the element's position is its index and never shifts.

Shrinkable Stores (neither ``frozen`` nor ``monotone``):

.. cpp:function:: iterator erase(const_iterator pos)
.. cpp:function:: iterator erase(const_iterator first, const_iterator last)
.. cpp:function:: size_type erase(const key_type& key)
.. cpp:function:: void clear()
.. cpp:function:: template<typename Pred> friend size_type erase_if(Set& store, Pred pred)

   Found by argument-dependent lookup, as ``std::erase_if``. ``clear`` costs O(capacity), O(elements present)
   with ``proportional_clear``, O(1) with ``constant_clear``.

Set algebra
^^^^^^^^^^^

.. cpp:function:: bool includes(const Set& other) const

   Whether every element of ``other`` is in this set.

.. cpp:function:: bool intersects(const Set& other) const

   Whether the two share an element.

.. cpp:function:: bool unite(const Set& other)

   Inserts every element of ``other``; whether that added any, as fixpoints ask. Two bitsets do all three by
   words.

Positions (``positional``)
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. cpp:function:: const_reference operator[](std::size_t pos) const
.. cpp:function:: const_reference at(std::size_t pos) const
.. cpp:function:: const_reference front() const
.. cpp:function:: const_reference back() const
.. cpp:function:: std::size_t position(const_iterator it) const

   The position of the element at ``it``; ``size()`` for ``end()``, as ``find`` gives for an absent one.

.. cpp:function:: const_reference operator[](index_type at) const
.. cpp:function:: const_reference at(index_type at) const
.. cpp:function:: index_type index(const_iterator it) const

   The same, through :cpp:struct:`index\<K\> <urd::index>`: what an interner mints. ``at`` throws ``std::out_of_range`` for
   ``none`` or a position past the end.

.. cpp:function:: auto view(index_type at) const

   A :cpp:class:`view\<K, Store\> <urd::view>` of the element, through this Store (``urd/view.hpp`` included).

Layers (``layered``)
^^^^^^^^^^^^^^^^^^^^

.. cpp:function:: void layer_over(const Set* parent)

   Lookups find ``parent``'s elements too, from now on; ``nullptr`` ends it. The Store holds, iterates and erases
   only its own elements; inserting what the parent has inserts nothing. The parent is not owned: it must outlive
   the layer and stay unchanged.

.. cpp:function:: const Set* parent() const noexcept

Change log (``change_log``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. cpp:function:: std::span<const value_type> added() const
.. cpp:function:: std::span<const value_type> removed() const

   Exactly the elements that changed since ``clear_changes()``; an element added and removed again is in neither.
   Semi-naive evaluation works on ``added()`` each round.

.. cpp:function:: void clear_changes()

Comparison
^^^^^^^^^^

.. cpp:function:: friend bool operator==(const Set& lhs, const Set& rhs)

   Equal contents: by the representation's ``==`` where it has one, else element by element in iteration order.

.. cpp:function:: friend auto operator<=>(const Set& lhs, const Set& rhs)

   Lexicographic, where the Store has an order and its elements compare three ways.

Complexity
----------

n elements, U the key range (largest index + 1); \* amortized, ~ expected.

.. list-table::
   :header-rows: 1
   :widths: 30 10 10 10 14 14 12

   * - Representation
     - insert
     - lookup
     - erase
     - clear
     - iterate
     - memory
   * - ``boost::unordered_flat_set``
     - O(1)\*~
     - O(1)~
     - O(1)~
     - O(capacity)
     - O(capacity)
     - ~n/0.875 slots + 1 byte each; retains
   * - ``std::unordered_set``
     - O(1)\*~
     - O(1)~
     - O(1)~
     - O(n + buckets)
     - O(n)
     - a node per element; stable references
   * - ``std::set``
     - O(log n)
     - O(log n)
     - O(log n)
     - O(n)
     - O(n), in order
     - a node per element; reclaims
   * - ``RecyclingSet<Set>``
     - as Set, a kept node first
     - as Set
     - as Set; keeps the node
     - O(n)
     - as Set
     - erased nodes kept; stable references, retains
   * - ``boost::container::flat_set``
     - O(n)
     - O(log n)
     - O(n)
     - O(n)
     - O(n), contiguous
     - n elements; retains
   * - ``DenseSet`` (bitset)
     - O(1)\*
     - O(1)
     - O(1)
     - O(U/64); O(n) tracking its words
     - O(U/64 + n), in order
     - U bits; set algebra by words
   * - ``SparseSet``
     - O(1)\*
     - O(1)
     - O(1)
     - O(1)
     - O(n), contiguous
     - n keys + U positions
   * - ``IndexedSet``
     - O(1)\*~
     - O(1)~
     - O(n) (keeps order)
     - O(capacity)
     - O(n), contiguous
     - n elements + a table of positions
   * - ``StableIndexedSet``
     - O(1)\*~
     - O(1)~
     - —
     - O(capacity)
     - O(n)
     - segments that never move; no lock
   * - ``ConcurrentIndexedSet``
     - O(1)\*~ (new ones under a lock)
     - O(1)~ without a lock
     - —
     - O(capacity)
     - O(n)
     - segments that never move
   * - ``gtl::parallel_flat_hash_set``
     - O(1)\*~ under one shard's lock
     - O(1)~ under it
     - O(1)~
     - O(capacity)
     - O(capacity)
     - 16 shards
   * - ``gtl::parallel_node_hash_set``
     - O(1)\*~ under one shard's lock
     - O(1)~ under it
     - O(1)~
     - O(capacity)
     - O(capacity)
     - 16 shards, a node per element; stable references
   * - ``ChangeLogged``
     - O(1)\*~
     - O(1)~
     - O(1)~
     - O(n)
     - O(capacity)
     - + the elements added and removed
   * - ``Layered``
     - O(1)\*~
     - O(1)~ own + parent
     - O(1)~ own
     - O(capacity)
     - own elements
     - the parent is not owned

Example
-------

.. code-block:: cpp

   using Seen = Store<key<User>, dense>;                 // a bitset: User 17 is bit 17
   auto seen = Seen { start };
   if (seen.insert(next).second) frontier.push(next);

   using Names = Store<key<std::string>, positional, monotone>;   // an interner
   auto names = Names();
   const auto [it, fresh] = names.insert("block");
   const urd::index<std::string> block = names.index(it);         // 0
   names[block];                                                  // "block"
