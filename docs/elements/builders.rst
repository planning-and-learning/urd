Builders
========

.. cpp:namespace:: urd

A record is stored once and never changed; a ``T`` that is still being made (a state while it is expanded, a
denotation while it is computed, a row before it is looked up) is a *builder* (``urd/elements/builder.hpp``).

.. cpp:struct:: template<typename T, typename C> builder

   Specialized per type. It has the members of its record ``data<T>``, under the same names and in the same order
   (``fields()``): the same record in another memory organization. A member that is a value in the record is a value
   in the builder; a ``data<X>`` held in place is a ``builder<X, C>``; a flat container is std's of the members it
   holds; an index of a record is a *child*, and a flat vector of indices a ``std::vector`` of children, each in its own
   form. ``C`` is the context the builder's children are read through.

   A builder only mutates. It is read through a view, as a record is (``borrowed_view<T, C, builder<T, C>>``,
   :cpp:func:`resolve` ``(builder, context)``), with its record's mixin.

.. cpp:type:: template<typename T, typename C, typename K = index<T>> interned_child = index_view<T, C, K>
.. cpp:type:: template<typename T, typename C> built_child = pooled_view<T, C, builder<T, C>>
.. cpp:type:: template<typename T, typename C> held_child = pooled_view<T, C, data<T>>

   A child's three forms, by name: code that takes one form apart (``std::get_if<built_child<T, C>>``) says which.

.. cpp:type:: template<typename T, typename C, typename K = index<T>> child = std::variant<interned_child<T, C, K>, built_child<T, C>, held_child<T, C>>

.. cpp:type:: template<typename L, typename C> child_variant

   The built form of an ``index_variant<L>``: the ``std::variant`` of the children of ``L``'s tags, in the same order.

   A child of a builder, a view through its own context in one of three forms:

   - interned: its :cpp:class:`index_view`, whose key ``K`` is ``index<T>`` or a :cpp:struct:`key` of several (a row's: its relation and
     its id); the none view (no key, no context): a child default-constructs empty;
   - built: a builder of its own, owned by its :cpp:class:`pooled_view`;
   - a record not interned yet, owned by its :cpp:class:`pooled_view`.

   Every form carries its context, so a child reads as it is, whoever reads it: it is resolved as any variant member is
   (:cpp:struct:`resolution` of a variant), the index_view as it is, a pooled view as its record's ``borrowed_view``
   through its own context. The pooled temporaries go back to their pools when the child drops them, and become interned
   only when their parent is (:cpp:func:`intern`). ``EqualTo`` and ``Hash`` are the variant's: each view's identity.

.. cpp:class:: template<typename T, typename C = typename context_of<T>::type, typename R = data<T>> pooled_view

   Owns a record ``R`` (``data<T>`` or ``builder<T, C>``) by its pooled :cpp:type:`handle`: a child's temporary.
   Move-only. Read and identified by the record, through its own context.

   .. cpp:function:: pooled_view(handle<R> record, const C* context)
   .. cpp:function:: R& held() const

      The record it owns, to write: a builder being built, a record not interned yet.

.. cpp:type:: template<typename R> handle = std::unique_ptr<R, release>

   A record held by its handle, from a Store of slots (``Store<slots<R>>::make_handle()``): given back to its Store,
   its buffers kept, when the handle goes. :cpp:struct:`release` names no Store, so that a record may hold handles of
   its own kind and a library may keep its pools out of its headers.

.. cpp:function:: template<typename T, typename C, typename K, typename Pool> builder<T, C>& builder_of(child<T, C, K>& held, Pool& pool, const C& context)

   The builder a child holds, to fill: a new one from ``pool``, read through ``context``, where it holds none (it is
   interned, or a record).
