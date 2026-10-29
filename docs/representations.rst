Representations
===============

.. cpp:namespace:: urd::repr

The containers a Store can resolve to live in ``include/urd/repr``, which code that uses a Store never includes:
requests use only the vocabulary of ``urd/traits.hpp`` and ``urd/properties.hpp``. Each container declares the
properties it keeps in ``container_traits``, and the typed tests check every declared property against a reference
model through thousands of random operations; a deliberately wrong declaration is caught. Adding a container is
described in :doc:`INTEGRATION`.

The standard library's and Boost's
----------------------------------

``std::vector``, ``std::set``, ``std::multiset``, ``std::map``, ``std::multimap``, ``std::unordered_set``,
``std::unordered_map``, ``std::unordered_multimap``, ``boost::unordered_flat_set``, ``boost::unordered_flat_map``,
``boost::container::flat_set``, ``flat_multiset``, ``flat_map``, ``flat_multimap``, and with gtl
``gtl::parallel_flat_hash_set``, ``parallel_flat_hash_map`` and their node-based twins, each with urd's :cpp:struct:`urd::Hash`,
:cpp:struct:`urd::EqualTo` and :cpp:struct:`urd::Less`.

urd's own
---------

.. list-table::
   :header-rows: 1
   :widths: 22 48 30

   * - Container
     - What it is
     - Serves
   * - ``DenseSet``
     - a bitset over the key range; with word tracking, a clear that touches only the words set
     - ``dense`` sets
   * - ``SparseSet``
     - Briggs–Torczon: n keys plus U positions, O(1) clear; optionally in insertion order
     - ``dense, constant_clear``; ``dense, insertion_ordered``
   * - ``IndexMap``
     - a slot per index; with generations, an O(1) clear; over segments, values that never move; with a spin lock
       per slot too, thread-safe updates
     - ``dense`` maps
   * - ``IndexedSet``, ``ConcurrentIndexedSet``
     - a vector plus a flat hash table of positions; the concurrent one over segments that never move
     - ``insertion_ordered``, ``positional`` (interners)
   * - ``OrderedMap``
     - a key set in insertion order beside its values
     - ``insertion_ordered`` maps
   * - ``PostingMap``
     - a flat hash table of value vectors
     - ``multi`` maps
   * - ``CsrMultiMap``, ``BidirectionalCsr``
     - compressed sparse rows; with the in-edges too
     - ``multi, dense, frozen`` maps, graphs
   * - ``Jagged``, ``RowPool``
     - rows in one array per level; rows kept with their storage for reuse
     - rows (:ref:`rows`)
   * - ``Recycling``, ``RecyclingSet``
     - a map that keeps erased nodes and their values for the next keys; a node set that keeps erased nodes
     - maps of ``retains`` rows; sets with ``stable_refs, retains``
   * - ``FlatEntryMap``
     - entries in the order their values give, found by key
     - ``sorted, order_by<P(key, value)>``
   * - ``Queue``, ``AddressableHeap``, ``BucketQueue``
     - deque, vector or binary heap; a heap of handles; a bucket per priority
     - queues
   * - ``RowInterner``, ``TreeVectorSet``, ``TreeBitsetSet``, ``SlotVector``
     - rows stored once in full; as trees sharing subtrees; slots reused after erase
     - pools
   * - ``RecordInterner``, ``ArenaInterner``, ``ByteArena``
     - records stored once, in an indexed set or serialized into a byte arena
     - ``flat`` pools
   * - ``LayeredPool``, ``Layered``
     - a pool, set or map over a parent
     - ``layered``
   * - ``Scoped`` (over a flat hash, tree or dense map), ``ChangeLogged`` (over a flat hash, dense, tree or node set), ``DisjointSets``, ``SegmentedVector``
     - a map with a log of previous values; a set logging its changes; union-find; a vector of doubling segments
     - ``scoped``, ``change_log``, ``partition``, ``monotone, stable_refs``

Order of preference
-------------------

The registry lists the candidates in an order backed by :doc:`BENCHMARKS`: an open-addressing hash table first
wherever order is not asked for, node-based tables for ``stable_refs``, a tree where order is asked for over
elements inserted over time, the dense containers after the hash tables (and only a request that promises
``dense`` reaches them at all: a container that stores by index demands keys that are indices, whatever its place
in the list), the indexed set after them, pools and queues by their kind. Where a request is ``small`` or ``frozen``,
containers with stable storage (a node per element, or segments) move last. :doc:`RESOLUTION` shows what every
request of a grid resolves to; a test audits
that the chosen container gives no more costly guarantees nobody asked for than another serving one, listing the
intended exceptions.
