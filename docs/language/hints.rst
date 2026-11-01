Hints
=====

*Leaving out a tag you need makes the program slower by a constant factor; a hint never changes a guarantee.*

How the Store is used. A hint only reorders the containers tried, so the Store has the same interface and the same
guarantees with it or without it; each must be backed by a benchmark (:doc:`/BENCHMARKS`).

.. urd-group:: hints

.. urd-category:: general

   A few elements: picks compact containers, never changes a guarantee. Must be backed by a benchmark.
