Threads
=======

Whether several threads change the Store at once. A Store that only reads after building needs nothing here:
``frozen`` is read by any number of threads.

.. urd-theme:: threads

.. urd-category:: single_threaded

   Threads share the Store: inserts from threads at once, and beside them reads of published elements (inserted
   before the reading thread started, or by it); for maps also ``update`` and ``read`` on any keys, each value under
   its own lock, ``at`` only for keys no thread writes; for pools inserts and reads by handle. A lookup beside
   inserts is ``contains`` or ``count`` (``find``'s iterator compared with ``end()`` races with the size). Erase of
   another thread's element, ``clear``, iteration, ``reserve``, copy, move and destruction need no other operation
   running.
