Graphs
======

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename I, typename E = std::monostate, typename D = std::monostate>
   using Graph = Store<graph<I, E, D>, ...>;   // AdjacencyLists, CsrGraph, BitMatrix or BitRows

   using Follows = Store<graph<User>, frozen, bidirectional>;                        // compressed sparse rows, both ways
   using Labeled = Store<graph<State, Action, Label>, monotone>;                     // adjacency lists, data on both
   using Dense = Store<graph<Vertex>, frozen, constant_adjacency>;                                // rows of bits, each once
   using Compatible = Store<graph<Vertex>, frozen, partitioned, undirected, constant_adjacency>;  // the same, per part
   using Building = Store<graph<Vertex>, monotone, partitioned, undirected, constant_adjacency>; // rows that grow

.. cpp:class:: Graph

   A graph: ``Store<graph<I, E, D>, ...>``. The name ``Graph`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Graph

A graph has vertices ``0..n-1`` and edges between them, and keeps the data of each: ``D`` per vertex, ``E`` per edge.
Data kept elsewhere (shared, say) goes by a vertex's index or an edge's number. A graph without data has data of a type
with one value, ``std::monostate`` (regular void): every graph has ``vertex_value`` and ``edge_value``, and code written
for graphs never asks whether there is data. A value of a type with one value (``urd::unit``) may be left out, since
nothing is lost: an edge given as a pair, a vertex or edge added without data, no ``vertex_values`` given. A graph that
keeps edge lists (every one but rows of bits) numbers its edges and hands them out as these numbers: their order as
added or as given, kept when the graph is frozen (where none was removed) and in the order of their numbers in a
subgraph. An edge's
endpoints and data are read by its number.

The request decides the interface: a ``frozen`` graph adds nothing, a ``monotone`` one removes nothing, edges have
numbers wherever the graph keeps edge lists, in-neighbours only with ``bidirectional``, parts only with ``partitioned``, and rows of
bits only with ``constant_adjacency``.

Template parameters
-------------------

``I``
   :cpp:concept:`indexable`: the vertex type.
``E``
   The data of an edge; ``std::monostate`` for none.
``D``
   The data of a vertex; ``std::monostate`` for none. Rows of bits (``constant_adjacency``) keep no data.

Construction
------------

.. cpp:function:: Graph()

   An empty graph, where it grows.

.. cpp:function:: template<typename It> Graph(It first, It last)

   From edges (``urd::graph_input_edge<I, E>``: ``std::tuple<I, I, E>``, or ``std::pair<I, I>`` where ``E`` is a unit),
   with one vertex past every endpoint, where ``D`` is a unit.

.. cpp:function:: template<typename Edge> explicit Graph(const graph_data<Edge, D>& data)

   From its vertex count, its edges (each numbered by its position; any ``urd::input_edge_of`` it), its vertices' data
   ``vertex_values`` (one per vertex; none where ``D`` is a unit) and, ``partitioned``, the sizes of its parts, whose vertices are numbered part after part. An endpoint past
   the vertices throws; so does an edge inside a part, parts that do not add up, or vertex data of another count.

Member functions
----------------

.. cpp:function:: std::size_t num_vertices() const
.. cpp:function:: auto vertices() const

   The vertices, in increasing order.

.. cpp:function:: auto neighbours(I u) const
.. cpp:function:: std::size_t out_degree(I u) const

   The targets of the edges leaving ``u``, a target once per edge; with ``undirected``, every neighbour.

.. cpp:function:: bool adjacent(I u, I v) const
.. cpp:function:: auto in_neighbours(I v) const
.. cpp:function:: std::size_t in_degree(I v) const

   With ``bidirectional``: the sources of the edges entering ``v``.

.. cpp:function:: I add_vertex()
.. cpp:function:: I add_vertex(const D& value)

   Where it grows and is not ``partitioned``: a partitioned graph's vertices are fixed by its parts. Without a value
   only where ``D`` is a unit.

.. cpp:function:: const D& vertex_value(I v) const

.. cpp:function:: std::size_t add_edge(I u, I v)
.. cpp:function:: std::size_t add_edge(I u, I v, const E& value)

   Where it grows: a new edge and its number; without a value only where ``E`` is a unit.

.. cpp:function:: void remove_edge(std::size_t e)
.. cpp:function:: void remove_vertex(I v)

   Where it shrinks: the others keep their numbers, which are never reused.

.. cpp:function:: void clear()

Edges by number, where the graph keeps edge lists (not ``constant_adjacency``):

.. cpp:function:: std::size_t num_edges() const
.. cpp:function:: auto edges() const
.. cpp:function:: auto out_edges(I u) const
.. cpp:function:: auto in_edges(I v) const

   Each edge as its number (``std::size_t``).

.. cpp:function:: std::optional<std::size_t> edge(I u, I v) const
.. cpp:function:: auto edges_between(I u, I v) const

   The number of an edge from ``u`` to ``v``; with ``multi``, all of them.

.. cpp:function:: I source(std::size_t e) const
.. cpp:function:: I target(std::size_t e) const
.. cpp:function:: const E& edge_value(std::size_t e) const
.. cpp:function:: const E& operator[](std::size_t e) const

Parts, where ``partitioned``:

.. cpp:function:: std::size_t num_parts() const
.. cpp:function:: std::size_t part_of(I v) const
.. cpp:function:: auto part(std::size_t p) const

A dense graph (``constant_adjacency``) keeps a row of bits per vertex and part: its vertices are fixed when it is built,
``adjacent`` is O(1), and the operations below combine its rows with sets of vertices a word at a time. Its bits stay
inside urd. It grows as a matrix (``monotone``: ``add_edge`` says whether the edge is new) and is frozen into rows
stored once each (``urd::freeze``). Not ``partitioned``, all its vertices are one part; ``undirected`` sets an edge
both ways; ``bidirectional`` keeps in-rows too.

Concepts
--------

``urd/graphs.hpp`` names what a graph offers after the Boost Graph Library's concepts, each a set of primitives, so
that an operation states what it needs and a graph of one's own with those primitives qualifies too.

.. cpp:concept:: template<typename G> vertex_list_graph

   ``vertex_type``, ``num_vertices()``, ``vertices()`` in increasing order.

.. cpp:concept:: template<typename G> adjacency_graph

   Adds ``neighbours(u)`` and ``out_degree(u)``.

.. cpp:concept:: template<typename G> adjacency_matrix

   Adds ``adjacent(u, v)`` to a vertex list.

.. cpp:concept:: template<typename G> graph_like

   An adjacency graph and an adjacency matrix: every urd graph.

.. cpp:concept:: template<typename G> in_adjacency_graph

   Adds ``in_neighbours(v)`` and ``in_degree(v)`` (``bidirectional`` or ``undirected``).

.. cpp:concept:: template<typename G> incidence_graph

   Edges by number: adds ``out_edges(u)``, ``source(e)`` and ``target(e)``. Every graph that keeps edge lists.

.. cpp:concept:: template<typename G> bidirectional_graph

   An incidence graph with ``in_edges(v)`` and in-neighbours.

.. cpp:concept:: template<typename G> edge_list_graph

   ``edges()``, ``num_edges()``, ``source(e)`` and ``target(e)``. Every graph that keeps edge lists.

.. cpp:concept:: template<typename G> mutable_graph

   ``add_vertex()`` and ``add_edge(u, v)``.

.. cpp:concept:: template<typename G> partitioned_graph

   A graph with parts: ``num_parts()``, ``part_of(v)``, ``part(p)``.

Rows of bits (``constant_adjacency``) are graph-like, with in-neighbours where they keep them, but no incidence or edge
list graphs: they have no edge numbers.

Operations on any graph
-----------------------

``urd/graphs.hpp`` writes operations once from a graph's primitives (``urd::graph_like``; ``urd::partitioned_graph``
adds the parts), so they run on a graph of one's own too. What they combine a graph's neighbours with are sets of vertices (``Store<set<I>>``), never bits. Where the
graph keeps rows of bits (``constant_adjacency``) and the set is dense (``Store<set<I>, dense>``), they go a word at a
time inside urd: an operation on part ``p`` costs the words of ``p``, the set built over the part's window of keys
(``urd::key_range``) or over all vertices. Elsewhere they go a vertex at a time.

.. cpp:function:: template<partitioned_graph G, typename Set> void copy_neighbours(const G& g, I v, std::size_t p, Set& into)
.. cpp:function:: template<partitioned_graph G, typename Set> void intersect_neighbours(const G& g, I v, std::size_t p, Set& into)
.. cpp:function:: template<partitioned_graph G, typename Set> void subtract_neighbours(const G& g, I v, std::size_t p, Set& into)

   ``into`` set to, kept to, or cleared of the neighbours of ``v`` in part ``p``; without ``p``, of all of ``v``'s
   neighbours.

.. cpp:function:: template<partitioned_graph G> std::size_t degree_in(const G& g, I v, std::size_t p)

   How many neighbours ``v`` has in part ``p``.

.. cpp:function:: template<partitioned_graph G, typename Set, typename F> void for_each_neighbour_in(const G& g, I v, std::size_t p, const Set& among, F f)

   ``f`` on each neighbour of ``v`` in part ``p`` (without ``p``, of ``v``) that ``among`` holds: the vertices still
   live, say.

.. cpp:function:: template<typename To, edge_list_graph G> To freeze(const G& g)

   The graph ``To`` (a Store of ``urd::graph<V, E, D>``) with ``g``'s vertices, edges, data and parts, from its edges in
   the order of their numbers; or, where both keep rows of bits of one shape, a row at a time, equal rows stored once.
   Rows of bits are no edge list: they freeze only into rows of their shape.

.. cpp:function:: template<typename To, edge_list_graph G, typename Set> To induced(const G& g, const Set& keep)

   The graph ``To`` on the vertices ``keep`` holds, with the edges between them and the data, numbered in order, each
   part keeping its place: from edges, or, rows of bits, from rows masked by a dense ``keep`` a word at a time.

Complexity
----------

Adjacency lists: ``add_edge`` O(1)\*, ``remove_edge`` and ``adjacent`` O(degree). Compressed sparse rows:
``adjacent`` and ``edge`` O(log degree), ``source`` O(log n) (O(1) with ``constant_endpoints``). Rows of bits:
``adjacent`` O(1), ``neighbours`` O(n / w + degree); with a dense set, the operations on part p O(s_p / w + what they
visit), ``degree_in`` O(s_p / w), ``freeze`` and ``induced`` O(n² / w) expected.

.. urd-complexity:: graphs
