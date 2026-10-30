Change over time
================

What the Store does after it is built: whether it changes at all, how it is cleared, and what it remembers of
its changes. Saying less change buys cheaper containers and stronger promises (a frozen Store is read by any
number of threads).

.. urd-theme:: change

.. urd-category:: may_change

   Only grows (no ``erase``, ``clear``, ``pop_back``), or changes only by ``assign(first, last)``, a rebuild in the
   storage held where the container can; a frozen Store is read by any number of threads between rebuilds. A
   monotone queue keeps ``pop``: pop is how a queue is consumed, and ``monotone`` takes only ``clear`` away.

.. urd-category:: immutable

   Whether the Store offers ``insert`` (growable) or also ``erase`` and ``clear`` (shrinkable). It follows from the
   answer to *Does it change after building?*: shrinkable by default, growable with ``monotone``, neither with
   ``frozen``.

.. urd-category:: any_clear

   ``clear`` costs O(elements present), or O(1).

.. urd-category:: any_tracking

   ``added()`` and ``removed()`` since ``clear_changes()``.

.. urd-category:: any_scope

   Changes undone by ``pop_scope``.

.. urd-category:: any_layering

   Lookups fall through to a parent; on pools, indices continue after the parent's.
