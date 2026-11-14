// A module of round trips through urd's nanobind casters (urd/python/): a Store goes to Python and back, by
// representation, and so do the flat forms of records (urd/python/flat.hpp) and views of them (urd/python/ranges.hpp),
// for tests/integration/python/casters.py.

#include "urd/elements/flat.hpp"
#include "urd/python/flat.hpp"
#include "urd/python/maps/std_map.hpp"
#include "urd/python/maps/unordered_flat_map.hpp"
#include "urd/python/ranges.hpp"
#include "urd/python/sequences/segmented.hpp"
#include "urd/python/sequences/sequence.hpp"
#include "urd/python/values/indexed.hpp"
#include "urd/python/values/std_set.hpp"
#include "urd/python/values/unordered_flat_set.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/store.hpp"

#include <cista/containers/array.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <ranges>
#include <string>

namespace
{

using Sequence = urd::Store<urd::sequence<int>>;
using StableSequence = urd::Store<urd::sequence<int>, urd::stable_refs>;
using Set = urd::Store<urd::set<int>>;
using SortedSet = urd::Store<urd::set<int>, urd::sorted>;
using Map = urd::Store<urd::map<std::string, int>>;
using SortedMap = urd::Store<urd::map<int, std::string>, urd::sorted>;
using Nested = urd::Store<urd::sequence<urd::Store<urd::sequence<int>>>>;
// A positional set hands out indices: its representation is a numbered indexed set (urd::repr::Numbered).
using Positional = urd::Store<urd::set<int>, urd::positional, urd::monotone>;
static_assert(requires(const Positional::representation& numbered) { numbered.layer(); });

static_assert(urd::python::castable<Sequence> && urd::python::castable<StableSequence> && urd::python::castable<Set> && urd::python::castable<SortedSet>
              && urd::python::castable<Map> && urd::python::castable<SortedMap> && urd::python::castable<Positional>);
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
    m.def("positional_set",
          [](const Sequence& values)
          {
              auto positions = Positional(urd::layer { 0 });
              for (const int value : values)
                  positions.insert(value);
              return positions;
          });
    m.def("sizes",
          [](const Nested& values)
          {
              auto sizes = Sequence();
              for (const auto& row : values)
                  sizes.push_back(static_cast<int>(row.size()));
              return sizes;
          });
    m.def("array_view",
          []
          {
              using Array = ::cista::array<int, 3>;
              static constexpr auto data = Array { 1, 2, 3 };
              return std::views::all(data);
          });
    m.def("pair_view",
          []
          {
              using Pair = urd::flat_pair<int, int>;
              static constexpr auto data = Pair { 4, 5 };
              return data;
          });
    m.def("nested_view",
          []
          {
              using Pair = urd::flat_pair<int, int>;
              using Array = ::cista::array<Pair, 2>;
              static constexpr auto data = Array { Pair { 6, 7 }, Pair { 8, 9 } };
              return std::views::all(data);
          });
    m.def("roundtrip_pair", [](urd::flat_pair<int, int> value) { return value; });
    m.def("roundtrip_optional_pairs", [](urd::flat_vector<urd::flat_optional<urd::flat_pair<int, int>>> values) { return values; });
    m.def("roundtrip_variant", [](urd::flat_variant<int, double> value) { return value; });
}
