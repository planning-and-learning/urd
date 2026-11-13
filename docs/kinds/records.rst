Records, repositories, views
============================

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename Record, typename... Properties>
   using Records = Store<pool<Record>, flat, Properties...>;
   template<typename T, typename... Properties>
   using Symbols = Store<pool<urd::data<T>, index<T>>, flat, Properties...>;   // records of T, handing out index<T>

.. cpp:class:: Records

   A pool of records stored flat: ``Store<pool<Record>, flat, Properties...>``. The name ``Records`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Records

Records are the pool kind (:doc:`pools`) holding values that are not rows: each distinct value once, interned whole.
Without ``flat``, any hashable value is interned as it is (``ValueInterner``, an indexed set of the values: a pool of
strings is ``Store<pool<std::string>>``). With ``flat``, records are stored as bytes:

A pool of records stored ``flat`` interns whole records: each distinct record once, as one self-contained block of
bytes, under the :cpp:struct:`index\<Record\> <urd::index>` ``insert`` gave it (or the index the request names,
``pool<urd::data<T>, index<T>>``: the record a Store holds for an ``index<T>``, whose views are ``index_view<T, Store>``);
read back by index, looked up by record, counted.
A record names its identity (:cpp:struct:`Members`); it is trivially copyable, or it names its members for cista
(``cista_members()``) and holds its sequences, text and variants as the flat containers of ``urd/elements/flat.hpp``. The
records are frozen once stored.

.. list-table::
   :header-rows: 1
   :widths: 50 50

   * - Request
     - Representation
   * - ``Store<pool<Record>, flat>``, a trivially copyable record
     - ``RecordInterner``: an indexed set of the records
   * - the same, any other record
     - ``ArenaInterner``: serialized by cista into a byte arena that never moves a block
   * - ``RecordInterner`` with ``stable_refs``
     - the records in segments that never move, no lock (the arena never moves a block, so it has ``stable_refs`` already)
   * - either with ``concurrent``
     - the same, thread-safe: lookups without a lock, a new record under the writer lock
   * - with ``layered``
     - ``LayeredPool`` over one that never moves a record: the arena, the record interner over segments, or a
       thread-safe one

Each pool has its header: ``urd/repr/records/records.hpp`` (the record interner) or ``urd/repr/records/arena.hpp`` (the arena
interner) is included where the Store is held, ``urd/elements/flat.hpp`` where a flat record is written.

.. code-block:: cpp

   #include <urd/elements/flat.hpp>
   #include <urd/repr/records/arena.hpp>
   struct Predicate
   {
       urd::flat_string name;
       std::uint32_t arity = 0;
       static constexpr auto identifying_members() noexcept { return urd::Members<&Predicate::name, &Predicate::arity> {}; }
       static constexpr auto cista_members() noexcept { return urd::Members<&Predicate::name, &Predicate::arity> {}; }
   };

Fields
------

A record may name its fields once: each a public name and the data member that holds it, in data order. Its
identifying members are then their members, and whatever reads a record field by field (serialization, printing,
Python bindings, a walk over its children) iterates the declaration instead of asking for each member, so that a field
is never left out.

.. code-block:: cpp

   struct Atom
   {
       urd::index<Predicate> predicate;
       urd::flat_vector<urd::index<Object>> objects;
       static constexpr auto fields() noexcept
       {
           return urd::Fields<urd::field<"predicate", &Atom::predicate>, urd::field<"objects", &Atom::objects>> {};
       }
       static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }
   };

.. cpp:struct:: template<fixed_string Name, auto Member> field

   ``name`` (a ``std::string_view``, null-terminated), ``member``, and ``prefixed<"get_">``, the name after a prefix.

.. cpp:function:: template<HasFields T, typename F> constexpr void for_each_field(F&& f)

   ``f.template operator()<Field>()`` for each field, in order. A field is read through a view as
   ``view.template member<Field::member>()``: resolved through the view's context, like any member.

Flat containers
---------------

.. cpp:type:: template<typename T> flat_vector = cista::offset::vector<T>
.. cpp:type:: flat_string = cista::offset::string
.. cpp:type:: template<typename... Ts> flat_variant = cista::offset::variant<Ts...>
.. cpp:type:: template<typename T> flat_optional = cista::optional<T>
.. cpp:type:: template<typename T1, typename T2> flat_pair = cista::pair<T1, T2>

   ``is_flat_vector<T>``, ``is_flat_variant<T>``, ``is_flat_optional<T>`` and ``is_flat_pair<T>`` say whether a type is
   one of them.

   With :cpp:struct:`Hash` and :cpp:struct:`EqualTo` (the same byte hash as ``std::string``, so equal text hashes
   alike), and resolved by views like their standard counterparts.

Member types
------------

``value_type`` is ``Record``; ``handle_type`` is :cpp:struct:`index\<Record\> <urd::index>`.

Member functions
----------------

.. cpp:function:: explicit Records(layer number)

   Empty, numbered (:cpp:struct:`urd::layer`): a pool of records is built only so, and its views are identified by
   index and number. A layer gets its own number, as its parent does.

.. cpp:function:: std::pair<handle_type, bool> insert(const value_type& record)

   The record's index, and whether it was new: an equal record already held gives its index.

.. cpp:function:: template<std::invocable<handle_type> Factory> std::pair<handle_type, bool> insert(const value_type& key, Factory make)

   The index ``key`` gets, and whether it was new: where it is, the record stored is what ``make(index)`` gives,
   built with the index reserved for it (a record that holds its own index). It must equal ``key``, and ``make``
   must not touch this Store.

.. cpp:function:: handle_type find(const value_type& record) const

   The index of an equal record, or ``none``.

.. cpp:function:: bool contains(const value_type& record) const
.. cpp:function:: bool contains(handle_type at) const
.. cpp:function:: const value_type& operator[](handle_type at) const
.. cpp:function:: const value_type& at(handle_type at) const

   ``std::out_of_range`` where there is no record at ``at``.

.. cpp:function:: index_view<Record, Store> view(handle_type at) const

   A :cpp:class:`index_view\<Record, Store\> <urd::index_view>` of the record through this Store; of a layered pool, through the layer that
   owns it.

.. cpp:function:: std::size_t layer() const

   The number it was built with (:cpp:struct:`urd::layer`): a pool of records has no constructor without one.

.. cpp:function:: std::size_t count(const value_type& record) const
.. cpp:function:: const_iterator begin() const
.. cpp:function:: const_iterator end() const

   The records in index order, O(n). Not where ``layered``: there each record is reached through the layer that owns
   it, so walk the indices.

.. cpp:function:: std::size_t size() const
.. cpp:function:: bool empty() const
.. cpp:function:: void clear()

   Shrinkable or ``monotone_until_clear`` pools; the arena keeps its segments.

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
.. cpp:function:: std::size_t base() const

   Where this layer's own indices start: the parent's size at the branch.

   The parent must not grow after the branch: inserting a new element then throws ``std::logic_error``.

.. cpp:function:: const Records& owner(handle_type at) const

   The Store along the chain that holds the element at ``at``.

Relations: a map of layered pools
---------------------------------

A map whose values layer themselves layers through both levels. The relations of a planning task, a map from
predicate index to the pool of rows of object indices bound to it, fall through to the previous phase's pool for a
predicate the phase did not touch, and continue that pool's indices for one it did:

.. code-block:: cpp

   using Row = Store<sequence<index<Object>>, frozen>;
   using Rows = Store<pool<Row>, monotone, layered>;
   using Relations = Store<map<index<Predicate>, Rows>, dense, layered>;
   auto task = Relations();
   task.layer_over(&domain);
   task[on].insert({ a, b });     // the domain's row, if it had one, else a new one after the domain's rows
   task.owner(on);                // the Store whose own pool holds on's rows: task now

Rows of a width given at construction (``bit``) are built from it: ``try_emplace`` builds a predicate's own pool from
its arguments and layers it over the parent's, which may be narrower. With ``concurrent`` at both levels, threads
reach and fill the pools at once, each own pool made and layered once (:doc:`maps`):

.. code-block:: cpp

   using BitRows = Store<pool<Row, index<Binding>>, monotone_until_clear, bit, layered, concurrent>;
   using Relations = Store<map<index<Predicate>, BitRows>, dense, layered, concurrent>;
   task.try_emplace(on, bit_width::for_bound(num_objects));   // made once, over the domain's pool of on
   task.at(on).insert({ a, b });                              // from any thread

Rows by column widths (``by_column_widths``, :doc:`pools`) are built the same way, from ``try_emplace(on, widths)``; a parent's row
in narrower columns is found through the child's wider ones.

A predicate's pool whose Store offers ``clear`` (``monotone_until_clear``) is reused when the map is cleared and the
slot made again at the same width (or equal column widths): cleared and layered anew, so refilling allocates nothing. A
``monotone`` pool has no ``clear`` and is built anew.
