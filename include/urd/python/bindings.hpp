#pragma once

// nanobind helpers for urd's elements: records and indices compare and hash as urd says, records are built from views,
// and a record is bound by bind_data, its views by bind_view.

#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/view.hpp"
#include "urd/format.hpp"
#include "urd/python/flat.hpp"

#include <fmt/format.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <stdexcept>
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

template<typename T>
nb::class_<T> bind_index(nb::module_& m, const std::string& name)
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
    return cls;
}

/// The fields of record Data (urd::Fields), each a read/write attribute under its name, and an __init__ from their
/// values, in order.
template<typename Data>
    requires HasFields<Data>
void bind_fields(nb::class_<Data>& cls)
{
    for_each_field<Data>([&]<typename F>() { cls.def_rw(F::c_name, F::member); });
    [&]<typename... Fs>(Fields<Fs...> /*fields*/) { cls.def(nb::init<member_type_t<Fs::member>...>(), nb::arg(Fs::c_name)...); }(Data::fields());
}

/// A view's getters, get_<name> for each field of its record: the field resolved through the view's context.
template<typename View>
    requires HasFields<fields_record_t<View>>
void bind_field_getters(nb::class_<View>& cls)
{
    for_each_field<fields_record_t<View>>(
        [&]<typename F>()
        { cls.def(F::template prefixed<"get_">.data(), [](const View& view) -> decltype(auto) { return view.template member<F::member>(); }); });
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
/// Binds the plain value T (no record: a struct of fields, ColumnLayout, a row's key) under its name (urd::name<T>()):
/// an __init__ from its fields in order, each field read and written under its name, compared by its identifying
/// members, written with fmt (a value needs its fmt::formatter).
template<typename T>
    requires HasFields<T>
nb::class_<T> bind_value(nb::module_& m)
{
    auto value = nb::class_<T>(m, urd::name<T>().c_str()).def(nb::init<>());
    bind_fields(value);
    add_comparison(value);
    static_assert(fmt::is_formattable<T>::value, "urd::python::bind_value: a value is written with fmt: specialize fmt::formatter for it");
    value.def("__str__", [](const T& v) { return fmt::format("{}", v); }).def("__repr__", [](const T& v) { return fmt::format("{}", v); });
    return value;
}

/// Binds the record T under its name (urd::name<T>(), <name> below), the one way a record is bound: <name>Index (bind_index, where its record holds an
/// urd::index<T> index) and <name>Data (an __init__ from its fields in order, each field read and written under its
/// name, compared by its identifying members, written with fmt). Every name follows the record's tag and fields(), so no
/// binding names a record or a field by hand. Its views are bound after it, with bind_view.
template<typename T>
    requires HasFields<urd::data<T>>
nb::class_<urd::data<T>> bind_data(nb::module_& m)
{
    const auto name = urd::name<T>();
    if constexpr (requires(const urd::data<T>& record) { requires std::same_as<decltype(record.index), urd::index<T>>; })
        bind_index<urd::index<T>>(m, name + "Index");
    auto data = nb::class_<urd::data<T>>(m, (name + "Data").c_str()).def(nb::init<>());
    bind_fields(data);
    add_comparison(data);
    data.def("__str__", [](const urd::data<T>& d) { return fmt::format("{}", d); }).def("__repr__", [](const urd::data<T>& d) { return fmt::format("{}", d); });
    return data;
}

/// Binds the view View under the name of its tag (urd::name<T>(), T its value_type; <name>Builder for a view of a
/// builder<T>), the one way a view is bound, whether it reads a stored record (data<T>), a record stored in another
/// layout (read through urd::stored_member) or a builder (builder<T>): get_<field>() for each field of its record
/// (fields_record_t), compared and hashed by its identity, written with fmt (a view needs its fmt::formatter). A view
/// that names its index (index_view: index() gives an urd::index<T>) also gives index() and converts to T's Index
/// wherever an index is expected, also in lists and optionals; that Index is bound first, by bind_data<T>.
template<typename View>
    requires HasFields<fields_record_t<View>>
nb::class_<View> bind_view(nb::module_& m)
{
    using T = typename View::value_type;
    const auto name = urd::name<T>() + (std::same_as<record_of<View>, urd::builder<T>> ? "Builder" : "");
    auto view = nb::class_<View>(m, name.c_str());
    bind_field_getters(view);
    add_comparison(view);
    add_hash(view);
    static_assert(fmt::is_formattable<View>::value, "urd::python::bind_view: a view is written with fmt: specialize fmt::formatter for it");
    view.def("__str__", [](const View& v) { return fmt::format("{}", v); }).def("__repr__", [](const View& v) { return fmt::format("{}", v); });
    if constexpr (requires(const View& v) { requires std::same_as<decltype(v.index()), urd::index<typename View::value_type>>; })
    {
        using Handle = urd::index<typename View::value_type>;
        view.def("index", &View::index);
        const auto index_type = nb::type<Handle>();
        if (!index_type.is_valid())
            throw std::logic_error("urd::python::bind_view: " + name + " names its index: bind its record with bind_data first.");
        nb::borrow<nb::class_<Handle>>(index_type).def("__init__", [](Handle* self, const View& v) { new (self) Handle(v.index()); });
        nb::implicitly_convertible<View, Handle>();
    }
    return view;
}

}  // namespace urd::python
