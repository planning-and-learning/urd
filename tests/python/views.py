"""Views returned to Python keep their context alive (urd/python/owner.hpp): alone, from a view, in a list or tuple, from a free
function and in a callback."""

import gc
import importlib.util
import sys
import weakref

spec = importlib.util.spec_from_file_location("urd_views_test", sys.argv[1])
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def numbers():
    numbers = m.Numbers()
    for value in (1, 2, 3):
        numbers.append(value)
    return numbers, weakref.ref(numbers)


def gone(ref):
    gc.collect()
    return ref() is None


# A view of a temporary context keeps it alive, and so does a view read from a view.
first = m.Numbers()
first.append(7)
first.append(8)
ref = weakref.ref(first)
view = first.get(0)
del first
assert not gone(ref) and view.value == 7
following = view.next()
del view
assert not gone(ref) and following.value == 8
del following
assert gone(ref)

# Views in a returned list each keep the context alive.
context, ref = numbers()
views = context.all()
del context
assert not gone(ref) and [v.value for v in views] == [1, 2, 3]
last = views[2]
del views
assert not gone(ref) and last.value == 3
del last
assert gone(ref)

# A view in a returned tuple too.
context, ref = numbers()
view, size = context.first_and_size()
del context
assert not gone(ref) and view.value == 1 and size == 3
del view
assert gone(ref)

# A free function whose context is not its first argument, and a callback.
context, ref = numbers()
view = m.view_of(1, context)
kept = []
m.each(context, kept.append)
del context
assert not gone(ref) and view.value == 2 and [v.value for v in kept] == [1, 2, 3]
del view, kept
assert gone(ref)

print("PASSED")
