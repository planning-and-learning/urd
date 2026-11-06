// A module of views returned to Python (urd/python/owner.hpp), for tests/python/views.py: each keeps its context alive.

#include "urd/elements/view.hpp"
#include "urd/python/owner.hpp"

#include <cstdint>
#include <nanobind/nanobind.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>
#include <tuple>
#include <vector>

namespace nb = nanobind;

namespace
{

struct Number
{
    int value;
};

struct Numbers
{
    std::vector<Number> records;

    const Number& operator[](urd::index<Number> at) const { return records[at.value]; }
    std::size_t layer() const noexcept { return 0; }
    urd::view<Number, Numbers> view(urd::index<Number> at) const { return { at, *this }; }
};

using NumberView = urd::view<Number, Numbers>;
static_assert(urd::resolves<Numbers, Number>);


}  // namespace

NB_MODULE(urd_views_test, m)
{
    nb::class_<Numbers>(m, "Numbers", nb::is_weak_referenceable())
        .def(nb::init<>())
        .def("append", [](Numbers& numbers, int value) { numbers.records.push_back(Number { value }); })
        .def("get", [](const Numbers& numbers, std::uint32_t at) { return numbers.view(urd::index<Number>(at)); })
        .def("all",
             [](const Numbers& numbers)
             {
                 auto views = std::vector<NumberView>();
                 for (std::uint32_t at = 0; at < numbers.records.size(); ++at)
                     views.push_back(numbers.view(urd::index<Number>(at)));
                 return views;
             })
        .def("first_and_size", [](const Numbers& numbers) { return std::make_tuple(numbers.view(urd::index<Number>(0)), numbers.records.size()); });

    // Without self: the view keeps its context's own Python object alive, an argument or not.
    m.def("view_of", [](std::uint32_t at, const Numbers& numbers) { return numbers.view(urd::index<Number>(at)); });
    m.def("each",
          [](const Numbers& numbers, const nb::callable& callback)
          {
              for (std::uint32_t at = 0; at < numbers.records.size(); ++at)
                  callback(numbers.view(urd::index<Number>(at)));
          });

    nb::class_<NumberView>(m, "NumberView")
        .def_prop_ro("value", [](const NumberView& view) { return view.data().value; })
        .def("next", [](const NumberView& view) { return view.context().view(urd::index<Number>(view.index().value + 1)); });
}
