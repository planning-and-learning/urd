Graphs
======

.. cpp:namespace:: urd

.. code-block:: cpp

   template<typename I, typename E = void>
   using Graph = Store<graph<I, E>, ...>;      // AdjacencyLists, CsrGraph, BitMatrix or BitRows

   using Follows = Store<graph<User>, frozen, bidirectional>;                        // compressed sparse rows, both ways
   using Labeled = Store<graph<State, Action>, monotone>;                            // adjacency lists, edges with data
   using Dense = Store<graph<Vertex>, frozen, constant_adjacency>;                                // rows of bits, each once
   using Compatible = Store<graph<Vertex>, frozen, partitioned, undirected, constant_adjacency>;  // the same, per part
   using Building = Store<graph<Vertex>, monotone, partitioned, undirected, constant_adjacency>; // rows that grow

.. cpp:class:: Graph

   A graph: ``Store<graph<I, E>, ...>``. The name ``Graph`` stands for the resolved Store type on this page.

.. cpp:namespace:: urd::Graph

A graph has vertices ``0..n-1`` and edges between them. Data per vertex lives beside it, in a
``Store<sequence<D, I>>`` by vertex; data per edge is ``E``, carried by each edge, or a ``Store<sequence<D>>`` by edge
number. Edges have numbers where the request is ``positional`` or has edge data: in the order they were added while the
graph grows, and by ``(source, target)`` once it is ``frozen``, parallel edges in the order given, so that a dense
graph's numbers are the ranks of its bits.

The request decides the interface: a ``frozen`` graph adds nothing, a ``monotone`` one removes nothing, edges have
numbers only where asked for, in-neighbours only with ``bidirectional``, parts only with ``partitioned``, and rows of
bits only with ``constant_adjacency``.

Template parameters
-------------------

``I``
   :cpp:concept:`indexable`: the vertex type.
``E``
   The data of an edge, or ``void`` for none.

Construction
------------

.. cpp:function:: Graph()

   An empty graph, where it grows.

.. cpp:function:: template<typename It> Graph(It first, It last)

   From edges (``urd::graph_input_edge<I, E>``: ``std::pair<I, I>``, or ``std::tuple<I, I, E>``), with one vertex past
   every endpoint.

.. cpp:function:: explicit Graph(const graph_data<graph_input_edge<I, E>>& data)

   From its vertex count, its edges and, ``partitioned``, the sizes of its parts, whose vertices are numbered part after
   part. An endpoint past the vertices throws; so does an edge inside a part, or parts that do not add up.

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

   Where it grows and is not ``partitioned``: a partitioned graph's vertices are fixed by its parts.

.. cpp:function:: std::size_t add_edge(I u, I v)
.. cpp:function:: std::size_t add_edge(I u, I v, const E& value)

   Where it grows: a new vertex; a new edge and its number, with its data where the graph has some.

.. cpp:function:: void remove_edge(std::size_t e)
.. cpp:function:: void remove_vertex(I v)

   Where it shrinks: the others keep their numbers, which are never reused.

.. cpp:function:: void clear()

Edges by number, where ``positional`` or with edge data:

.. cpp:function:: std::size_t num_edges() const
.. cpp:function:: auto edges() const
.. cpp:function:: auto out_edges(I u) const
.. cpp:function:: auto in_edges(I v) const

   Each edge as ``urd::graph_edge<I, E>``: ``source``, ``target``, ``id`` and, with edge data, ``value``.

.. cpp:function:: std::optional<std::size_t> edge(I u, I v) const
.. cpp:function:: auto edges_between(I u, I v) const

   The number of an edge from ``u`` to ``v``; with ``multi``, all of them.

.. cpp:function:: I source(std::size_t e) const
.. cpp:function:: I target(std::size_t e) const
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

Operations on any graph
-----------------------

``urd/graphs.hpp`` writes operations once from a graph's primitives (``urd::graph_like``: ``num_vertices``,
``vertices``, ``neighbours``, ``adjacent``; ``urd::partitioned_graph`` adds the parts), so they run on a graph of one's
own too. What they combine a graph's neighbours with are sets of vertices (``Store<set<I>>``), never bits. Where the
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

.. cpp:function:: template<typename To, graph_like G> To freeze(const G& g)

   The graph ``To`` (a Store of ``urd::graph<V>``) with ``g``'s vertices, edges and parts: from its edges (an undirected
   graph's both ways), or, where both keep rows of bits of one shape, a row at a time, equal rows stored once.

.. cpp:function:: template<typename To, graph_like G, typename Set> To induced(const G& g, const Set& keep)

   The graph ``To`` on the vertices ``keep`` holds, with the edges between them, numbered in order, each part keeping its
   place: from edges, or from rows masked by a dense ``keep`` a word at a time.

Complexity
----------

Adjacency lists: ``add_edge`` O(1)\*, ``remove_edge`` and ``adjacent`` O(degree). Compressed sparse rows:
``adjacent`` and ``edge`` O(log degree), ``source`` O(log n) (O(1) with ``constant_endpoints``). Rows of bits:
``adjacent`` O(1), ``neighbours`` O(n / w + degree); with a dense set, the operations on part p O(s_p / w + what they
visit), ``degree_in`` O(s_p / w), ``freeze`` and ``induced`` O(n² / w) expected.

.. urd-complexity:: graphs
