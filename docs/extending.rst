Extending urd
=============

.. cpp:namespace:: urd

urd's vocabulary is open. A container, a category, a type trait, a role, an operation, a facet, a type slot and a kind
are each a declaration of its own; none is a number in a shared list, and adding one edits no file of urd. Each is
identified by itself (the address of its words, or its type), so a library can add its own beside urd's.
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
       static constexpr urd::detail::property_set relies_on =                            // what a request must promise
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

``container_traits``, costs and synopses are derived from the declaration; a container is declared, or urd knows nothing
of it. The registry chooses the first container of the request's kind that provides what the request requires and
relies on nothing the request does not promise.

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

A fact is a tag about a role: a category, its answer, and whose it is. Without a role it is about the Store itself (what
a request states, what a container provides); with one, about part of its data: ``urd::role::element``, ``order``,
``mapped``, ``row``, ``rows``, ``row_element``, each a struct with a ``name`` and a ``description``
(``include/urd/registry/facts.hpp``). A type trait is a category answered by the type rather than stated
(``urd::trait::can_hash``): a struct with its words, whose category urd makes from it. A request's facts and a
container's reliance are one set of them (``property_set``); a container relies on
``relies(stated({ change::monotone }), traits<role::element, trait::can_hash>())``, and a request holding those facts
promises it.

An operation and a facet
------------------------

An operation is a struct with its words, the member of a container's ``operation_costs`` named for it (``in``), and
whether a wrapper's costs pass it on from its part's (``passed_on``): ``urd::operation::insert`` in
``include/urd/costs.hpp``, where urd's are written by a local macro (a member's name cannot be a template argument);
``tests/store/outside_kind.cpp`` writes its two by hand. A facet is a class template ``F<Derived, R, S>`` that gives the Store members, with
``facet_condition<F>`` (when a Store has it) and ``facet_operations<F>`` (the operations it offers), as in
``include/urd/store/*.hpp``.

A kind
------

The kinds are the data of one tree (``include/urd/tree.hpp``: the walk from a node to the first child that claims a
request, written once). A kind is a struct deriving from ``urd::detail::any_kind``, which says what every Store is,
and stating what differs:

.. code-block:: cpp

   struct tally_kind : urd::detail::any_kind
   {
       static constexpr std::string_view name = "tally";
       static constexpr bool claims(const urd::detail::request_facts& request) { return request.stated.has(tallying::tallied); }
       using takes = urd::detail::slot_list<counted_slot>;  // the type arguments it takes
       using format = urd::detail::request_format<tallying>;  // its questions, before every kind's
       using facets = urd::detail::facet_list<counting>;      // its Store's interface, after every kind's
       template<typename Args> using data = tally_data<Args>; // its types, from its type arguments
   };
   template<> struct urd::extra_kinds<void> { using type = urd::detail::type_list<tally_kind>; };

``any_kind`` gives the questions and facets every kind has (joined with a kind's own by ``format_of`` and
``facets_of``), and what a kind is unless it says otherwise: ``meaning`` (what a request means beyond what it states:
frozen implies stable references), ``asks`` and ``promises`` (what it asks of its container and promises it),
``prefers`` (which containers come after the others), ``well_formed`` (when a request contradicts itself),
``accepts_order_by``, ``changes_in_place`` (whether rows of it change in place) and ``builds_from`` (what it is built
from besides elements). A kind has no flags: what holds of its Store is a tag its meaning adds (a sequence's elements
are ``positional``) or a facet it lists (iteration). Kinds added by ``extra_kinds`` are tried before urd's own;
specialize it before the first Store is named. A node with ``children`` claims a request where one of them does, and
a request's kind is the leaf it reaches.

A type slot
-----------

A Store's type arguments fill slots: ``key<K>`` the key slot, ``value<V>`` the value slot. A slot is a struct with
its words; a type argument is a template naming its slot and its type:

.. code-block:: cpp

   struct counted_slot { static constexpr std::string_view name = "counted", description = "what a tally counts"; };
   template<typename T> struct counted { using type = T; using slot = counted_slot; };

A kind takes slots (``takes``), claims by the slots a request fills (``request.slots.has<key_slot>()``) and builds its
data (``data``: its element types, what it holds and finds, what orders it) from the type arguments; a type argument
for a slot its kind does not take does not compile. ``order_by<P>`` fills the order slot, which the ordered kinds take.
``urd::Store<counted<std::string>, tallied>`` is a tally of strings.

urd-search reads the same declarations: a kind, container, trait or operation added this way appears in its
registry tree and evidence once it is built with them.
