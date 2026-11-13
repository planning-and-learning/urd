"""urd's graphs in Python (urd/python/graphs.hpp): the members each graph has by urd's graph concepts."""

import importlib.util
import sys

spec = importlib.util.spec_from_file_location("urd_graphs_test", sys.argv[1])
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

# A growing graph: an incidence and edge list graph, its data a unit (None).
g = m.Growing()
a, b, c = g.add_vertex(), g.add_vertex(), g.add_vertex()
e = g.add_edge(a, b)
g.add_edge(b, c)
assert g.num_vertices == 3
assert g.edges() == [0, 1]
assert g.neighbours(a) == [b]
assert g.source(e) == a and g.target(e) == b
assert g.vertex_value(a) is None and g.edge_value(e) is None
assert not hasattr(g, "in_edges")
assert m.growing_from([(0, 1), (1, 2)]).neighbours(1) == [2]

# A frozen graph with in-edges and data: its edges keep their numbers, its data by reference.
labeled = m.Labeled([(1, 0, 7), (0, 1, 9)], ["x", "y"])
assert labeled.edges() == [0, 1]
assert labeled.edge_value(0) == 7
assert labeled.vertex_value(1) == "y"
assert labeled.in_edges(0) == [0]
assert labeled.in_neighbours(1) == [0]

# Rows of bits: graph-like, no edge numbers.
rows = m.Rows(3, [(0, 2)])
assert rows.adjacent(0, 2) and not rows.adjacent(2, 0)
assert rows.vertex_value(0) is None
assert not hasattr(rows, "edges")

print("PASSED")
