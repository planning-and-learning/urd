Indices
=======

.. cpp:namespace:: urd

.. cpp:concept:: template<typename K> indexable

   :cpp:struct:`index_of` applies to ``K``: what a ``dense`` key needs.

.. cpp:struct:: template<typename K> index_of

   How a key is an index 0..n-1: ``is_index(key)`` (a negative integer, or :cpp:struct:`urd::index`'s none, is no
   index, and a lookup of one finds nothing), ``to_index(key)`` and ``from_index(std::size_t)``. Given for
   non-negative integers, enums, types identified by one integral member, and :cpp:struct:`urd::index`; specialize it
   for others.

.. cpp:struct:: template<typename T> index

   The index of a ``T`` in the Store that numbers ``T``'s (``urd/elements/index.hpp``): a positional set mints one on first
   insert, and a pool's handles are indices. One 32-bit integer (``value``) with a ``none`` value
   (``is_none()``), a different type per ``T`` that converts to no other, hashable, orderable, and a dense key.
   Write it ``urd::index``: the C library declares a function ``index`` in the global namespace.

   .. cpp:member:: value_type value

      The number; ``none_value`` by default.

   .. cpp:member:: static constexpr value_type none_value

      The ``none`` value: the largest ``value_type``. A default-built ``index`` is none.

   .. cpp:function:: bool is_none() const noexcept

.. cpp:struct:: layer

   The number of a Store that hands out indices for views (a pool of records, a positional set): such a Store is
   built from it first, ``Atoms(urd::layer { 3 })``, and answers it as ``layer()``. Who builds the Store gives it, and
   Stores whose views must differ get different numbers; a layer gets its own, as its parent does.

   .. cpp:member:: std::size_t number
