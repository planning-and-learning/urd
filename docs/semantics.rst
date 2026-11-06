Elements: identity, hashing, order, indices
============================================

.. cpp:namespace:: urd

What a container needs of its elements comes from three function objects, with nothing in namespace ``std``:

.. cpp:struct:: template<typename T> Hash

   ``hash_t operator()(const T&) const``: a deterministic 64-bit hash (xxHash64 over bytes, Boost.ContainerHash
   mixing for composites). Given for arithmetic types, enums, strings and string views, the standard's composites
   (``pair``, ``tuple``, ``array``, ``vector``, ``set``, ``map``, ``optional``, ``variant``, ``span``), any
   :cpp:concept:`Identifiable` type, :cpp:struct:`urd::index`,
   :cpp:class:`urd::view`, the flat containers of ``urd/elements/flat.hpp``, any range whose elements in order are its
   value (``urd::value_range<R> = true``: hashed, compared and ordered as its elements), and Stores whose contents fix
   their order (``sorted``, ``canonical``, sequences). There is deliberately no fallback to ``std::hash``.

.. cpp:struct:: template<typename T> EqualTo

   ``bool operator()(const T&, const T&) const``: equality agreeing with :cpp:struct:`Hash`; for floating point,
   NaN equals NaN. Given for any iterable Store (equal contents, as its ``==`` compares them).

.. cpp:struct:: template<typename T> Less

   ``bool operator()(const T&, const T&) const``: a strict weak order agreeing with :cpp:struct:`EqualTo`,
   lexicographic over members and ranges. Given for Stores iterated in an order their request fixes (``sorted``,
   ``insertion_ordered``, sequences), so such a Store can be a sorted key.

Which of the three a Store has is decided by its request, never by its representation: a Store named as another
Store's element is resolved before its representation is defined.

A type names its identity once and is then hashable, comparable and ordered, in member order:

.. cpp:struct:: template<auto... Ms> Members

   The identifying members of a record, as member pointers.

.. code-block:: cpp

   struct Edge
   {
       Vertex from, to;
       static constexpr auto identifying_members() noexcept { return Members<&Edge::from, &Edge::to> {}; }
   };
   using Edges = Store<set<Edge>>;                    // hashed by (from, to)

.. cpp:concept:: template<typename T> MemberIdentified

   ``T::identifying_members()`` gives a :cpp:struct:`Members`.

.. cpp:concept:: template<typename T> Identifiable

   ``T`` is :cpp:concept:`MemberIdentified`, or its ``identifying_members()`` gives its identity as a value (a
   :cpp:class:`urd::view` gives a tuple). Hash, EqualTo and Less of ``T`` are those of that identity.

.. cpp:concept:: template<typename T> hashable

   :cpp:struct:`Hash` and :cpp:struct:`EqualTo` apply to ``T``: what an unordered key needs.

.. cpp:concept:: template<typename T> orderable

   :cpp:struct:`Less` and :cpp:struct:`EqualTo` apply to ``T``: what a ``sorted`` key or a ``by_priority``
   element needs.

.. cpp:concept:: template<typename K> indexable

   :cpp:struct:`index_of` applies to ``K``: what a ``dense`` key needs.

.. cpp:struct:: template<typename K> index_of

   How a key is an index 0..n-1: ``is_index(key)`` (a negative integer, or :cpp:struct:`urd::index`'s none, is no
   index, and a lookup of one finds nothing), ``to_index(key)`` and ``from_index(std::size_t)``. Given for
   non-negative integers, enums, types identified by one integral member, and :cpp:struct:`urd::index`; specialize it
   for others.

Prehashed keys
--------------

A key hashed once can be looked up, and inserted into an interner, without being hashed again: the standard's way to
pass a precomputed hash. C++20 had dedicated overloads for it (P0920) and dropped them (P1661) for heterogeneous
lookup by a key that carries its hash, which is what urd offers; there are no ``*_with_hash`` members.

.. cpp:struct:: template<typename T> prehashed

   ``const T& key`` and ``hash_t hash``, :cpp:struct:`Hash`'s of ``key``. It refers to ``key``, which must outlive it.

.. cpp:function:: template<typename T> prehashed<T> prehash(const T& key)

   ``key`` with its hash. Not of a temporary (that overload is deleted): the result would refer to it once it is gone.

A hashed Store's ``find``, ``contains`` and ``count`` take a ``prehashed<T>`` wherever they take a ``T`` (the hash
sets and maps, the parallel ones, the positional sets, the record, value and arena interners), and an interner's
``insert(key)`` and ``insert(key, make)`` do too. An interner hashes a new element once, its insert included: the
lookup's hash goes into the table with the new index.

Indices and views
-----------------

.. cpp:struct:: template<typename T> index

   The index of a ``T`` in the Store that numbers ``T``'s (``urd/elements/index.hpp``): a positional set mints one on first
   insert, and a pool's handles are indices. One 32-bit integer (``value``) with a ``none`` value
   (``is_none()``), a different type per ``T`` that converts to no other, hashable, orderable, and a dense key.
   Write it ``urd::index``: the C library declares a function ``index`` in the global namespace.

   .. cpp:member:: value_type value

      The number; ``none_value`` by default.

   .. cpp:function:: static index none() noexcept
   .. cpp:function:: bool is_none() const noexcept

.. cpp:struct:: layer

   The number of a Store that hands out indices for views (a pool of records, a positional set): such a Store is
   built from it first, ``Atoms(urd::layer { 3 })``, and answers it as ``layer()``. Who builds the Store gives it, and
   Stores whose views must differ get different numbers; a layer gets its own, as its parent does.

   .. cpp:member:: std::size_t number

.. cpp:class:: template<typename T, typename C = typename context_of<T>::type, typename H = index<T>> view

   A ``T`` read through a context (``urd/elements/view.hpp``): what locates it and a pointer to a context ``C`` that
   resolves it (:cpp:concept:`resolves`). A positional set and a pool of records resolve their own type; any type
   that gives the record, its number and the view of an index serves. The context must outlive the view.

   It holds what reads the record, its third parameter ``H``: one of the holding types below. By default the index
   (``index_view``), read through the context; ``resolve`` makes the views of records held in place (borrowed), and
   the context those of its indices. The view itself has no cases: ``data()`` is ``held.read(context)``.

   .. cpp:function:: view(H held, const C& context)

      ``held`` converts from what the holding type holds (an index, a record, a ``std::shared_ptr``); a
      ``borrowed<R>`` only from an lvalue.

   .. cpp:function:: H index() const noexcept

      Only where it holds a handle (:cpp:concept:`handle`); a view of a record names none, so that the record's
      mixin may.

   .. cpp:function:: const C& context() const noexcept
   .. cpp:function:: decltype(auto) data() const

      The record, as the holding type reads it: for a handle as the context holds it (a ``T``, or its stored record
      ``data<T>``), else the record held.

   .. cpp:function:: template<auto Member> decltype(auto) get() const

      A member of the record resolved through the context (:cpp:func:`resolve`): an ``index<U>`` becomes the
      ``view<U, C>`` the context makes, pointing at the layer that owns the element; a range of indices a range of
      views; a variant the variant of its alternatives resolved; an optional the optional of its value resolved;
      anything else stays as it is.

   .. cpp:function:: auto identifying_members() const noexcept

      The identity: what it holds (a handle itself, a record holder by what identifies its record), and the context's number
      (:cpp:struct:`layer`). Two views of one element from one
      context are equal under ``EqualTo`` and hash equally, wherever they were made; views from differently numbered
      contexts are not. ``==`` and ``<=>`` compare by it, and so do ``EqualTo``, ``Hash`` and ``Less``.

.. cpp:concept:: template<typename C, typename T> resolves

   ``context[index<T>]`` is convertible to ``const T&``, or is the stored record ``data<T>`` (the slot of
   ``urd/elements/data.hpp``, which must be Serializable); ``context.layer()`` is its number; ``context.view(index<T>)``
   gives the ``view<T, C>`` of the index, through the context that owns it (a layer points it at the layer that owns
   the element; a context made of several Stores writes one per type).

.. cpp:concept:: template<typename H> handle

   ``H`` locates a record in a context, which resolves it: it names what it locates (``using located = T;``) and reads
   it, ``h.read(context)`` being ``context[h]``. ``index<T>`` is one; a library's composite handle (a relation and a
   row) declares the same.

.. cpp:struct:: template<typename R> borrowed
.. cpp:struct:: template<typename R> shared
.. cpp:struct:: template<typename R> owned

   The holding types of a record ``R`` (``data<T>``, a ``builder<T>``, or another form of ``T``), each naming it
   (``record_type``) and reading it, ``read(context)``, regardless of the context:

   - ``borrowed<R>``: a record stored elsewhere (in place in a record, a caller's, a builder), which must outlive the
     view. Built from an lvalue only: a temporary is refused, and so is ``resolve`` of one.
   - ``shared<R>``: a record the view shares (``std::shared_ptr<const R>``), released with its last holder.
   - ``owned<R>``: a record computed on the fly, owned by value; a copy of the view copies it.

   Each compares and hashes by its record (its identity), so views of equal records from one context are equal.
   ``index_view<T, C>``, ``borrowed_view<T, C, R = data<T>>``, ``shared_view<T, C, R>`` and ``owned_view<T, C, R>``
   name the views of each.

.. cpp:function:: template<typename M, typename C> decltype(auto) resolve(M&& member, const C& context)

   A record's member as a view sees it: ``resolution<M>::of(member, context)``, the member passed on as given (so
   that a record or builder, which its resolution borrows, is refused as a temporary); see :cpp:func:`view::get`.

.. cpp:struct:: template<typename M> resolution

   How a member of type ``M`` resolves: as it is, unless specialized. Given for handles (:cpp:concept:`handle`: ``index<U>``, or a type naming what it locates; the context's view),
   ranges of indices (a range of views), ``std::variant`` and ``std::optional`` (their contents resolved), and in
   ``urd/elements/flat.hpp`` for cista's variant and optional. A new member type resolves by specializing it.

.. cpp:struct:: template<typename T> context_of

   Names a record type's default context once (``template<> struct urd::context_of<Atom> { using type =
   Repository; };``), so that ``view<Atom>`` needs no second argument.

.. cpp:struct:: template<typename T, typename V> view_mixin

   Where a library adds the named accessors of a record's views, one line each over ``get<>``: ``template<typename
   V> struct urd::view_mixin<Atom, V> { auto predicate() const { return static_cast<const V&>(*this).template
   get<&urd::record_of<V>::predicate>(); } };``, where ``record_of<V>`` is the record the view reads (named through
   ``V``, so it may be incomplete where the mixin is declared). The mixin is keyed on what the holding type reads:
   views of an index and of a held record of ``T`` share ``T``'s mixin; views holding a builder use ``builder<T>``'s,
   which is ``T``'s unless specialized. The view's
   ``get<&T::m>()`` hides any ``get`` a mixin would declare: a mixin names its accessors otherwise.
