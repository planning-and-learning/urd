#pragma once

// nanobind helpers for urd's elements: records and indices compare and hash as urd says, records are built from views,
// and a record is bound by bind_data, its builder by bind_builder, its views by bind_view.

#include "urd/elements/builder.hpp"
#include "urd/elements/children.hpp"
#include "urd/elements/clone.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/handle.hpp"
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
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// A builder is copied as urd copies it (urd::CopiedAsIs): one holding a child of its own is move-only, which a
/// std::vector of children does not tell (it reports itself copyable whatever its elements); nanobind would copy it.
template<typename T, typename C>
struct is_copy_constructible<urd::builder<T, C>> : std::bool_constant<urd::CopiedAsIs<urd::builder<T, C>>>
{
};
NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

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

/// The fields of a record or key (urd::Fields), each read and written under its name, and an __init__ from them (below).
template<typename Data>
    requires HasFields<Data>
void bind_fields(nb::class_<Data>& cls);

namespace detail
{
/// A class bound once, by whichever module binds it first (a tag one library defines and another stores records of:
/// ygg::formalism::Object); every other module names that class. Empty where none is bound yet.
template<typename T>
std::optional<nb::class_<T>> bound_already(nb::module_& m, const std::string& name)
{
    if (const auto bound = nb::type<T>(); bound.is_valid())
    {
        m.attr(name.c_str()) = bound;
        return nb::borrow<nb::class_<T>>(bound);
    }
    return std::nullopt;
}

template<typename Tag>
nb::class_<urd::key<Tag>> bind_index_of(nb::module_& m)
{
    using T = urd::key<Tag>;
    const auto name = urd::name<Tag>() + (NumberedKind<Tag> ? "Index" : "Key");
    if (auto bound = detail::bound_already<T>(m, name))
        return *bound;
    auto cls = nb::class_<T>(m, name.c_str());
    cls.def(nb::init<>()).def("is_none", &T::is_none);
    add_comparison(cls);
    add_hash(cls);
    if constexpr (NumberedKind<Tag>)
    {
        // urd::index's own names: the default is none, value is the integer, int() and index() give it too.
        cls.def(nb::init<typename T::value_type>(), nb::arg("index"))
            .def("__int__", [](const T& i) { return i.value; })
            .def("__index__", [](const T& i) { return i.value; })
            .def_ro("value", &T::value)
            .def_ro_static("none_value", &T::none_value)
            .def("__repr__", [name](const T& i) { return name + "(" + std::to_string(static_cast<typename T::value_type>(i.value)) + ")"; });
    }
    else
        // A key whose value is its store's handle (urd::key_value_of): written, as a key is, with fmt.
        cls.def("__repr__", [name](const T& i) { return name + fmt::format("{}", i); });
    return cls;
}

/// A key of several indices (urd::key<Ts...>) under its name (<Name>Key, after its last tag): an __init__ from its
/// parts in order (each an index of its tag), parts() as the tuple of them, compared, hashed and written with fmt.
template<typename... Ts>
nb::class_<urd::key<Ts...>> bind_key(nb::module_& m)
{
    using K = urd::key<Ts...>;
    const auto name = urd::name<typename K::element_type>() + "Key";
    if (auto bound = detail::bound_already<K>(m, name))
        return *bound;
    auto cls = nb::class_<K>(m, name.c_str())
                   .def(nb::init<>())
                   .def(nb::init<urd::index<Ts>...>())
                   .def("parts", [](const K& key) { return nb::make_tuple(key.template get<Ts>()...); })
                   .def("is_none", &K::is_none);
    add_comparison(cls);
    add_hash(cls);
    cls.def("__str__", [](const K& key) { return fmt::format("{}", key); }).def("__repr__", [name](const K& key) { return name + fmt::format("{}", key); });
    return cls;
}

/// The binding of a key type: an index of a tag, or a key of several.
template<typename T>
struct key_binder
{
    static auto bind(nb::module_& m) { return detail::bind_index_of<T>(m); }
};

template<typename T, typename... Ts>
struct key_binder<urd::key<T, Ts...>>
{
    static auto bind(nb::module_& m) { return detail::bind_key<T, Ts...>(m); }
};
}  // namespace detail

/// Binds a key, the one way a key is bound: an index, urd::key<Tag> of a tag, under <name>Index (urd::name<Tag>()); or
/// a key of several indices, urd::key<Ts...>, under <name>Key (its last tag's name), from its parts. A record's index is
/// bound before bind_data of the record; a key is bound once, by whichever module binds it first, every other names that
/// class.
template<typename T>
auto bind_index(nb::module_& m)
{ return detail::key_binder<T>::bind(m); }

/// The fields of record Data (urd::Fields), each a read/write attribute under its name, and an __init__ from their
/// values, in order.
template<typename Data>
    requires HasFields<Data>
void bind_fields(nb::class_<Data>& cls)
{
    // Read in place, written as a clone (urd::clone: a copy where it is copyable, field by field where it holds a
    // move-only record, a plan), so that a field of any type binds alike.
    for_each_field<Data>(
        [&]<typename F>()
        {
            cls.def_prop_rw(
                F::c_name,
                [](const Data& data) -> const member_type_t<F::member>& { return data.*F::member; },
                [](Data& data, const member_type_t<F::member>& value) { data.*F::member = urd::clone(value); },
                nb::rv_policy::reference_internal);
        });
    [&]<typename... Fs>(Fields<Fs...> /*fields*/)
    {
        cls.def("__init__", [](Data* self, const member_type_t<Fs::member>&... values) { new (self) Data(urd::clone(values)...); }, nb::arg(Fs::c_name)...);
    }(Data::fields());
}

/// A view's getters, get_<name> for each field of its record: the field resolved through the view's context.
template<typename View>
    requires HasFields<record_of<View>>
void bind_field_getters(nb::class_<View>& cls)
{
    // A field read in place (a reference into the view's record or context: a plan, a value of fields) is returned as
    // that reference, kept alive by the view (reference_internal), never copied: a move-only value has no copy; one
    // resolved (a view) is returned by value, moved.
    for_each_field<record_of<View>>(
        [&]<typename F>()
        {
            cls.def(
                F::template prefixed<"get_">.data(),
                [](const View& view) -> decltype(auto) { return view.template member<F::member>(); },
                nb::rv_policy::reference_internal);
        });
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
        if (!nb::type<urd::key<T>>().is_valid())
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

namespace detail
{
template<typename B>
bool from_python(nb::handle value, B& target);

/// The conversion from Python of a child of kind U read through C (its key K): it copies a builder or record of the
/// kind and walks its fields, and through its children the builders of their kinds. One source of a binding library
/// instantiates it for each kind of a context (URD_PYTHON_CONVERSIONS, urd/python/conversion_instantiations.hpp), which
/// its other sources declare: a source that binds a builder then compiles its own kind's conversion, not those of
/// every kind its children reach. A plain value has no builder.
template<typename U, typename C, typename K = key<U>>
struct conversions
{
    static bool to_child(nb::handle value, child<U, C, K>& target)
        requires(!PlainValue<U>);
};

/// A child from Python: a builder of its kind (built: a copy, owned by the child), a record of its kind (held: a copy),
/// or its key (interned: an index, or a view, which converts to its key).
template<typename U, typename C, typename K>
bool child_from_python(nb::handle value, child<U, C, K>& target)
{ return conversions<U, C, K>::to_child(value, target); }

template<typename U, typename C, typename K>
bool conversions<U, C, K>::to_child(nb::handle value, child<U, C, K>& target)
    requires(!PlainValue<U>)
{
    if (nb::isinstance<builder<U, C>>(value))
        target.template emplace<1>(urd::make_owned<builder<U, C>>(urd::clone(nb::cast<const builder<U, C>&>(value))));
    else if (nb::isinstance<data<U>>(value))
        target.template emplace<2>(urd::make_owned<data<U>>(urd::clone(nb::cast<const data<U>&>(value))));
    else if (auto key = K(); nb::try_cast(value, key))
        target.template emplace<0>(key);
    else
        return false;
    return true;
}

/// A builder held in place from Python: a builder of its kind (a copy), or a record of its kind written into it
/// (built_form::assign), where a record makes one (made_from_record).
template<typename U, typename C>
bool builder_from_python(nb::handle value, builder<U, C>& target)
{
    if (nb::isinstance<builder<U, C>>(value))
    {
        target = urd::clone(nb::cast<const builder<U, C>&>(value));
        return true;
    }
    if constexpr (made_from_record<data<U>, C>)
    {
        if (nb::isinstance<data<U>>(value))
        {
            built_form<data<U>, C>::assign(nb::cast<const data<U>&>(value), target);
            return true;
        }
    }
    return false;
}

template<typename B>
inline constexpr bool is_child_value = false;
template<typename U, typename C, typename K>
inline constexpr bool is_child_value<std::variant<K, handle<builder<U, C>>, handle<data<U>>>> = true;

template<typename B>
inline constexpr bool is_std_optional = false;
template<typename E>
inline constexpr bool is_std_optional<std::optional<E>> = true;

template<typename B>
inline constexpr bool is_std_variant = false;
template<typename... Bs>
inline constexpr bool is_std_variant<std::variant<Bs...>> = true;

/// Whether a built form holds a child or a builder, at any depth of its lists, optionals and variants: what nanobind
/// cannot cast whole (std::vector of a move-only element still says it is copy constructible).
template<typename B>
inline constexpr bool holds_children = is_child_value<B> || is_builder<B>;
template<typename E, typename A>
inline constexpr bool holds_children<std::vector<E, A>> = holds_children<E>;
template<typename E>
inline constexpr bool holds_children<std::optional<E>> = holds_children<E>;
template<typename... Bs>
inline constexpr bool holds_children<std::variant<Bs...>> = is_child_value<std::variant<Bs...>> || (holds_children<Bs> || ...);
template<typename F, typename S>
inline constexpr bool holds_children<std::pair<F, S>> = holds_children<F> || holds_children<S>;

template<typename B>
inline constexpr bool is_std_pair = false;
template<typename F, typename S>
inline constexpr bool is_std_pair<std::pair<F, S>> = true;

/// A std::vector of what holds children: built element by element; one of values nanobind casts whole.
template<typename B>
inline constexpr bool is_vector_of_children = false;
template<typename E, typename A>
inline constexpr bool is_vector_of_children<std::vector<E, A>> = holds_children<E>;

/// The first alternative of a variant that takes the value.
template<std::size_t I, typename... Bs>
bool alternative_from_python(nb::handle value, std::variant<Bs...>& target)
{
    if constexpr (I == sizeof...(Bs))
        return false;
    else
    {
        auto alternative = std::variant_alternative_t<I, std::variant<Bs...>>();
        if (detail::from_python(value, alternative))
        {
            target.template emplace<I>(std::move(alternative));
            return true;
        }
        return detail::alternative_from_python<I + 1>(value, target);
    }
}

/// A builder's member from Python, as its built form takes it (urd::built_form): a child (child_from_python), a list
/// of members element by element, a pair of them member by member, an optional (None is empty), a variant by the first alternative that takes it, a
/// builder held in place as a builder or its record (a copy), anything else as nanobind casts it. Whether it took it.
template<typename B>
bool from_python(nb::handle value, B& target)
{
    if constexpr (is_child_value<B>)
        return detail::child_from_python(value, target);
    else if constexpr (is_std_variant<B>)
        return detail::alternative_from_python<0>(value, target);
    else if constexpr (is_std_optional<B>)
    {
        if (value.is_none())
        {
            target.reset();
            return true;
        }
        return detail::from_python(value, target.emplace());
    }
    else if constexpr (is_std_pair<B> && holds_children<B>)
    {
        if (!nb::isinstance<nb::sequence>(value) || nb::isinstance<nb::str>(value) || nb::len(value) != 2)
            return false;
        return detail::from_python(value[0], target.first) && detail::from_python(value[1], target.second);
    }
    else if constexpr (is_vector_of_children<B>)
    {
        if (!nb::isinstance<nb::sequence>(value) || nb::isinstance<nb::str>(value))
            return false;
        auto result = B();
        for (const auto element : value)
            if (!detail::from_python(element, result.emplace_back()))
                return false;
        target = std::move(result);
        return true;
    }
    else if constexpr (is_builder<B>)
        return detail::builder_from_python(value, target);
    else if constexpr (std::copy_constructible<B>)
        return nb::try_cast(value, target);
    else
        return false;  // a form with no Python value (a relation's ordered rows): its builder's mutators make it
}
}  // namespace detail

/// Binds the builder<T, C> under <name>Builder (urd::name<T>()), the one way a builder is bound: an __init__ that takes
/// each field by its name, every one optional (empty where it is not given: its default is `...`, so that an explicit
/// None is a value, an optional's empty one, and nothing else's); set_<field>(value) for each field; the
/// mutators its functions() lists, under their names; and view(context), its view through a context
/// (<name>BuilderBorrowed), which every read goes through, as a record's do. A field takes what its built form holds,
/// none of it read through a context: a child a key (an index, or a view of one), a builder or a record of its kind (a
/// copy, owned by the child); a list, an optional or a variant of these, element by element; a value as nanobind casts
/// it. Bound after bind_data<T>.
template<typename T, typename C>
    requires HasFields<builder<T, C>>
nb::class_<builder<T, C>> bind_builder(nb::module_& m)
{
    using B = builder<T, C>;
    auto cls = nb::class_<B>(m, (urd::name<T>() + "Builder").c_str());
    const auto set = []<typename F>(B& built, nb::handle value)
    {
        if (!detail::from_python(value, built.*F::member))
            throw nb::type_error((std::string("urd::python::bind_builder: ") + F::c_name + " does not take this value").c_str());
    };
    [&]<typename... Fs>(Fields<Fs...> /*fields*/)
    {
        cls.def(
            "__init__",
            [set](B* self, std::conditional_t<true, nb::handle, Fs>... values)
            {
                new (self) B();
                // `...`, Python's "unspecified", is a field not given (it stays empty); any other value, None too, is set.
                ((values.is(nb::ellipsis()) ? void() : set.template operator()<Fs>(*self, values)), ...);
            },
            (nb::arg(Fs::c_name) = nb::ellipsis())...);
        (cls.def(
             Fs::template prefixed<"set_">.data(),
             [set](B& built, nb::handle value) { set.template operator()<Fs>(built, value); },
             nb::arg(Fs::c_name)),
         ...);
    }(B::fields());
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
/// <name>Shared for one that shares it, so that the views of one record bound in one module have their own names), the one way a view is bound, whether it
/// reads a stored record (data<T>), a record a context hands out in another layout (a value with the record's fields) or a builder (builder<T, C>):
/// get_<field>() for each field of its record, compared and hashed by its identity, written with fmt (a view needs its fmt::formatter). A view that names its
/// index (index_view: index() gives its key, an urd::key<T> or a key of its own such as a row's) also gives index() and converts to that key wherever one is
/// expected, also in lists and optionals; the key is bound first (T's index by bind_index<T>, a key that is a value by
/// bind_data of its tag).
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
            throw std::logic_error("urd::python::bind_view: " + name
                                   + " names its index: bind its index first (bind_index<T>, or bind_data of its key's tag).");
        nb::borrow<nb::class_<Handle>>(index_type).def("__init__", [](Handle* self, const View& v) { new (self) Handle(v.index()); });
        nb::implicitly_convertible<View, Handle>();
    }
    return view;
}

}  // namespace urd::python
