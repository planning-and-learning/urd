.. _moving-and-copying:

Moving and copying elements
===========================

.. cpp:namespace:: urd

A Store that holds objects (a sequence, a set, a map, a queue, slots, a graph's data) takes each by move, or by copy
where its type is copyable (``std::copy_constructible``):

- Every member that puts an element in has a ``T&&`` form that moves it in: ``push_back``, ``insert``,
  ``insert(pos, …)``, ``insert(hint, …)``, ``push``, ``update(handle, …)``; ``emplace`` and its kin forward. Where
  insertion looks the element up first (a set, a map's key), it moves only on a miss: a hit leaves the argument as it
  was.
- The ``const T&`` forms and every member that copies (a range of iterators, an ``initializer_list``,
  ``assign(n, value)``, ``resize(n, value)``, the constructors from elements) exist only for a copyable type. For a
  move-only one they are deleted, and the error names the reason and what to call instead:

  .. code-block:: text

     error: use of deleted function '... push_back(const value_type&) requires denied<"push_back(const T&): the
     element is move-only: move it in, push_back(std::move(x))", T>'

- A map's key follows the same rule: ``operator[]``, ``try_emplace`` and ``insert_or_assign`` take ``K&&`` and, for a
  copyable key, ``const K&``. A map's element holds its key ``const``, so a move-only key goes in by ``try_emplace``,
  not by ``insert(std::pair&&)``. Not every map holds one: gtl's flat hash maps (the unsorted default) and scoped maps
  copy keys, and an insertion-ordered map hands out pairs holding a copy.

A pool of records interns, and offers both:

- ``insert(const T&)`` (and a ``make`` that refers to a record) reads the record: serialized (``flat``, the arena),
  any record, a move-only one too; held by value, copied, a copyable one. The caller's record stays as it was, so that
  a scratch record is reused, buffers and all.
- ``insert(T&&)`` (and a ``make`` that gives a ``T`` or a ``T&&``) hands the record over: moved in where it is new
  (serialized from, in the arena), left as it was where an equal one is held. For temporaries.

A trivially copyable record is held by value (``flat`` too): a move-only one there is handed over (``insert(T&&)``);
a record that owns memory is serialized (``flat`` with ``cista_members``).
