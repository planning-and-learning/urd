What the data is like
=====================

Promises you make about the elements, and the few the container keeps about them. A demand such as ``dense``
lets urd pick a container that relies on it; state it only where it holds.

.. urd-theme:: data

.. urd-category:: any_keys

   Keys are indices 0..n-1 (:cpp:struct:`urd::index_of`), stored by position; or few in their range.

.. urd-category:: duplicates

   No two elements equal. Implied for sets, and for maps without ``multi``; stated for pools that intern.

.. urd-category:: maybe_empty

   At least one element: no default constructor, and construction from elements asserts one; a fixed-shape Store
   asserts a row. A promise about iterated elements: sets, maps and sequences only.

.. urd-category:: any_priority

   A priority queue never pushed below the last popped: integral priorities go to buckets.

.. urd-category:: general

   A few elements: picks compact containers, never changes a guarantee. Must be backed by a benchmark.
