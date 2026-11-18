Order
=====

.. cpp:namespace:: urd

.. cpp:struct:: template<typename T> Less

   ``bool operator()(const T&, const T&) const``: a strict weak order agreeing with :cpp:struct:`EqualTo`,
   lexicographic over members and ranges. Given for Stores iterated in an order their request fixes (``sorted``,
   ``insertion_ordered``, sequences), so such a Store can be a sorted key.

.. cpp:concept:: template<typename T> orderable

   :cpp:struct:`Less` and :cpp:struct:`EqualTo` apply to ``T``: what a ``sorted`` key or a ``by_priority``
   element needs.
