Python bindings
===============

A Store crosses into Python, through `nanobind <https://nanobind.readthedocs.io>`_, as the standard container of its
kind does, with nanobind's own casters:

==============================  ============
Store                           Python
==============================  ============
a sequence                      ``list``
a set                           ``set``
a map with one value per key    ``dict``
==============================  ============

Which Stores convert is decided by their representation. The header in ``urd/python/`` named as the
representation's in ``urd/repr/`` switches it on: a binding includes the caster of each representation its Stores
resolve to, as it includes the representation itself.

.. code-block:: cpp

   #include <urd/python/sequences/sequence.hpp>      // Store<sequence<T>>: std::vector
   #include <urd/python/maps/unordered_flat_map.hpp>  // Store<map<K, V>>: gtl::flat_hash_map

   NB_MODULE(example, m)
   {
       m.def("names", [] { return urd::Store<urd::sequence<std::string>> { "on", "clear" }; });
       m.def("arity", [](urd::Store<urd::map<std::string, int>> arity) { return arity.size(); });
   }

Casters exist for the sequences (``sequence``, ``segmented``), the sets (``dense_set``, ``flat_set``, ``indexed``,
``parallel_hash_set``, ``sparse_set``, ``std_set``, ``unordered_flat_set``) and the maps with one value per key
(``flat_map``, ``index_map``, ``ordered_map``, ``parallel_hash_map``, ``std_map``, ``unordered_flat_map``). A multimap
has none, since a ``dict`` holds one value per key; nor do the wrappers (recycling, scoped, change-logged), queues,
pools and partitions. ``urd::python::castable<S>`` says whether a Store converts.

Records and graphs are bound, not converted:

- ``bind_fields(cls)`` (``urd/python/bindings.hpp``): a record's fields as read/write attributes, and an ``__init__``
  from their values.
- ``bind_field_getters(cls)``: a view's ``get_<name>`` for each field.
- ``bind_graph(cls)`` (``urd/python/graphs.hpp``): a graph's members, as urd's graph concepts say it has them. Their
  values go through nanobind's casters: ranges as lists (``urd/python/ranges.hpp``), a graph's data by reference,
  ``None`` where it carries none (``std::monostate``).

The tests ``tests/integration/python/views.py`` and ``tests/integration/python/graphs.py`` are built with the caster test. The headers need nanobind; nothing
else in urd does. The test ``tests/integration/python/casters.py`` is built where CMake finds nanobind (``-Dnanobind_DIR=...``).
