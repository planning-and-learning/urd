#pragma once

// nanobind helpers for urd's elements: records and indices compare and hash as urd says, records are built from views.

#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/hash.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <string>

namespace urd::python
{
namespace nb = nanobind;

template<typename V>
    requires urd::Identifiable<V>
nb::class_<V>& add_comparison(nb::class_<V>& cls)
{
    // Nanobind adds __eq__ after type creation, so mirror Python's default: unhashable unless add_hash() opts back in.
    cls.attr("__hash__") = nb::none();

    return cls  //
        .def(
            "__eq__",
            [](const V& lhs, const V& rhs) { return lhs == rhs; },
            nb::is_operator())
        .def(
            "__lt__",
            [](const V& lhs, const V& rhs) { return lhs < rhs; },
            nb::is_operator())
        .def(
            "__le__",
            [](const V& lhs, const V& rhs) { return lhs <= rhs; },
            nb::is_operator())
        .def(
            "__gt__",
            [](const V& lhs, const V& rhs) { return lhs > rhs; },
            nb::is_operator())
        .def("__ge__", [](const V& lhs, const V& rhs) { return lhs >= rhs; }, nb::is_operator());
}

template<typename V>
nb::class_<V>& add_hash(nb::class_<V>& cls)
{
    return cls.def("__hash__", [](const V& self) { return urd::Hash<V> {}(self); });
}

/// Binds a record type; records compare by their identifying members.
template<typename V>
nb::class_<V> bind_data(nb::module_& m, const char* name)
{
    auto cls = nb::class_<V>(m, name);
    add_comparison(cls);
    return cls;
}

template<typename T>
void bind_index(nb::module_& m, const std::string& name)
{
    auto cls = nb::class_<T>(m, name.c_str());
    // urd::index's own names: the default is none, value is the integer, int() and index() give it too.
    cls.def(nb::init<>())
        .def(nb::init<typename T::value_type>(), nb::arg("index"))
        .def("__int__", [](const T& i) { return i.value; })
        .def("__index__", [](const T& i) { return i.value; })
        .def_ro("value", &T::value)
        .def("is_none", &T::is_none)
        .def_ro_static("none_value", &T::none_value);

    add_comparison(cls);
    cls.def("__hash__", [](const T& i) { return static_cast<typename T::value_type>(i.value); })
        .def("__repr__", [name](const T& i) { return name + "(" + std::to_string(static_cast<typename T::value_type>(i.value)) + ")"; });
}

/// cls.def(init_from_views<Args...>(), names...): an __init__ taking Args as Python passes them (views, readouts,
/// sequences of them, plain values), building the record from their stored forms (urd::to_stored).
template<typename... Args>
struct init_from_views : nb::def_visitor<init_from_views<Args...>>
{
    template<typename Class, typename... Extra>
    void execute(Class& cls, const Extra&... extra) const
    {
        using Record = typename Class::Type;
        cls.def("__init__", [](Record* self, Args... arguments) { new (self) Record(urd::to_stored(arguments)...); }, extra...);
    }
};
}  // namespace urd::python
