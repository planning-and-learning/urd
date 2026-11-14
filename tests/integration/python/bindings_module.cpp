// A module of urd's binding helpers (urd/python/bindings.hpp), for tests/integration/python/bindings.py: an index bound
// with bind_index.

#include "urd/elements/index.hpp"
#include "urd/python/bindings.hpp"

#include <nanobind/nanobind.h>

namespace
{

struct Atom
{
};

}  // namespace

NB_MODULE(urd_bindings_test, m) { urd::python::bind_index<urd::index<Atom>>(m, "AtomIndex"); }
