urd
===

**Say what your data is. urd picks the container and checks that it keeps its promises.**

A ``Store`` is described by what it holds and how it is used, not by which container holds it:

.. code-block:: cpp

   #include <urd/store.hpp>
   #include <urd/use/all.hpp>   // or only the families these Stores need: dense, hash, queues
   using namespace urd;

   struct User                                                             // an index: one integral member
   {
       std::uint32_t id;
       static constexpr auto identifying_members() noexcept { return Members<&User::id> {}; }
   };

   using Names    = Store<key<std::string>, positional, monotone>;         // an interner: each name gets an index
   using Follows  = Store<key<User>, value<User>, multi, dense, frozen, bidirectional>;  // a frozen graph
   using Seen     = Store<key<User>, dense>;                               // a bitset
   using Frontier = Store<value<User>, fifo>;                              // a queue
   using Distance = Store<key<User>, value<int>, dense, constant_clear>;   // cleared per search in O(1)

urd resolves each request at compile time to a container from the standard library, Boost, or its own
``urd::repr``, and gives the Store exactly the interface those properties allow: a ``frozen`` Store has no
``insert``, an unordered one no ``lower_bound``, a queue no iteration. Every container urd can choose is checked
against a reference model through thousands of random operations.

This documentation describes the Stores the way the standard library describes its containers: for each kind, its
member types, its member functions grouped by what they do and by the properties that switch them on, and the
complexity of each operation per representation.

.. toctree::
   :maxdepth: 2
   :caption: The request language

   requests
   properties
   semantics

.. toctree::
   :maxdepth: 2
   :caption: Kinds of Store

   kinds/sets
   kinds/maps
   kinds/sequences
   kinds/queues
   kinds/pools
   kinds/records
   kinds/partitions

.. toctree::
   :maxdepth: 1
   :caption: Representations

   representations
   RESOLUTION

.. toctree::
   :maxdepth: 1
   :caption: Development

   INTEGRATION
   BENCHMARKS
   SURVEY
   ROADMAP

Header-only C++20, GPL-3.0. Build: see :ref:`building`.
