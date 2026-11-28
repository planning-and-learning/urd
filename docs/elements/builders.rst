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

.. cpp:type:: template<typename T, typename C, typename K = key_t<T>> interned_child = K
.. cpp:type:: template<typename T, typename C> built_child = handle<builder<T, C>>
.. cpp:type:: template<typename T, typename C> held_child = handle<data<T>>

   A child's three forms, by name: code that takes one form apart (``std::get_if<built_child<T, C>>``) says which.

.. cpp:type:: template<typename T, typename C, typename K = key_t<T>> child = std::variant<interned_child<T, C, K>, built_child<T, C>, held_child<T, C>>

   A child record ``T`` of a builder, in one of three forms:

   - interned: its key ``K`` (``key<T>``, or a key of its own such as a row's: its relation and its id); the none key
     until one is given: a child default-constructs empty;
   - built: a builder of its own, owned by its :cpp:type:`handle`;
   - held: a record of its own not interned yet, owned by its :cpp:type:`handle`.

   No form carries a context: a child is read through the context of the view that reads its builder, resolved as any
   variant member (:cpp:struct:`resolution`): a key as the :cpp:class:`index_view` its context makes, a handle as its
   record's ``borrowed_view`` (``borrowed_view<T, C, builder<T, C>>``, ``borrowed_view<T, C, data<T>>``). The resolved
   child is a ``child_view``: read with ``T``'s getters whatever its form, compared and hashed as the view of its form.
   A built or held child becomes interned only when its parent is (:cpp:func:`intern`); its handle gives it back to its
   pool when the child drops it.

.. cpp:type:: template<typename R> handle = std::unique_ptr<R, release>

   A record held by its handle, from a Store of slots (``Store<slots<R>>::make_handle()``): given back to its Store,
   its buffers kept, when the handle goes. :cpp:struct:`release` names no Store, so that a record may hold handles of
   its own kind and a library may keep its pools out of its headers.

.. cpp:function:: template<typename T, typename C, typename K, HandsOutHandles Pool> builder<T, C>& builder_of(child<T, C, K>& held, Pool& pool)
.. cpp:function:: template<typename T, typename C, typename K, ChecksOutBuilders<T, C> Storage> builder<T, C>& builder_of(child<T, C, K>& held, Storage& storage)
.. cpp:function:: template<typename T, typename C, typename K, std::invocable Make> builder<T, C>& builder_of(child<T, C, K>& held, Make make)

   The builder a child holds, to fill: where it holds none (it is interned, or a record), a new one from ``pool`` (a
   pool of one kind: ``pool.make_handle()``), one ``storage`` checks out of the child's kind (a storage of pools of many
   kinds: ``storage.template checkout<T>()``), or the :cpp:type:`built_child` ``make()`` returns (``make_owned``).

.. cpp:concept:: template<typename Storage, typename T, typename C> ChecksOutBuilders

   Whether ``Storage`` checks out builders of ``T`` read through ``C``: ``storage.template checkout<T>()`` returns a
   :cpp:type:`built_child` ``<T, C>``.

.. cpp:function:: template<typename T, typename C, typename K> builder<T, C>& built(child<T, C, K>& held)

   The builder a child holds built, to write in place (also ``const``): ``std::bad_variant_access`` where it holds its
   key or a record (its context unpacks it first: :cpp:func:`unpacked`).

.. cpp:function:: template<typename V> auto built_view(const V& reading)
.. cpp:function:: template<typename V> auto interned_view(const V& reading)

   The view of a reading's form, where a reader needs it: its builder's view (``built_view``), its
   :cpp:class:`index_view` (``interned_view``). A view of that form is itself; of a child view (or the variant of the
   views of a child's forms), the form it holds, ``std::bad_variant_access`` where it holds another.

.. cpp:function:: template<typename T, typename... Vs, typename F> decltype(auto) visit_view(const child_view<T, Vs...>& view, F&& f)

   ``f`` of the view of the form the child holds.

.. cpp:concept:: template<typename C, typename T, typename K = key_t<T>> Unpacks
.. cpp:concept:: template<typename C, typename T> UnpacksRecords

   Whether a context ``C`` unpacks a key ``K`` of a ``T`` (``context.unpack(key)``), or a record of a ``T`` it does not
   hold (``context.unpack(record)``, a ``const data<T>&``): either returns a
   ``std::shared_ptr<const builder<T, C>>``, owned as the context decides (a builder from its pool, given back with the
   last copy), so that no control block is made per unpack where the context keeps one.

.. cpp:concept:: template<typename C, typename K, typename B> UnpacksBuilt

   Whether a context ``C`` reads the record a key ``K`` names into a builder ``B`` in place
   (``context.unpack_built(key, built)``), its buffers kept. ``build_from`` builds a builder held in place from its
   record's key so (a record whose stored form lies in a pool of its own, built as its builder:
   ``built_form<key<S>, C>::type`` is ``builder<S, C>``).

.. cpp:function:: template<typename V> shared_view<T, C, builder<T, C>> unpacked(const V& view)

   A reading of a ``T`` as a builder's view, shared:

   - a builder's shared view: itself;
   - a builder's borrowed (or owning) view: its builder, unowned (``shared_view::unowned``: valid while the builder is);
   - an :cpp:class:`index_view`: the builder its context unpacks its key into, where the context :cpp:concept:`Unpacks`;
   - a record's borrowed view: the builder its context unpacks the record into, where the context
     :cpp:concept:`UnpacksRecords`;
   - a child view: that of the form it holds, where each of its forms unpacks (a context that unpacks keys but not
     records unpacks its index views, not its child views, which may hold a record).

   What the context hands out is released with the last copy of the view.

.. cpp:function:: template<typename V> data<T> pack(const V& view)

   The record a reading of a ``T`` reads, as a ``data<T>`` of its own:

   - a builder's view: its builder packed (each child the key it has: an interned child's key, a built or held child's
     own index where that is its whole key; the builder's own index);
   - an :cpp:class:`index_view`: its stored record read out field by field (``stored_field``: a record held whole, or
     one its store keeps compressed), its own index the view's key where that is its whole key;
   - a record's borrowed view: a copy of its record (:cpp:func:`clone`);
   - a child view: that of the form it holds, where each of its forms packs.

.. cpp:concept:: template<typename V> Packable

   Whether a view ``V`` packs (``urd::pack(view)``).
