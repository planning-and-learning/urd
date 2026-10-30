Queues
======

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename T, typename... Properties>
   using Queue = Store<value<T>, fifo | lifo | by_priority, Properties...>;

.. cpp:class:: Queue

   A queue: ``Store<value<T>, fifo | lifo | by_priority, Properties...>``. The name ``Queue`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Queue

A queue is reached by its front or top, and visited by iteration (not a bucket queue). An access tag makes a value Store a queue.

.. list-table::
   :header-rows: 1
   :widths: 44 56

   * - Request
     - Representation
   * - ``Store<value<T>, fifo>``
     - ``Fifo`` (a deque); ``stable_refs`` too: the front stays put while elements queue behind it
   * - ``Store<value<T>, fifo, retains>``
     - ``RingFifo``: one vector used as a ring, kept on pop and clear
   * - ``Store<value<T>, lifo>``
     - ``Lifo`` (a vector)
   * - ``Store<value<T>, by_priority>``
     - ``Heap`` (a binary heap; the least element leaves first)
   * - ``Store<value<T>, by_priority, order_by<&T::cost>>``
     - ``Heap`` by the cost, ties by the element's own order
   * - ``Store<value<T>, by_priority, handles>``
     - ``AddressableHeap``: ``push`` gives a handle; ``update(h, e)``, ``erase(h)``
   * - ``Store<value<I>, by_priority, monotone_priority>``
     - ``BucketQueue`` (a bucket per integral priority)

Template parameters
-------------------

``T``
   Any type; :cpp:concept:`orderable` with ``by_priority`` (or what ``order_by`` projects it to).

Member types
------------

``value_type`` is ``T``; ``handle_type`` is the representation's handle, with ``handles``.

Member functions
----------------

Construction (``Store()``, from elements) and capacity (``size``, ``empty``, ``reserve``) are as for :doc:`sets`. A
queue is not compared: a heap's order is no part of its contents.

.. cpp:function:: const_iterator begin() const
.. cpp:function:: const_iterator end() const

   The elements in the order pushed (``fifo``, ``lifo``), or in no particular order (``by_priority``), O(n). Not with
   ``monotone_priority``: a bucket queue would visit the bucket of every priority in its range.

.. cpp:function:: void push(const value_type& element)
.. cpp:function:: void push(value_type&& element)
.. cpp:function:: template<typename... Args> void emplace(Args&&... args)

   With ``handles``, ``push`` and ``emplace`` give the handle that reaches the element later.

.. cpp:function:: void pop()

   Removes the next element; the Store must not be empty. Offered under ``monotone`` too: pop is how a queue is
   consumed, and ``monotone`` takes only ``clear`` away. ``nonempty`` is not a queue's to promise, since pop
   empties it: a request for one is rejected.

.. cpp:function:: const value_type& front() const
.. cpp:function:: const value_type& back() const

   ``fifo``: the element that came first, and the one that came last.

.. cpp:function:: const value_type& top() const

   ``lifo``: the element that came last; ``by_priority``: the least by the order.

.. cpp:function:: void clear()

   Shrinkable queues; a ``monotone`` queue empties only by ``pop``.

.. cpp:function:: void update(const handle_type& handle, const value_type& element)
.. cpp:function:: void erase(const handle_type& handle)

   With ``handles``: replaces the element of ``handle``, which moves to its place in the order; removes it,
   wherever it is.

.. cpp:function:: const value_type& operator[](const handle_type& handle) const

   With ``handles``: the element of ``handle``, wherever it is in the order, O(1).

Complexity
----------

.. list-table::
   :header-rows: 1
   :widths: 26 16 16 20 12 10

   * - Representation
     - push
     - access
     - pop / erase
     - clear
     - memory
   * - ``Fifo`` / ``Lifo``
     - O(1)\*
     - O(1)
     - O(1)
     - O(n)
     - deque (stable_refs) / vector (retains)
   * - ``RingFifo``
     - O(1)\*
     - O(1)
     - O(1)
     - O(n)
     - one vector as a ring; retains
   * - ``Heap``
     - O(log n)
     - ``top`` O(1)
     - O(log n)
     - O(n)
     - vector; retains
   * - ``AddressableHeap``
     - O(log n)
     - ``top`` O(1)
     - ``pop``, ``erase(h)``, ``update(h, e)`` O(log n)
     - O(n)
     - heap of handles + a slot per handle
   * - ``BucketQueue``
     - O(1)\*
     - ``top`` O(1)
     - O(1)\* under monotone pushes
     - O(range)
     - a vector per priority

Example
-------

.. code-block:: cpp

   struct Node { double f; State state; static constexpr auto identifying_members() noexcept { return Members<&Node::f, &Node::state> {}; } };
   using Open = Store<value<Node>, by_priority, order_by<&Node::f>>;
   auto open = Open();
   open.push({ 3.5, start });
   while (!open.empty()) { const auto node = open.top(); open.pop(); /* expand */ }
