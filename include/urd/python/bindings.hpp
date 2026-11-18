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
#include <type_traits>
#include <utility>

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
    // An index is bound once, by whichever module binds it first (a tag one library defines and another stores records
    // of: ygg::formalism::Object); every other module names that class.
    if (const auto bound = nb::type<T>(); bound.is_valid())
    {
        m.attr(name.c_str()) = bound;
        return nb::borrow<nb::class_<T>>(bound);
    }
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
    requires HasFields<record_of<View>>
void bind_field_getters(nb::class_<View>& cls)
{
    for_each_field<record_of<View>>(
        [&]<typename F>()
        { cls.def(F::template prefixed<"get_">.data(), [](const View& view) -> decltype(auto) { return view.template member<F::member>(); }); });
}

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

namespace detail
{
/// Whether a view owns its record (owning_view): bound beside a view of the same record that does not.
template<typename View>
inline constexpr bool owns_record = false;
template<typename T, typename C, typename R>
inline constexpr bool owns_record<owning_view<T, C, R>> = true;
}  // namespace detail

namespace detail
{
/// A view's function F bound as a method of View: a member of its mixin called on the view, a free function with the
/// view first as it is. A reference it returns refers into the view's context (reference_internal: Python keeps the view
/// alive); a view or a value is returned as nanobind returns one (a view keeps its context alive, urd/python/owner.hpp).
template<typename View, auto F, typename R, typename... Args>
constexpr auto method(R (* /*function*/)(const View&, Args...))
{ return F; }

template<typename View, auto F, typename R, typename C, typename... Args>
constexpr auto method(R (C::* /*member*/)(Args...) const)
{
    return +[](const View& view, Args... args) -> R { return (view.*F)(std::forward<Args>(args)...); };
}

template<typename View, auto F, typename R, typename C, typename... Args>
constexpr auto method(R (C::* /*member*/)(Args...) const noexcept)
{
    return +[](const View& view, Args... args) -> R { return (view.*F)(std::forward<Args>(args)...); };
}

template<typename View, auto F, typename R, typename... Args>
constexpr auto method(R (* /*function*/)(const View&, Args...) noexcept)
{ return F; }

/// What a function returns.
template<typename R, typename... Args>
R returned(R (* /*function*/)(Args...));
template<typename R, typename... Args>
R returned(R (* /*function*/)(Args...) noexcept);
}  // namespace detail

/// Each function a view's mixin lists (urd::Functions), a method of View under its listed name.
template<typename View>
void bind_functions(nb::class_<View>& cls)
{
    if constexpr (HasFunctions<View>)
        for_each_function<View>(
            [&]<typename F>()
            {
                constexpr auto bound = detail::method<View, F::pointer>(F::pointer);
                // Each parameter under its C++ name (urd::arg), so that Python passes it by keyword too.
                F::with_arguments(
                    [&]<typename... Args>()
                    {
                        if constexpr (std::is_lvalue_reference_v<decltype(detail::returned(bound))>)
                            cls.def(F::c_name, bound, nb::arg(Args::c_name)..., nb::rv_policy::reference_internal);
                        else
                            cls.def(F::c_name, bound, nb::arg(Args::c_name)...);
                    });
            });
}

/// Binds the view View under the name of its tag (urd::name<T>(), T its value_type; <name>Builder for a view of a
/// builder<T, C>, <name>Owned for a view that owns its record, so that the views of one record bound in one module have
/// their own names), the one way a view is bound, whether it reads a stored record (data<T>), a record a context hands out
/// in another layout (a value with the record's fields) or a builder (builder<T, C>): get_<field>() for each field of its
/// record, compared and hashed by its identity, written with fmt (a view needs its fmt::formatter). A view
/// that names its index (index_view: index() gives its key, an urd::index<T> or a key of its own such as a row's) also
/// gives index() and converts to that key wherever one is expected, also in lists and optionals; the key is bound first
/// (T's Index by bind_data<T>, another key by bind_value).
template<typename View>
    requires HasFields<record_of<View>>
nb::class_<View> bind_view(nb::module_& m)
{
    using T = typename View::value_type;
    const auto name = urd::name<T>() + (urd::is_builder<record_of<View>> ? "Builder" : "") + (detail::owns_record<View> ? "Owned" : "");
    auto view = nb::class_<View>(m, name.c_str());
    bind_field_getters(view);
    bind_functions(view);
    // Its context, referred to, not copied: what a view is read through (a repository, a builder) is reached the same
    // way from every view, with no getter of its own.
    view.def("context", &View::context, nb::rv_policy::reference_internal);
    add_comparison(view);
    add_hash(view);
    static_assert(fmt::is_formattable<View>::value, "urd::python::bind_view: a view is written with fmt: specialize fmt::formatter for it");
    view.def("__str__", [](const View& v) { return fmt::format("{}", v); }).def("__repr__", [](const View& v) { return fmt::format("{}", v); });
    if constexpr (requires(const View& v) { v.index(); })
    {
        using Handle = std::remove_cvref_t<decltype(std::declval<const View&>().index())>;
        view.def("index", &View::index);
        const auto index_type = nb::type<Handle>();
        if (!index_type.is_valid())
            throw std::logic_error("urd::python::bind_view: " + name + " names its index: bind its index first (bind_data<T>, or bind_value of its key).");
        nb::borrow<nb::class_<Handle>>(index_type).def("__init__", [](Handle* self, const View& v) { new (self) Handle(v.index()); });
        nb::implicitly_convertible<View, Handle>();
    }
    return view;
}

}  // namespace urd::python
