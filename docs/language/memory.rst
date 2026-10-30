Memory and layout
=================

Where the elements live and what becomes of storage. Ask for these where code holds references across inserts,
hands the elements to code that wants an array, or keeps memory across rounds.

.. urd-theme:: memory

.. urd-category:: moving_refs

   A reference to a stored element stays valid until that element is removed, by whichever path hands it out: an
   iterator's element, a map's value (``at``, ``operator[]``, ``update``, an entry's ``.second``), a sequence's
   ``operator[]``, a queue's ``front`` or ``top``, a pool's ``[handle]``. Insert moves no other element; ``erase`` and
   ``clear`` end only the references to what they remove.

.. urd-category:: any_layout

   Elements lie next to each other in memory: ``data()``.

.. urd-category:: any_packing

   Each record of an interning pool is one self-contained block of bytes (trivially copyable, or serialized by
   cista), frozen once stored and read by :cpp:struct:`urd::index`.

.. urd-category:: any_sharing

   Equal parts of different rows are stored once (tree pools).

.. urd-category:: any_memory

   Storage freed by erase and clear is kept for reuse, or returned.

.. urd-category:: any_representation

   Equal contents have equal representations (a bitset); such a Store is hashable by its contents.
