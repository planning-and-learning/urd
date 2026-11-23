Indices
=======

.. cpp:namespace:: urd

.. cpp:concept:: template<typename K> indexable

   :cpp:struct:`index_of` applies to ``K``: what a ``dense`` key needs.

.. cpp:struct:: template<typename K> index_of

   How a key is an index 0..n-1: ``is_index(key)`` (a negative integer, or :cpp:type:`urd::index`'s none, is no
   index, and a lookup of one finds nothing), ``to_index(key)`` and ``from_index(std::size_t)``. Given for
   non-negative integers, enums, types identified by one integral member, and :cpp:type:`urd::index`; specialize it
   for others.

.. cpp:struct:: template<typename... Ts> key

   The key of a record in the context that finds it, one 32-bit number per tag, in order (``urd/elements/index.hpp``):
   the first selects a container of the context, the next looks up in it, and so on (a row of a relation:
   ``key<Relation, Row>``, its relation's number, then its row's there). One flat base per tag holding its number (no
   ``std::tuple``), each part read with ``get<T>()`` (a ``key<T>``); the context resolves a key (``context.view(key)``)
   and its views hold it whole as their index (``index_view<T, C, key<..., T>>``). Trivially copyable, compared and
   hashed part by part in order, stored flat by cista part by part. ``element_type`` is its last tag, the kind it
   names. A default-built key is none in every part (``is_none()``). Generic code asks :cpp:concept:`Key`. Bound in
   Python as ``<Name>Key`` after its last tag (``parts()`` gives its indices); an index as ``<Name>Index``.

   .. cpp:function:: template<typename T> key<T> get() const noexcept

.. cpp:concept:: template<typename K> Key

   ``K`` is a :cpp:struct:`key` (of one tag, an index, or of several).

.. cpp:type:: template<typename T> index = key<T>

   The index of a ``T`` in the Store that numbers ``T``'s: a key of one tag. A positional set mints one on first
   insert, and a pool's handles are indices. One 32-bit integer (``value``, read through its one base) with a ``none``
   value (``is_none()``), a different type per ``T`` that converts to no other, hashable, orderable, and a dense key.
   Write it ``urd::index``: the C library declares a function ``index`` in the global namespace. A key of several tags
   has no ``value`` (it is ambiguous there): its parts are read with ``get<T>()``.

.. cpp:struct:: layer

   The number of a Store that hands out indices for views (a pool of records, a positional set): such a Store is
   built from it first, ``Atoms(urd::layer { 3 })``, and answers it as ``layer()``. Who builds the Store gives it, and
   Stores whose views must differ get different numbers; a layer gets its own, as its parent does.

   .. cpp:member:: std::size_t number
