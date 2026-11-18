Children of a record
====================

.. cpp:namespace:: urd

``urd/elements/children.hpp`` walks the handles a record holds: its children, every ``index<U>`` with ``U`` in a
type list ``Types``, found in its cista members directly, in (nested) flat vectors, as a variant's alternative, in an
optional or a pair, or in a nested record. ``U`` may be the record's own type ``T``: only a direct member of type
``index<T>``, the record's own handle, is no child. An index of a type not in ``Types`` is neither handed out nor
walked into.

.. cpp:function:: template<typename Types, typename T, typename R, typename C, typename F> void for_each_child(const R& record, const C& context, F f)

   Calls ``f(context.view(child))`` for each child, in member order.

.. cpp:function:: template<typename Types, typename T, typename R, typename C, typename F> R map_children(R record, const C& context, F f)

   The record with each child ``c`` replaced by ``to_stored(f(context.view(c)))``: ``f`` returns an index of ``c``'s
   type or a view of one. A variant keeps its alternative.

.. cpp:function:: template<typename Types, typename T, typename R, typename C, typename F> R map_variant_children(R record, const C& context, F f)

   ``map_children``, but each ``flat_variant`` slot not inside another variant (whatever its alternatives) is
   replaced whole, by ``to_stored(f(resolve(slot, context)))``: ``f`` gets the ``std::variant`` of the slot's
   alternatives resolved and may return another alternative (a ``not`` becoming an ``or``), as a view, an index or
   the variant. The walk does not enter a slot: the children inside it are ``f``'s to map. Children outside variants
   are replaced as ``map_children`` replaces them, so ``f`` takes both views and resolved variants (an overload set,
   or a generic callback).

A builder's children are its :cpp:type:`child` members, found in the same shapes as std's containers and builders
held in place hold them.

.. cpp:function:: template<typename T, typename C, typename F> data<T>& map_children(const builder<T, C>& source, data<T>& scratch, F f)

   The record of a builder, written into ``scratch`` (a reused record: its buffers kept, its own handle as it is):
   each field in step with the builder's, each child replaced by the index ``f`` gives it (``f`` returns an index, an
   ``index_view``, or the ``(view or index, created)`` pair a repository's insert returns), a builder held in place
   as its record, std's containers as the flat ones, a value as it is.

.. cpp:function:: template<typename T, typename C, typename Insert> decltype(auto) intern(const builder<T, C>& built, data<T>& scratch, Insert&& insert)

   Interns a builder: its record written into ``scratch`` (``map_children``), an interned child as its index, a
   built child's builder and a record not interned yet inserted through ``insert`` (so built children first), then
   ``insert(scratch)``, whose result it returns. ``insert`` is the caller's: it takes a record or a builder of every
   kind the builder holds, inserts it into the repository that holds its kind (a library canonicalizes there; a
   builder is interned again, with a scratch record of its own), and returns the inserted record's index, its
   ``index_view``, or the ``(view or index, created)`` pair (:cpp:func:`inserted_index`).
