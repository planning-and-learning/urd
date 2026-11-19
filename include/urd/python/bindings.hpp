#pragma once

// nanobind helpers for urd's elements: records and indices compare and hash as urd says, records are built from views,
// and a record is bound by bind_data, its builder by bind_builder, its views by bind_view.

#include "urd/elements/builder.hpp"
#include "urd/elements/children.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/view.hpp"
#include "urd/format.hpp"
#include "urd/python/flat.hpp"

#include <cstddef>
#include <fmt/format.h>
#include <nanobind/nanobind.h>
// A builder's built forms are std's containers (urd::built_form): its view's getters return them
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/vector.h>
#include <stdexcept>
#include <string>
#include <tuple>
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

/// Binds urd::index<Tag> under <name>Index (urd::name<Tag>()), the one way an index is bound: a record's (bound before
/// bind_data of a record that holds one) or one of a tag of no record (a column, a slot). An index is bound once, by
/// whichever module binds it first (a tag one library defines and another stores records of: ygg::formalism::Object);
/// every other module names that class.
template<typename Tag>
nb::class_<urd::index<Tag>> bind_index(nb::module_& m)
{
    using T = urd::index<Tag>;
    const auto name = urd::name<Tag>() + "Index";
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
    add_hash(cls);
    cls.def("__repr__", [name](const T& i) { return name + "(" + std::to_string(static_cast<typename T::value_type>(i.value)) + ")"; });
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

/// Binds the record T as <name>Data (urd::name<T>()), the one way a record is bound: an __init__ from its fields in
/// order, each field read and written under its name, compared by its identifying members, written with fmt. Every name
/// follows the record's tag and fields(), so no binding names a record or a field by hand. A record that holds its index
/// has it bound first (bind_index<T>); its builder (bind_builder) and its views (bind_view) are bound after it.
template<typename T>
    requires HasFields<urd::data<T>>
nb::class_<urd::data<T>> bind_data(nb::module_& m)
{
    const auto name = urd::name<T>();
    if constexpr (urd::SelfIndexed<T>)
    {
        if (!nb::type<urd::index<T>>().is_valid())
            throw std::logic_error("urd::python::bind_data: " + name + " holds its index: bind it first (bind_index<T>).");
    }
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

/// Whether a view borrows its record (borrowed_view: of a data<T> or a builder<T, C>): bound beside the view of the same
/// record that holds its index, and beside the builder (bind_builder).
template<typename View>
inline constexpr bool borrows_record = false;
template<typename T, typename C, typename R>
inline constexpr bool borrows_record<borrowed_view<T, C, R>> = true;

/// Whether a view shares its record (shared_view): bound beside a view of the same record that borrows it.
template<typename View>
inline constexpr bool shares_record = false;
template<typename T, typename C, typename R>
inline constexpr bool shares_record<shared_view<T, C, R>> = true;
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

/// A builder's mutator (bind_builder): a member that changes it, called on it.
template<typename View, auto F, typename R, typename C, typename... Args>
constexpr auto method(R (C::* /*member*/)(Args...))
{
    return +[](View& built, Args... args) -> R { return (built.*F)(std::forward<Args>(args)...); };
}

template<typename View, auto F, typename R, typename C, typename... Args>
constexpr auto method(R (C::* /*member*/)(Args...) noexcept)
{
    return +[](View& built, Args... args) -> R { return (built.*F)(std::forward<Args>(args)...); };
}

/// What a builder's mutator may keep pointers into: each argument it takes by reference (a repository its children are
/// read through, a Store its rows are drawn from), kept alive by the builder (nb::keep_alive<1, its position>; self is
/// 1). A view's function (its self const) keeps nothing.
template<typename Self, typename... Args, std::size_t... Is>
constexpr auto kept(std::index_sequence<Is...> /*positions*/)
{
    if constexpr (std::is_const_v<std::remove_reference_t<Self>>)
        return std::tuple<>();
    else
        return std::tuple_cat(std::conditional_t<std::is_lvalue_reference_v<Args>, std::tuple<nb::keep_alive<1, Is + 2>>, std::tuple<>>()...);
}

template<typename R, typename Self, typename... Args>
constexpr auto kept_of(R (* /*function*/)(Self, Args...))
{ return kept<Self, Args...>(std::index_sequence_for<Args...>()); }

template<typename R, typename Self, typename... Args>
constexpr auto kept_of(R (* /*function*/)(Self, Args...) noexcept)
{ return kept<Self, Args...>(std::index_sequence_for<Args...>()); }

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
                        std::apply(
                            [&](auto... keep)
                            {
                                if constexpr (std::is_lvalue_reference_v<decltype(detail::returned(bound))>)
                                    cls.def(F::c_name, bound, nb::arg(Args::c_name)..., nb::rv_policy::reference_internal, keep...);
                                else
                                    cls.def(F::c_name, bound, nb::arg(Args::c_name)..., keep...);
                            },
                            detail::kept_of(bound));
                    });
            });
}

/// Binds the builder<T, C> under <name>Builder (urd::name<T>()), the one way a builder is bound: an __init__ of an empty
/// one; set_<field>(context, value) for each field, the value in its record's form (data<T>'s field: a key, a list of
/// keys, a number; a view converts to its key) made the field's built form through the context (built_form::from); the
/// mutators its functions() lists, under their names; and view(context), its view through the context
/// (<name>BuilderBorrowed), which every read goes through, as a record's do. Bound after bind_data<T>.
template<typename T, typename C>
    requires HasFields<builder<T, C>>
nb::class_<builder<T, C>> bind_builder(nb::module_& m)
{
    using B = builder<T, C>;
    auto cls = nb::class_<B>(m, (urd::name<T>() + "Builder").c_str()).def(nb::init<>());
    [&]<typename... Ds, typename... Bs>(Fields<Ds...> /*record*/, Fields<Bs...> /*builder*/)
    {
        (cls.def(
             Bs::template prefixed<"set_">.data(),
             [](B& built, const C& context, const member_type_t<Ds::member>& value)
             { built.*Bs::member = built_form<member_type_t<Ds::member>, C>::from(value, context); },
             nb::arg("context"),
             nb::arg(Bs::c_name),
             nb::keep_alive<1, 2>()),  // an interned child refers to its context
         ...);
    }(data<T>::fields(), B::fields());
    bind_functions(cls);
    cls.def(
        "view",
        [](const B& built, const C& context) { return borrowed_view<T, C, B>(&built, &context); },
        nb::arg("context"),
        nb::keep_alive<0, 1>(),
        nb::keep_alive<0, 2>());
    return cls;
}

/// Binds the view View under the name of its tag (urd::name<T>(), T its value_type; <name>Builder before the rest for a
/// view of a builder<T, C>, <name>Borrowed for a view that borrows its record (a data<T> or a builder), <name>Owned for a view that owns its record,
/// <name>Shared for one that shares it, so that the views of one record bound in one module have their own names), the one way a view is bound, whether it reads a stored record
/// (data<T>), a record a context hands out in another layout (a value with the record's fields) or a builder
/// (builder<T, C>): get_<field>() for each field of its
/// record, compared and hashed by its identity, written with fmt (a view needs its fmt::formatter). A view
/// that names its index (index_view: index() gives its key, an urd::index<T> or a key of its own such as a row's) also
/// gives index() and converts to that key wherever one is expected, also in lists and optionals; the key is bound first
/// (T's index by bind_index<T>, another key by bind_value).
template<typename View>
    requires HasFields<record_of<View>>
nb::class_<View> bind_view(nb::module_& m)
{
    using T = typename View::value_type;
    const auto name = urd::name<T>() + (urd::is_builder<record_of<View>> ? "Builder" : "") + (detail::borrows_record<View> ? "Borrowed" : "")
                      + (detail::owns_record<View> ? "Owned" : "") + (detail::shares_record<View> ? "Shared" : "");
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
    if constexpr (urd::Indexed<View>)
    {
        using Handle = std::remove_cvref_t<decltype(std::declval<const View&>().index())>;
        view.def("index", &View::index);
        const auto index_type = nb::type<Handle>();
        if (!index_type.is_valid())
            throw std::logic_error("urd::python::bind_view: " + name + " names its index: bind its index first (bind_index<T>, or bind_value of its key).");
        nb::borrow<nb::class_<Handle>>(index_type).def("__init__", [](Handle* self, const View& v) { new (self) Handle(v.index()); });
        nb::implicitly_convertible<View, Handle>();
    }
    return view;
}

}  // namespace urd::python
