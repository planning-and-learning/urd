// A module of urd's binding helpers (urd/python/bindings.hpp), for tests/integration/python/bindings.py: an index bound
// with bind_index, a record bound with bind_data, and its views bound with bind_view: a stored term's (index_view) and a
// term builder's (borrowed_view of builder<Term>), read through a small context.

#include "urd/bits.hpp"
#include "urd/elements/builder.hpp"
#include "urd/elements/children.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/view.hpp"
#include "urd/python/bindings.hpp"
#include "urd/python/containers/bitset_span.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <fmt/format.h>
#include <nanobind/nanobind.h>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

struct Atom
{
    static constexpr std::string_view name = "Atom";
};

struct Term
{
    static constexpr std::string_view name = "Term";
};

/// A tag of tags: its name is composed of theirs, as it declares (urd::joined_name).
template<typename A, typename B>
struct Pair
{
    static constexpr std::string_view name = urd::joined_name<"Pair", A::name, B::name>;
};

struct Context;

}  // namespace

template<>
struct urd::data<Term>
{
    urd::index<Term> index;
    int arity = 0;
    urd::flat_vector<int> objects;
    urd::flat_optional<int> weight;

    data() = default;

    data(int arity_, urd::flat_vector<int> objects_, urd::flat_optional<int> weight_) :
        index(),
        arity(arity_),
        objects(std::move(objects_)),
        weight(std::move(weight_))
    {
    }

    data(const data& other) = delete;
    data& operator=(const data& other) = delete;
    data(data&& other) = default;
    data& operator=(data&& other) = default;
    ~data() = default;

    static constexpr auto fields() noexcept
    { return urd::Fields<urd::field<"arity", &data::arity>, urd::field<"objects", &data::objects>, urd::field<"weight", &data::weight>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::arity, &data::objects, &data::weight> {}; }
};

/// A term being built: the fields of data<Term> under the same names, so its view reads as a term's.
template<>
struct urd::builder<Term, Context>
{
    urd::index<Term> index;
    urd::built_t<int, Context> arity = 0;
    urd::built_t<urd::flat_vector<int>, Context> objects;
    urd::built_t<urd::flat_optional<int>, Context> weight;

    static constexpr auto fields() noexcept
    { return urd::Fields<urd::field<"arity", &builder::arity>, urd::field<"objects", &builder::objects>, urd::field<"weight", &builder::weight>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    /// Adds an object (a mutator, bound by bind_builder as its functions() lists it).
    void add_object(int object) { objects.push_back(object); }

    static constexpr auto functions() noexcept { return urd::Functions<urd::function<"add_object", &builder::add_object, urd::arg<"object">>> {}; }
};

/// The block a term's flags read (get_flags): it outlives every call.
inline constexpr std::uint64_t term_flag_block = 1;

/// A term's accessors, for its stored record and its builder alike (record_of<V>: whichever it reads).
template<typename V>
struct urd::view_mixin<Term, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] decltype(auto) get_arity() const { return self().template member<&urd::record_of<V>::arity>(); }

    [[nodiscard]] decltype(auto) get_objects() const { return self().template member<&urd::record_of<V>::objects>(); }

    [[nodiscard]] decltype(auto) get_weight() const { return self().template member<&urd::record_of<V>::weight>(); }

    // Derived: whether it has no objects; and, with an argument, whether it has more objects than a count.
    [[nodiscard]] bool is_empty() const { return get_objects().empty(); }

    [[nodiscard]] bool has_more_objects_than(std::size_t count) const { return get_objects().size() > count; }

    [[nodiscard]] auto arity_plus_one() const { return get_arity() + 1; }

    // A bitset_span (urd/python/containers/bitset_span.hpp): two bits over a block that outlives the call, the first set.
    [[nodiscard]] urd::bitset_span<const std::uint64_t> get_flags() const noexcept { return { &term_flag_block, 2 }; }

    static constexpr auto functions() noexcept
    {
        return urd::Functions<urd::function<"is_empty", &view_mixin::is_empty>,
                              urd::function<"has_more_objects_than", &view_mixin::has_more_objects_than, urd::arg<"count">>,
                              urd::function<"arity_plus_one", &view_mixin::arity_plus_one>,
                              urd::function<"get_flags", &view_mixin::get_flags>> {};
    }
};

namespace
{
/// A value: two fields, a record no Store holds (data<Point>, no index), bound with bind_data.
struct Point
{
    static constexpr std::string_view name = "Point";
};
}  // namespace

template<>
struct urd::data<Point>
{
    int x = 0;
    int y = 0;

    data() = default;

    data(int x_, int y_) : x(x_), y(y_) {}

    static constexpr bool copyable() noexcept
    {
        static_assert(std::is_trivially_copyable_v<data>, "a copyable record is trivially copyable");
        return true;
    }

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"x", &data::x>, urd::field<"y", &data::y>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::x, &data::y> {}; }
};

namespace
{

/// The context of the module's terms: each held once, under its position.
struct Context
{
    std::vector<urd::data<Term>> terms;

    // Its records are move-only, so it is (std::vector reports itself copyable whatever it holds; nanobind would copy it).
    Context() = default;
    Context(const Context& other) = delete;
    Context& operator=(const Context& other) = delete;
    Context(Context&& other) = default;
    Context& operator=(Context&& other) = default;
    ~Context() = default;

    [[nodiscard]] const urd::data<Term>& operator[](urd::index<Term> at) const { return terms[at.value]; }

    [[nodiscard]] urd::index_view<Term, Context> view(urd::index<Term> at) const { return { at, this }; }

    [[nodiscard]] std::size_t layer() const noexcept { return 0; }
};

Context& context()
{
    static auto held = []
    {
        auto made = Context();
        auto objects = urd::flat_vector<int>();
        objects.push_back(4);
        objects.push_back(5);
        made.terms.emplace_back(2, std::move(objects), urd::flat_optional<int>());
        made.terms.back().index = urd::index<Term>(0);
        return made;
    }();
    return held;
}

urd::builder<Term, Context>& term_builder()
{
    static auto held = []
    {
        auto made = urd::builder<Term, Context>();
        made.arity = 1;
        made.objects.push_back(9);
        return made;
    }();
    return held;
}

namespace nb = nanobind;

using TermView = urd::index_view<Term, Context>;
using TermBuilderView = urd::borrowed_view<Term, Context, urd::builder<Term, Context>>;
using TermOwnedView = urd::owning_view<Term, Context>;

}  // namespace

NB_MODULE(urd_bindings_test, m)
{
    [[maybe_unused]] const auto context_class = nb::class_<Context>(m, "Context");
    urd::python::bind_index<Atom>(m);
    urd::python::bind_data<Point>(m);
    urd::python::bind_index<Term>(m);
    urd::python::bind_data<Term>(m);
    urd::python::bind_view<TermView>(m);
    urd::python::bind_builder<Term, Context>(m);
    urd::python::bind_view<TermBuilderView>(m);
    urd::python::bind_view<TermOwnedView>(m);
    m.def("pair_name", [] { return urd::name<Pair<Term, Pair<Atom, Term>>>(); });
    m.def("term", [](std::size_t at) { return context().view(urd::index<Term>(static_cast<urd::index<Term>::value_type>(at))); });
    m.def("term_builder", [] { return TermBuilderView(&term_builder(), &context()); });
    m.def("context", []() -> const Context& { return context(); }, nb::rv_policy::reference);
    m.def("term_owned",
          []
          {
              auto record = urd::data<Term>();
              record.arity = 3;
              return TermOwnedView(std::move(record), &context());
          });
}
