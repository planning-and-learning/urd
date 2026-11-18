Identity
========

.. cpp:namespace:: urd

What a container needs of its elements comes from three function objects, with nothing in namespace ``std``:

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
   view gives a tuple). Hash, EqualTo and Less of ``T`` are those of that identity.
