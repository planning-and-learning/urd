# Copyright (C) 2026 Dominik Drexler
# SPDX-License-Identifier: GPL-3.0-or-later
"""An index bound with urd::python::bind_index (urd/python/bindings.hpp): its value, none, comparison, hash and repr."""

import importlib.util
import sys

spec = importlib.util.spec_from_file_location("urd_bindings_test", sys.argv[1])
assert spec is not None
assert spec.loader is not None
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

three = m.AtomIndex(3)
assert three.value == 3
assert int(three) == 3
assert [0, 1, 2, 3][three] == 3
assert not three.is_none()
assert m.AtomIndex().is_none()
assert m.AtomIndex().value == m.AtomIndex.none_value
assert three == m.AtomIndex(3)
assert three != m.AtomIndex(4)
assert m.AtomIndex(2) < three <= m.AtomIndex(3)
assert m.AtomIndex(4) > three >= m.AtomIndex(3)
assert hash(three) == hash(m.AtomIndex(3))
assert len({three, m.AtomIndex(3), m.AtomIndex(4)}) == 2
assert repr(three) == "AtomIndex(3)"
sys.stdout.write("PASSED\n")

# bind_data: TermIndex and TermData (from its fields, each read and written, compared); bind_view: Term (its view).
data = m.TermData(2, [4, 5], None)
assert data.arity == 2
assert data.objects == [4, 5]
data.objects = [6]
assert data.objects == [6]
assert m.TermData(2, [4, 5], None) == m.TermData(2, [4, 5], None)
assert m.TermData(2, [4, 5], None) != m.TermData(3, [4, 5], None)
term = m.term(0)
assert term.index() == m.TermIndex(0)
assert term.get_arity() == 2
assert term.get_objects() == [4, 5]
assert term == m.term(0)
assert hash(term) == hash(m.term(0))
assert term.arity_plus_one() == 3
# A view converts to its index wherever an index is expected: TermIndex(view), and the Data __init__'s index fields.
assert m.TermIndex(term) == m.TermIndex(0)
assert str(term) == repr(term) == "Term(\n    arity=2\n    objects=[\n        4\n        5\n    ]\n    weight=none\n)"
assert str(m.TermData(1, [], 3)) == "Term(\n    arity=1\n    objects=[]\n    weight=3\n)"
# An optional field takes None and a value alike.
assert m.TermData(2, [], 7).weight == 7 and m.TermData(2, [], None).weight is None
# bind_view of a builder's view: the same getters, compared and hashed by the builder, no index.
builder = m.term_builder()
assert builder.get_arity() == 1
assert builder.get_objects() == [9]
assert builder.get_weight() is None
assert builder == m.term_builder()
assert hash(builder) == hash(m.term_builder())
assert str(builder) == "Term(\n    arity=1\n    objects=[\n        9\n    ]\n    weight=none\n)"
assert type(builder) is m.TermBuilder
assert m.pair_name() == "PairTermPairAtomTerm"
assert not hasattr(builder, "index")
# A view that owns its record is bound beside the record's other views, under its own name.
owned = m.term_owned()
assert type(owned) is m.TermOwned and m.Term is not m.TermOwned
assert owned.get_arity() == 3
# bind_value: a plain value, from its fields, each read and written, compared, written with fmt.
point = m.Point(1, 2)
assert (point.x, point.y) == (1, 2)
point.y = 3
assert point == m.Point(1, 3) and point != m.Point(1, 2)
assert str(point) == "(1, 3)"
