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
