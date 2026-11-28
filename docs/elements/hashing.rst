Hashing and equality
====================

.. cpp:namespace:: urd

.. cpp:struct:: template<typename T> Hash

   ``hash_t operator()(const T&) const``: a deterministic 64-bit hash (scalars as themselves, Boost.ContainerHash
   mixing for composites). Given for arithmetic types, enums, strings and string views, the standard's composites
   (``pair``, ``tuple``, ``array``, ``vector``, ``set``, ``map``, ``optional``, ``variant``, ``span``), any
   :cpp:concept:`Identifiable` type, :cpp:type:`urd::index`,
   the views (:cpp:class:`urd::index_view` and the record views), the flat containers of ``urd/elements/flat.hpp``, any range whose elements in order are its
   value (``urd::value_range<R> = true``: hashed, compared and ordered as its elements), and Stores whose contents fix
   their order (``sorted``, ``canonical``, sequences). There is deliberately no fallback to ``std::hash``.

.. cpp:struct:: template<typename T> EqualTo

   ``bool operator()(const T&, const T&) const``: equality agreeing with :cpp:struct:`Hash`; for floating point,
   NaN equals NaN. Given for any iterable Store (equal contents, as its ``==`` compares them).

.. cpp:concept:: template<typename T> hashable

   :cpp:struct:`Hash` and :cpp:struct:`EqualTo` apply to ``T``: what an unordered key needs.

By content
----------

A view hashes and compares by its identity (its record's identity and its context's number): the views of a builder
and of the record it interns to are different elements. Their content is compared instead through
``urd/elements/content.hpp``.

.. cpp:function:: template<View V> hash_t hash_content(const V& view)

   The hash of what ``view`` reads: its record form's identifying members (:cpp:func:`identifying_members_of`
   ``<record_of<V>>``), each read through the view, combined as :cpp:struct:`Hash` combines an identified type's. A
   member that reads as a view (a child, a builder or record held in place) is taken by its content in turn, the form a
   child holds by the view of that form; a range of such by its elements', in order; any other value by its
   :cpp:struct:`Hash`. So a builder, a record held in place and an interned record of the same content hash alike,
   read through any contexts; a builder's view hashes as :cpp:struct:`Hash` hashes the builder where none of its
   identifying members is a child.

.. cpp:function:: template<View V, View W> bool equal_content(const V& lhs, const W& rhs)

   Whether two views of one kind read the same content (:cpp:func:`hash_content`'s): their record forms' identifying
   members, by position, read through each view; values of one type by their :cpp:struct:`EqualTo`, ranges of values
   of different types element by element.

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
