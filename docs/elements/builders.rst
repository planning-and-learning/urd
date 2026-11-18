Builders
========

.. cpp:namespace:: urd

A record is stored once and never changed; a ``T`` that is still being made (a state while it is expanded, a
denotation while it is computed, a row before it is looked up) is a *builder* (``urd/elements/builder.hpp``).

.. cpp:struct:: template<typename T, typename C> builder

   Specialized per type. It has the members of its record ``data<T>``, under the same names and in the same order
   (``fields()``): the same record in another memory organization. A member that is a value in the record is a value
   in the builder; a ``data<X>`` held in place is a ``builder<X, C>``; a flat container is std's of the members it
   holds; an index of a record is a *child*. ``C`` is the context the builder's children are read through.

   A builder only mutates. It reads nothing itself, not even its own children: an interned child is an index, which
   only a context can resolve. It is read through a view, as a record is (``borrowed_view<T, C, builder<T, C>>``,
   :cpp:func:`resolve` ``(builder, context)``), with its record's mixin.

.. cpp:type:: template<typename T, typename C> child = std::variant<index<T>, handle<builder<T, C>>, handle<data<T>>>

   A child of a builder, in one of three forms:

   - interned: its ``index<T>`` (the none index: a child default-constructs empty);
   - built: a builder of its own, held by its :cpp:type:`handle`;
   - a record not interned yet, held by its :cpp:type:`handle`.

   The two handles hold pooled temporaries the child owns: they go back to their pools when the child drops them, and
   become interned only when their parent is (:cpp:func:`intern`). It is read as any variant member is
   (:cpp:struct:`resolution` of a variant: each alternative resolved): the view the context makes of the index
   (``context.view(index)``), the builder's borrowed view, or the record's view (``context.view(record)`` where the
   context makes one, else its ``borrowed_view``), as a builder or a record held in place is read. ``EqualTo`` and
   ``Hash`` are the variant's: a handle compares by the record it holds.

.. cpp:type:: template<typename R> handle = std::unique_ptr<R, release>

   A record held by its handle, from a Store of slots (``Store<slots<R>>::make_handle()``): given back to its Store,
   its buffers kept, when the handle goes. :cpp:struct:`release` names no Store, so that a record may hold handles of
   its own kind and a library may keep its pools out of its headers.

.. cpp:function:: template<typename T, typename C, typename Pool> builder<T, C>& builder_of(child<T, C>& held, Pool& pool)

   The builder a child holds, to fill: a new one from ``pool`` where it holds none (it is interned, or a record).
