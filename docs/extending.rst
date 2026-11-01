Extending urd
=============

.. cpp:namespace:: urd

urd's vocabulary is open. A container, a category, a type trait, a role, an operation, a facet and a kind are each a
declaration of its own; none is a number in a shared list, and adding one edits no file of urd. Each is identified by
itself (the address of its words), so a library can add its own beside urd's.
``tests/store/outside_kind.cpp`` adds a kind with all of them and resolves a Store to it; it is the example below.

A container
-----------

A container is a declaration (``include/urd/repr/<kind>/*_fwd.hpp`` in urd) and two links to its type:

.. code-block:: cpp

   struct my_set_decl
   {
       static constexpr std::string_view id = "my_set";
       static constexpr auto headers = urd::detail::header_list<1> { "my/my_set.hpp" };  // where it is defined
       using kind = urd::set_kind;                                                       // the kind it serves
       static constexpr urd::detail::property_set provides = urd::detail::property_set::of({ urd::order::sorted, urd::uniqueness::unique });
       static constexpr urd::detail::role_tags relies_on =                               // what a request must promise
           urd::detail::relies(urd::detail::traits<urd::role::order, urd::trait::natural_ordered>());
       static constexpr std::array<urd::tag, 0> gives_elements {};                       // what Stores held in it get
       struct operation_costs                                                            // by operation, see costs.hpp
       {
           static constexpr urd::cost insert = urd::costs::log_n;
           static constexpr urd::cost lookup = urd::costs::log_n;
           static constexpr double words = 3;
       };
       static constexpr std::string_view description = "my set", holds = "a node per element";
   };

- ``representation_of<my_set_decl, S>::type`` names its type for a request's spec ``S``, in the container's own
  header, which also defines the marker ``urd::include<"my/my_set.hpp">``.
- ``declaration_of<MySet<T>>`` names the declaration of a type, with the roles its template arguments give
  (``element_roles<T>()``).
- ``declarations_of<Kind>`` lists a kind's containers in order of preference; a container added to an existing kind
  is placed there.

``container_traits``, costs and synopses are derived from the declaration. The registry chooses the first container
of the request's kind that provides what the request requires and relies on nothing the request does not promise.

A category
----------

A category is an enum, ``none`` first, with a tag type per answer and the list of them:

.. code-block:: cpp

   enum class tallying : std::uint8_t { none, tallied };
   struct any_tallying { using root = any_tallying; static constexpr auto value = tallying::none; static constexpr std::string_view name = "any_tallying"; };
   struct tallied : any_tallying { static constexpr auto value = tallying::tallied; static constexpr std::string_view name = "tallied"; };
   template<> struct urd::tags_of<tallying> { using type = urd::detail::type_list<any_tallying, tallied>; };

A tag type derived from another promises at least what it does. ``exact``, ``demanded`` and ``request_only`` on the
none type or a tag change how the category is matched (``include/urd/property_set.hpp``).

A type trait, a role
--------------------

A type trait is what a type *is*, derived, never stated: a struct with a ``name`` and a ``description``
(``urd::trait::can_hash``, ``include/urd/registry/facts.hpp``). A role is what of a Store's data a request promises
about and a container relies on (``urd::role::element``, ``order``, ``mapped``, ``row``, ``rows``, ``row_element``),
a struct of the same shape. A container relies on traits and tags by role; a kind lists its roles.

An operation and a facet
------------------------

An operation is a struct with its words, the member of a container's ``operation_costs`` named for it (``in``), and
whether a wrapper's costs pass it on from its part's (``passed_on``): ``urd::operation::insert`` in
``include/urd/costs.hpp``. A facet is a class template ``F<Derived, R, S>`` that gives the Store members, with
``facet_condition<F>`` (when a Store has it) and ``facet_operations<F>`` (the operations it offers), as in
``include/urd/store/*.hpp``.

A kind
------

A kind is a node of the tree in ``include/urd/kinds.hpp``:

.. code-block:: cpp

   struct tally_kind
   {
       static constexpr std::string_view name = "tally";
       using format = urd::detail::request_format<tallying, urd::threading>;  // the questions its requests answer
       using roles = urd::detail::role_list<>;                                // what its containers rely on
       template<typename K, typename V> using element_type = V;
       template<typename K, typename V> using mapped_type = void;
       static constexpr bool iterable = false;
       using facets = urd::detail::facet_list<counting>;                     // its Store's interface
       static constexpr bool keeps_by_value = false;
       static constexpr bool positional = false;
       static constexpr bool claims(const urd::detail::property_set& stated, bool keyed, bool valued) { return stated.has(tallying::tallied); }
       static constexpr urd::detail::property_set meaning(urd::detail::property_set stated) { return stated; }
   };
   template<> struct urd::extra_kinds<void> { using type = urd::detail::type_list<tally_kind>; };

Kinds added by ``extra_kinds`` are tried before urd's own; specialize it before the first Store is named. A kind with
``children`` (a list of kinds) instead of a format and facets splits further, as the queue does into arrival and
priority: a request's kind is the leaf it reaches.

urd-search reads the same declarations: a kind, container, trait or operation added this way appears in its
registry tree and evidence once it is built with them.
