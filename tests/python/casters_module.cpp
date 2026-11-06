// A module of round trips through urd's nanobind casters (urd/python/): a Store goes to Python and back, by
// representation, for tests/python/casters.py.

#include "urd/python/maps/std_map.hpp"
#include "urd/python/maps/unordered_flat_map.hpp"
#include "urd/python/sequences/segmented.hpp"
#include "urd/python/sequences/sequence.hpp"
#include "urd/python/values/indexed.hpp"
#include "urd/python/values/std_set.hpp"
#include "urd/python/values/unordered_flat_set.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/store.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <string>

namespace nb = nanobind;

namespace
{

using Sequence = urd::Store<urd::sequence<int>>;
using StableSequence = urd::Store<urd::sequence<int>, urd::stable_refs>;
using Set = urd::Store<urd::set<int>>;
using SortedSet = urd::Store<urd::set<int>, urd::sorted>;
using Map = urd::Store<urd::map<std::string, int>>;
using SortedMap = urd::Store<urd::map<int, std::string>, urd::sorted>;
using Nested = urd::Store<urd::sequence<urd::Store<urd::sequence<int>>>>;

static_assert(urd::python::castable<Sequence> && urd::python::castable<StableSequence> && urd::python::castable<Set>
              && urd::python::castable<SortedSet> && urd::python::castable<Map> && urd::python::castable<SortedMap>);
static_assert(!urd::python::castable<urd::Store<urd::map<int, int>, urd::multi>>, "a multimap has no caster: a dict keeps one value per key");

}  // namespace

NB_MODULE(urd_casters_test, m)
{
    m.def("sequence", [](Sequence values) { return values; });
    m.def("stable_sequence", [](StableSequence values) { return values; });
    m.def("set", [](Set values) { return values; });
    m.def("sorted_set", [](SortedSet values) { return values; });
    m.def("map", [](Map values) { return values; });
    m.def("sorted_map", [](SortedMap values) { return values; });
    m.def("nested", [](Nested values) { return values; });
    m.def("sizes", [](const Nested& values) {
        auto sizes = Sequence();
        for (const auto& row : values)
            sizes.push_back(static_cast<int>(row.size()));
        return sizes;
    });
}
