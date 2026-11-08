Pools
=====

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename Row, typename... Properties>
   using Interned = Store<pool<Row>, Properties...>;   // rows, each stored once, by handle
   template<typename Row, typename Handle, typename... Properties>
   using Named = Store<pool<Row, Handle>, Properties...>;      // the same, handing out the index Handle
   template<typename T, typename... Properties>
   using Slots = Store<slots<T>, Properties...>;                // elements in slots that never move

.. cpp:class:: Pool

   A pool: ``Store<slots<Row>, Properties...>``, interning with ``unique``. The name ``Pool`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Pool

A pool is one kind (``pool``, ``handles`` with ``unique``) whatever it holds: rows (a Store or a registered
container of elements) are interned as rows, on this page; values that are not rows are interned whole, on
:doc:`records`. Slots (``handles`` without ``unique``) are a kind of their own.

A pool is reached by handle, not iterated. A handle is an :cpp:struct:`index\<Row\> <urd::index>`, or the index the
request names, ``pool<V, H>`` (an ``urd::index`` of any type: a pool of ``urd::data<T>`` handing out ``index<T>``,
rows handing out a repository's own row index): a pool of rows kept in full numbers them 0..size()-1, so a pool without ``structural_sharing`` is counted and walked by its handles. A pool
with ``structural_sharing`` (a tree pool) shares nodes between rows and keeps no list of them: it has a handle of its
own, and no ``size``. Every handle type has a none value (``is_none()``), which ``find`` and ``erase`` give where there is
no row to name, as a pool of records does.

.. list-table::
   :header-rows: 1
   :widths: 50 50

   * - Request
     - Representation
   * - ``Store<pool<Row>>``
     - ``RowInterner``: each distinct row once, in full; rows of dense indices as bitset words
   * - ``Store<pool<Row>, structural_sharing>``
     - ``TreeVectorSet`` (sequence rows), ``TreeBitsetSet`` (rows of dense indices): equal subtrees stored once
   * - ``Store<slots<T>>``
     - ``SlotVector``: stable slots, freed ones reused
   * - any of them with ``concurrent``
     - the same, thread-safe
   * - ``Store<pool<Row>, bit>``
     - the bit row interner: rows of indices, each element in the ``urd::bit_width`` the pool is built with
   * - ``Store<pool<Row>, by_column_widths>``
     - the schema row interner: rows of unsigned columns stored by the ``urd::column_widths`` the pool is built with
       (below)
   * - ``Store<pool<Row>, layered>``
     - ``LayeredPool`` over the row interner (:doc:`records`); with ``bit`` or ``by_column_widths``, over the bit or
       schema row interner, built with its width or column widths

Template parameters
-------------------

``Row``
   A Store (``Store<sequence<T>, frozen>``, ``Store<set<I>, dense, frozen>``), or any range constructible from its
   elements. The Row's own rules decide which rows are equal: a set's in any order, once.

``H``
   The handle, ``index<Row>`` by default: any ``urd::index``. Pools that number their rows hand it out; a tree pool,
   whose handle is its own, takes only the default (a ``static_assert``).

Member types
------------

.. list-table::
   :widths: 30 70

   * - ``value_type``
     - ``Row``
   * - ``handle_type``
     - ``H`` (:cpp:struct:`index\<Row\> <urd::index>` by default), or the tree pools' handle
   * - ``write_type``
     - ``Write<element>``: a change to a sequence row, its value at a position

Member functions: interning pools (``handles, unique``)
--------------------------------------------------------

.. cpp:function:: template<std::ranges::input_range Elements> std::pair<handle_type, bool> insert(const Elements& elements)
.. cpp:function:: std::pair<handle_type, bool> insert(std::initializer_list<element> elements)

   The handle of the row with these elements, and whether the insert stored anything: ``false`` where an equal row is
   held (with ``structural_sharing``, where every node of it is), exactly where ``find`` would have given the handle.
   A tree pool of dense sets takes a row that keeps its bits (``significant_words()``, :doc:`sets`) by its 64-bit
   words, read as its blocks in place: no element listed, nothing allocated. ``find`` likewise.

.. cpp:function:: template<std::ranges::input_range Elements> handle_type find(const Elements& elements) const
.. cpp:function:: handle_type find(std::initializer_list<element> elements) const

   The handle ``insert(elements)`` would give, where inserting would store nothing; else none. Without
   ``structural_sharing``, an equal row inserted before. A tree pool also finds a row it can build from the nodes it
   holds, such as a part of a longer row.

.. cpp:function:: std::size_t size() const
.. cpp:function:: bool empty() const

   Without ``structural_sharing``: the distinct rows, whose handles are 0..size()-1.

.. cpp:function:: handle_type erase(handle_type handle)

   Without ``structural_sharing``, ``monotone``, ``concurrent`` or ``layered``: erases the row by moving the last row
   into its place, which then has ``handle``; gives the handle the moved row had, or none where the erased row was the
   last. The moved row's views stay valid. Whatever keeps handles (postings over the rows, counts) follows the move.

.. cpp:function:: std::pair<handle_type, bool> insert(handle_type base, std::span<const write_type> writes)

   Sequence rows: the row ``base`` with ``writes`` applied (sorted by position, each once and within the row). A
   pool that shares structure stores only what changed.

.. cpp:function:: std::pair<handle_type, bool> insert(handle_type base, std::span<const element> added, std::span<const element> removed)

   Rows that are sets: ``base`` with ``added`` put in and then ``removed`` taken out.

.. cpp:function:: bool contains(handle_type handle, const element& value) const

   Rows that are sets; a tree pool reads one path, not the row.

.. cpp:function:: value_type get(handle_type handle) const
.. cpp:function:: std::size_t row_size(handle_type handle) const
.. cpp:function:: void read(handle_type handle, std::span<element> out) const

   The row's elements into ``out``, which must hold ``row_size(handle)`` of them; allocates nothing (a tree pool
   reads them, and counts them for ``row_size``, along the tree).

.. cpp:function:: void read(handle_type handle, value_type& out) const

   A tree pool of dense sets: the row into ``out`` word by word (``assign_words``, :doc:`sets`), its storage reused;
   allocates nothing where ``out`` holds the row's words already.

.. cpp:function:: auto view(handle_type handle) const

   The stored row without copying, where the pool keeps rows in full: a span of the elements, or for a dense set its
   bits (``BitsetSpan``). Valid until the pool is cleared.

.. cpp:function:: void clear()

   Forgets every row; the handles given so far mean nothing after; a layered pool its own layer's. Shrinkable or
   ``monotone_until_clear`` pools.

Member functions: slots (``handles`` without ``unique``)
--------------------------------------------------------

.. cpp:namespace:: urd

.. cpp:class:: Slots

   A pool of slots: ``Store<slots<T>, Properties...>``. The name ``Slots`` stands for the resolved Store
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

``acquire_unique()`` and ``acquire_shared()`` own an acquired slot as ``std::unique_ptr`` and ``std::shared_ptr`` do:
the slot goes back with its (last) owner, and once as many owners have lived at once nothing allocates.

With ``keyed<K>`` (single-threaded), slots are acquired only by key: ``acquire_shared(key, args...)`` hands out a slot
given back under an equal key (``urd::Hash``, ``urd::EqualTo``) as it is, else a new ``T(key, args...)``. Each slot
keeps its key and goes back to its key's free list, so a slot is never reused for another key (a buffer shaped by its
column widths keeps its shape); once each key has had as many owners at once, nothing allocates.

.. code-block:: cpp

   using Buffers = Store<slots<Rows>, keyed<column_widths>>;
   auto buffers = Buffers();
   auto rows = buffers.acquire_shared(column_widths { 8, 16 });   // Rows(widths), or one given back under equal widths

Complexity
----------

.. urd-complexity:: pools

Measured, not only bounded: for 20,000 versions of a row that each differ from an earlier one in 3 positions, a
tree pool takes 136–271 bytes per version against 447–4,409 for rows stored in full (:doc:`../BENCHMARKS`).

Example
-------

.. code-block:: cpp

   using States = Store<pool<Store<set<Atom>, dense, frozen>>, structural_sharing>;
   auto states = States();
   const auto initial = states.insert(facts);                                 // index<...>
   const auto next = states.insert(initial, added, removed);                  // a delta: only the changed path is new
   states.contains(next, atom);

Rows by column widths
---------------------

With ``by_column_widths`` a pool is built with an ``urd::column_widths`` (``urd/repr/containers/schema_rows.hpp``):
its columns, each the bits (1 to 64) of the unsigned value it holds. urd knows no types: what a column's value means
(an index, a number's bits, a label) is the user's. Rows lie back to back, each the widths' ``bits()``.

.. code-block:: cpp

   using Objects = Store<pool<Store<sequence<index<Object>>, frozen>>, by_column_widths>;
   auto rows = Objects(column_widths(3, 10));        // three objects of 10 bits per row
   rows.insert(std::vector { a, b, c });             // any range of values, one per column
   rows.view(h).get<index<Object>>(0);               // a row_type (SchemaRow): get(i), get<T>(i), as<T>(), size()

   auto row = Objects::row_builder_type(rows.column_widths());   // an EncodedRow
   row.set(0, 7);                                                 // false where 7 does not fit the column
   rows.find(row.row());

A row goes in as any range of values, one per column: unsigned integers or indices (``index_of``); or as a row of
equal widths, taken as it is. A value that does not fit its column throws ``std::out_of_range``, a count other than the
columns' ``std::invalid_argument``, before anything is stored. ``view(h)`` hands out the Store's ``row_type`` (a
``SchemaRow``): ``get(i)`` (the column's unsigned value), ``get<T>(i)`` (as an unsigned integer or index type),
``as<T>()`` (the columns as a random-access range of ``T``), ``size()``. The Store's ``row_builder_type`` (an
``EncodedRow``) builds a row column by column (``set``, ``copy`` from another row) to look up or insert.

A row's padding is zero. A row hashes and compares by its values (``urd::hash_columns``), whatever the bits of its
columns, so a layered pool finds a parent's row through wider columns, and a key hashed once (``prehashed``, with
``hash_columns`` of the values) serves the whole chain.

Some rows of a pool
-------------------

``urd::subset_view<Pool>`` (``urd/elements/subset_view.hpp``) is some rows of a pool, by their sorted handles: it
borrows the pool and the handles, which must outlive it. ``size()``, ``empty()``, ``operator[](i)`` (the pool's
``view`` of the i-th handle) and ``begin()``/``end()`` (a borrowed random-access range of those views) read them;
``contains(key)`` finds the row in the pool (by values, or a stored row) and then its handle among these by binary
search. Over a layered pool, the handles may be a parent's.

.. code-block:: cpp

   const auto subset = urd::subset_view(rows, std::span(sorted_handles));
   subset.contains(std::array<std::uint64_t, 2> { 3, 300 });
