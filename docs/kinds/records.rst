Records, repositories, views
============================

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename Record, typename... Properties>
   using Records = Store<value<Record>, handles, unique, flat, Properties...>;

.. cpp:class:: Records

   A pool of records stored flat: ``Store<value<Record>, handles, unique, flat, Properties...>``. The name ``Records`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Records

A pool of records stored ``flat`` interns whole records: each distinct record once, as one self-contained block of
bytes, under the :cpp:struct:`index\<Record\> <urd::index>` ``insert`` gave it; read back by index, looked up by record, counted.
A record names its identity (:cpp:struct:`Members`); it is trivially copyable, or it names its members for cista
(``cista_members()``) and holds its sequences, text and variants as the flat containers of ``urd/flat.hpp``. The
records are frozen once stored.

.. list-table::
   :header-rows: 1
   :widths: 50 50

   * - Request
     - Representation
   * - ``Store<value<Record>, handles, unique, flat>``, a trivially copyable record
     - ``RecordInterner``: an indexed set of the records
   * - the same, any other record
     - ``ArenaInterner``: serialized by cista into a byte arena that never moves a block
   * - ``RecordInterner`` with ``stable_refs``
     - the records in segments that never move, no lock (the arena never moves a block, so it has ``stable_refs`` already)
   * - either with ``concurrent``
     - the same, thread-safe: lookups without a lock, a new record under the writer lock
   * - either with ``layered``
     - ``LayeredPool`` over it

Each pool has its header: ``urd/repr/records.hpp`` (the record interner) or ``urd/repr/arena.hpp`` (the arena
interner) is included where the Store is held, ``urd/flat.hpp`` where a flat record is written.

.. code-block:: cpp

   #include <urd/flat.hpp>
   #include <urd/repr/arena.hpp>
   struct Predicate
   {
       urd::flat_string name;
       std::uint32_t arity = 0;
       static constexpr auto identifying_members() noexcept { return urd::Members<&Predicate::name, &Predicate::arity> {}; }
       static constexpr auto cista_members() noexcept { return urd::Members<&Predicate::name, &Predicate::arity> {}; }
   };

Flat containers
---------------

.. cpp:type:: template<typename T> flat_vector = cista::offset::vector<T>
.. cpp:type:: flat_string = cista::offset::string
.. cpp:type:: template<typename... Ts> flat_variant = cista::offset::variant<Ts...>

   With :cpp:struct:`Hash` and :cpp:struct:`EqualTo` (the same byte hash as ``std::string``, so equal text hashes
   alike), and resolved by views like their standard counterparts.

Member types
------------

``record_type`` is ``Record``; ``index_type`` and ``handle_type`` are :cpp:struct:`index\<Record\> <urd::index>`.

Member functions
----------------

.. cpp:function:: std::pair<index_type, bool> insert(const record_type& record)

   The record's index, and whether it was new: an equal record already held gives its index.

.. cpp:function:: template<std::invocable<index_type> Factory> std::pair<index_type, bool> insert(const record_type& key, Factory make)

   The index ``key`` gets, and whether it was new: where it is, the record stored is what ``make(index)`` gives,
   built with the index reserved for it (a record that holds its own index). It must equal ``key``, and ``make``
   must not touch this Store.

.. cpp:function:: index_type find(const record_type& record) const

   The index of an equal record, or ``none``.

.. cpp:function:: bool contains(const record_type& record) const
.. cpp:function:: bool contains(index_type at) const
.. cpp:function:: const record_type& operator[](index_type at) const
.. cpp:function:: const record_type& at(index_type at) const

   ``std::out_of_range`` where there is no record at ``at``.

.. cpp:function:: auto view(index_type at) const

   A :cpp:class:`view\<Record, Store\> <urd::view>` of the record through this Store; of a layered pool, through the layer that
   owns it.

.. cpp:function:: std::size_t count(const record_type& record) const
.. cpp:function:: const_iterator begin() const
.. cpp:function:: const_iterator end() const

   The records in index order, O(n). Not where ``layered``: there each record is reached through the layer that owns
   it, so walk the indices.

.. cpp:function:: std::size_t size() const
.. cpp:function:: bool empty() const
.. cpp:function:: std::size_t memory_usage() const
.. cpp:function:: void clear()

   Shrinkable pools; the arena keeps its segments.

Complexity
----------

.. urd-complexity:: records

Layered pools (``layered``)
---------------------------

A pool of records or rows may be layered: a layer branches from a parent while it holds nothing of its own, its
indices continue after the parent's size at the branch, so an index means the same in every layer below; a lookup
answers from the layer, then from the parent's chain; an insert finds its record anywhere in the chain before adding
it here; reading by index goes to the layer that owns it. The parent must outlive the layer and not grow after the
branch, which inserts assert. The tree pools do not layer, since they number nodes, not rows.

.. cpp:function:: void layer_over(const Records* parent)
.. cpp:function:: const Records* parent() const noexcept
.. cpp:function:: std::size_t layer() const

   This layer's number along the chain: 0 for a root.

.. cpp:function:: std::size_t base() const

   Where this layer's own indices start: the parent's size at the branch.

.. cpp:function:: bool parent_grew() const
.. cpp:function:: const Records& owner(index_type at) const

   The Store along the chain that holds the element at ``at``.

Relations: a map of layered pools
---------------------------------

A map whose values layer themselves layers through both levels. The relations of a planning task, a map from
predicate index to the pool of rows of object indices bound to it, fall through to the previous phase's pool for a
predicate the phase did not touch, and continue that pool's indices for one it did:

.. code-block:: cpp

   using Row = Store<value<index<Object>>, frozen>;
   using Rows = Store<value<Row>, handles, unique, monotone, layered>;
   using Relations = Store<key<index<Predicate>>, value<Rows>, dense, layered>;
   auto task = Relations();
   task.layer_over(&domain);
   task[on].insert({ a, b });     // the domain's row, if it had one, else a new one after the domain's rows
