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

Either pool is the records family: ``urd/use/records.hpp`` is included where the Store is held, ``urd/flat.hpp``
where a flat record is written.

.. code-block:: cpp

   #include <urd/flat.hpp>
   #include <urd/use/records.hpp>
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

.. cpp:function:: std::size_t size() const
.. cpp:function:: bool empty() const
.. cpp:function:: std::size_t memory_usage() const
.. cpp:function:: void clear()

   Shrinkable pools; the arena keeps its segments.

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

Families
--------

.. cpp:namespace:: urd

.. cpp:namespace-push:: compose

.. cpp:class:: Family

   One ``F<T>`` for each type in ``Ts``, reached by its type (``urd/compose/family.hpp``): a repository per kind of
   record, where ``F`` is usually an alias of a Store. Where the members number their elements (positional sets,
   pools), the family resolves an index of any member type, so a :cpp:class:`view\<T, Family\> <urd::view>` reads a record and
   the indices held in it through one context. Where the members layer, the family layers.

   .. cpp:function:: Family()
   .. cpp:function:: explicit Family(const Family* parent)

      A layer over ``parent`` (``nullptr``: a root): each member layered over the parent's member of its type.

   .. cpp:function:: template<typename T> F<T>& get()
   .. cpp:function:: template<typename T> const F<T>& get() const
   .. cpp:function:: template<typename Function> void for_each(Function function)
   .. cpp:function:: const Family* parent() const noexcept
   .. cpp:function:: std::size_t layer() const noexcept
   .. cpp:function:: template<typename T> decltype(auto) operator[](index<T> at) const

      The record of ``at``, from the member for ``T``, whichever layer holds it.

   .. cpp:function:: template<typename T> auto view(index<T> at) const

      Through the family that owns the record, so the views' identities are (index, layer).

   .. cpp:function:: template<typename T> std::pair<index<T>, bool> insert(const T& record)
   .. cpp:function:: template<typename T, typename Factory> std::pair<index<T>, bool> insert(const T& key, Factory make)
   .. cpp:function:: template<typename T> index<T> find(const T& record) const
   .. cpp:function:: template<typename T> bool contains(index<T> at) const
   .. cpp:function:: void clear()

.. cpp:namespace-pop::

Example: a repository of symbols
--------------------------------

.. code-block:: cpp

   struct Atom
   {
       urd::index<Predicate> predicate;
       urd::flat_vector<urd::index<Term>> terms;
       static constexpr auto identifying_members() noexcept { return urd::Members<&Atom::predicate, &Atom::terms> {}; }
       static constexpr auto cista_members() noexcept { return urd::Members<&Atom::predicate, &Atom::terms> {}; }
   };
   template<typename T>
   using Symbols = Store<value<T>, handles, unique, flat, monotone, layered, concurrent>;
   using Repository = compose::Family<Symbols, Object, Predicate, Term, Atom>;

   auto domain = Repository();
   auto task = Repository(&domain);                                // the task's symbols after the domain's
   const auto [atom, fresh] = task.insert(Atom { on, { block, x } });
   const auto seen = task.view(atom);                              // view<Atom, Repository>
   seen.get<&Atom::predicate>().get<&Predicate::name>();           // through the layer that owns the predicate
   for (const auto term : seen.get<&Atom::terms>()) /* view<Term, Repository> */;

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
