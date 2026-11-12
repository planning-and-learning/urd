Costs
=====

*Leaving out a tag you need makes the program slower by a complexity class: what these guarantee is how much work or
memory an operation takes.*

They are guarantees like any other, kept by the container and checked by measurement (:doc:`/BENCHMARKS`). What every operation of every
representation costs is in the :doc:`complexity tables </kinds/sets>` of each kind.

.. urd-group:: costs

.. urd-category:: any_clear

   ``clear`` costs O(elements present), or O(1).

.. urd-category:: any_memory

   Storage freed by erase and clear is kept for reuse, or returned.

.. urd-category:: any_sharing

   Equal parts of different rows are stored once (tree pools).

.. urd-category:: any_adjacency

   Whether two vertices of a graph are adjacent in O(log degree) or O(degree), or O(1), with a vertex's neighbours in a
   part as a row of bits.

.. urd-category:: any_endpoints

   An edge's source from its number in O(log n), or O(1) at a source per edge.
