Elements: identity, hashing, order, indices
============================================

.. cpp:namespace:: urd

What a container needs of its elements comes from three function objects, with nothing in namespace ``std``:

.. cpp:struct:: template<typename T> Hash

   ``hash_t operator()(const T&) const``: a deterministic 64-bit hash (xxHash64 over bytes, Boost.ContainerHash
   mixing for composites). Given for arithmetic types, enums, strings and string views, the standard's composites
   (``pair``, ``tuple``, ``array``, ``vector``, ``set``, ``map``, ``optional``, ``variant``, ``span``), any
   :cpp:concept:`MemberIdentified` or :cpp:concept:`FunctionIdentified` type, :cpp:struct:`urd::index`,
   :cpp:class:`urd::view`, the flat containers of ``urd/flat.hpp``, and Stores whose contents fix their order
   (``sorted``, ``canonical``, sequences). There is deliberately no fallback to ``std::hash``.

.. cpp:struct:: template<typename T> EqualTo

   ``bool operator()(const T&, const T&) const``: equality agreeing with :cpp:struct:`Hash`; for floating point,
   NaN equals NaN. Transparent (``is_transparent``) where the hash is.

.. cpp:struct:: template<typename T> Less

   ``bool operator()(const T&, const T&) const``: a strict weak order agreeing with :cpp:struct:`EqualTo`,
   lexicographic over members and ranges.

A type names its identity once and is then hashable, comparable and ordered, in member order:

.. cpp:struct:: template<auto... Ms> Members

   The identifying members of a record, as member pointers.

.. code-block:: cpp

   struct Edge
   {
       Vertex from, to;
       static constexpr auto identifying_members() noexcept { return Members<&Edge::from, &Edge::to> {}; }
   };
   using Edges = Store<key<Edge>>;                    // hashed by (from, to)

.. cpp:concept:: template<typename T> MemberIdentified

   ``T::identifying_members()`` gives a :cpp:struct:`Members`.

.. cpp:concept:: template<typename T> FunctionIdentified

   A non-static ``identifying_members()`` gives the identity as a tuple (a :cpp:class:`urd::view` does).

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

Indices and views
-----------------

.. cpp:struct:: template<typename T> index

   The index of a ``T`` in the Store that numbers ``T``'s (``urd/index.hpp``): a positional set mints one on first
   insert, and a pool's handles are indices. One 32-bit integer (``value``) with a ``none`` value
   (``is_none()``), a different type per ``T`` that converts to no other, hashable, orderable, and a dense key.
   Write it ``urd::index``: the C library declares a function ``index`` in the global namespace.

   .. cpp:member:: value_type value

      The number; ``none_value`` by default.

   .. cpp:function:: static index none() noexcept
   .. cpp:function:: bool is_none() const noexcept

.. cpp:class:: template<typename T, typename C = typename context_of<T>::type> view

   A reference to an element held by index (``urd/view.hpp``): the index and a pointer to a context ``C`` that
   resolves it, where :cpp:concept:`resolves` means ``context[index<T>]`` gives the record. A positional set and a
   pool resolve their own type; a :cpp:class:`compose::Family` every member type; any type with such an
   ``operator[]`` serves. The context must outlive the view.

   .. cpp:function:: view(index<T> at, const C& context) noexcept
   .. cpp:function:: index<T> id() const noexcept
   .. cpp:function:: const C& context() const noexcept
   .. cpp:function:: decltype(auto) record() const

      The record, as the context holds it.

   .. cpp:function:: template<auto Member> decltype(auto) get() const

      A member of the record resolved through the context (:cpp:func:`resolve`): an ``index<U>`` becomes a
      ``view<U, C>`` where the context resolves ``U``, pointing at the layer that owns the element, else stays an
      index; a range of indices a range of views; characters a ``std::string_view``; another contiguous range a
      ``std::span``; a variant the variant of its alternatives resolved.

   .. cpp:function:: auto identifying_members() const noexcept

      The identity: the index, and the context's ``layer()`` number where it has one. Two views of one element from
      one layer are equal and hash equally, wherever they were made; views from different layers are not.

.. cpp:concept:: template<typename C, typename T> resolves

   ``context[index<T>]`` is convertible to ``const T&``.

.. cpp:function:: template<typename M, typename C> decltype(auto) resolve(const M& member, const C& context)

   A record's member as a view sees it; see :cpp:func:`view::get`.

.. cpp:struct:: template<typename T> context_of

   Names a record type's default context once (``template<> struct urd::context_of<Atom> { using type =
   Repository; };``), so that ``view<Atom>`` needs no second argument.

.. cpp:struct:: template<typename T, typename V> view_mixin

   Where a library adds the named accessors of a record's views, one line each over ``get<>``: ``template<typename
   V> struct urd::view_mixin<Atom, V> { auto predicate() const { return static_cast<const V&>(*this).template
   get<&Atom::predicate>(); } };``.
