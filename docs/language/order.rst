Order
=====

Whether iteration follows the keys, the order of insertion, or nothing, and whether elements are reached by
position. Leave order open where nothing reads it: an unordered Store is the cheapest.

.. urd-theme:: order

.. urd-category:: any_order

   Iteration ascending by :cpp:class:`urd::Less`, or in the order elements first came. ``sorted`` adds
   ``lower_bound``, ``upper_bound``, ``equal_range`` and ``operator<=>``; either adds ``rbegin``, ``rend``. To order
   by something else, see ``order_by<P>`` in :doc:`index`.

.. urd-category:: by_key

   Elements identified by position: sequences, and sets with positions (an interner with ``monotone``). Such
   containers also declare it, which selection reads.
