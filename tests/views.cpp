// urd::index<T> and urd::view<T, C>: a strong index minted by the Store that numbers Ts, and a reference made of
// an index and the context that resolves it, which resolves the record's members through the same context.

#include "urd/compose/family.hpp"
#include "urd/flat.hpp"
#include "urd/store.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/layers.hpp"
#include "urd/use/pools.hpp"
#include "urd/use/records.hpp"
#include "urd/use/sequences.hpp"
#include "urd/view.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace
{

using urd::key;
using urd::Store;
using urd::value;
using urd::view;

struct Owner
{
    std::string name;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Owner::name> {}; }
};
struct Rec
{
    std::string name;
    std::vector<urd::index<Owner>> parts;
    urd::index<Owner> owner;
    std::variant<urd::index<Owner>, int> alternative;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Rec::name, &Rec::parts, &Rec::owner, &Rec::alternative> {}; }
};

using Owners = Store<key<Owner>, urd::positional, urd::monotone>;
using Recs = Store<key<Rec>, urd::positional, urd::monotone>;

/// Two interners as one context: it resolves both record types, and numbers itself as a layer would.
struct Both
{
    Owners owners;
    Recs recs;
    std::size_t number = 0;

    [[nodiscard]] const Owner& operator[](urd::index<Owner> at) const { return owners[at]; }
    [[nodiscard]] const Rec& operator[](urd::index<Rec> at) const { return recs[at]; }
    [[nodiscard]] std::size_t layer() const noexcept { return number; }
};

}  // namespace

// The default context of Rec, and a named accessor: what a library writes once per record type.
template<>
struct urd::context_of<Rec>
{
    using type = Both;
};
template<typename V>
struct urd::view_mixin<Rec, V>
{
    [[nodiscard]] std::string_view name() const { return static_cast<const V&>(*this).template get<&Rec::name>(); }
};
static_assert(std::same_as<view<Rec>, view<Rec, Both>>);

namespace
{

// The index: a strong type per element type, none by default, a dense key.
static_assert(!std::same_as<urd::index<Owner>, urd::index<Rec>> && !std::convertible_to<urd::index<Owner>, urd::index<Rec>>
              && !std::convertible_to<urd::index<Owner>, std::uint32_t>);
static_assert(urd::indexable<urd::index<Owner>> && urd::hashable<urd::index<Owner>> && urd::orderable<urd::index<Owner>>);
static_assert(!urd::index_of<urd::index<Owner>>::is_index(urd::index<Owner>()) && urd::index_of<urd::index<Owner>>::to_index(urd::index<Owner>(3)) == 3);
static_assert(std::same_as<Store<key<urd::index<Owner>>, value<int>, urd::dense>::representation, urd::repr::IndexMap<urd::index<Owner>, int, false>>);
static_assert(std::same_as<Store<key<urd::index<Owner>>, urd::dense>::representation, urd::repr::DenseSet<urd::index<Owner>>>);

// Handles are indices: a pool's and an interner's.
static_assert(std::same_as<Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique>::handle_type, urd::index<Store<value<int>, urd::frozen>>>);
static_assert(std::same_as<Store<value<int>, urd::handles>::handle_type, urd::index<int>>);
static_assert(std::same_as<Owners::index_type, urd::index<Owner>>);

// Contexts: a positional set resolves its own type; Both resolves two; a hash set resolves nothing.
static_assert(urd::resolves<Owners, Owner> && !urd::resolves<Owners, Rec> && urd::resolves<Both, Owner> && urd::resolves<Both, Rec>);
static_assert(!urd::resolves<Store<key<Owner>>, Owner>);

// What the members resolve to, in each context.
static_assert(std::same_as<urd::resolved_t<urd::index<Owner>, Recs>, urd::index<Owner>>);
static_assert(std::same_as<urd::resolved_t<urd::index<Owner>, Both>, view<Owner, Both>>);
static_assert(std::same_as<urd::resolved_t<std::string, Both>, std::string_view>);
static_assert(std::same_as<urd::resolved_t<std::vector<urd::index<Owner>>, Recs>, std::span<const urd::index<Owner>>>);
static_assert(std::ranges::random_access_range<urd::resolved_t<std::vector<urd::index<Owner>>, Both>>
              && std::same_as<std::ranges::range_value_t<urd::resolved_t<std::vector<urd::index<Owner>>, Both>>, view<Owner, Both>>);
static_assert(std::same_as<urd::resolved_t<std::variant<urd::index<Owner>, int>, Both>, std::variant<view<Owner, Both>, int>>);
static_assert(std::same_as<urd::resolved_t<std::variant<urd::index<Owner>, int>, Recs>, std::variant<urd::index<Owner>, int>>);

// Views are hashable and comparable, by index and layer number.
static_assert(urd::hashable<view<Rec, Both>> && urd::hashable<view<Owner, Owners>> && urd::orderable<view<Rec, Both>>);

}  // namespace

TEST(Index, APositionalSetMintsIndices)
{
    auto owners = Owners();
    const auto [it, inserted] = owners.insert(Owner { "a" });
    EXPECT_TRUE(inserted);
    const auto a = owners.index(it);
    EXPECT_EQ(a, urd::index<Owner>(0));
    const auto b = owners.index(owners.insert(Owner { "b" }).first);
    EXPECT_EQ(b.value, 1U);
    EXPECT_EQ(owners[a].name, "a");
    EXPECT_EQ(owners.at(b).name, "b");
    EXPECT_THROW((void) owners.at(urd::index<Owner>()), std::out_of_range);
    EXPECT_EQ(owners.index(owners.insert(Owner { "a" }).first), a);
}

TEST(Index, KeysDenseTables)
{
    auto counts = Store<key<urd::index<Owner>>, value<int>, urd::dense>();
    counts.insert_or_assign(urd::index<Owner>(2), 5);
    EXPECT_TRUE(counts.contains(urd::index<Owner>(2)));
    EXPECT_FALSE(counts.contains(urd::index<Owner>(1)));
    EXPECT_FALSE(counts.contains(urd::index<Owner>()));  // none is no index
    EXPECT_EQ(counts.at(urd::index<Owner>(2)), 5);
}

namespace
{

Both make_both()
{
    auto both = Both();
    const auto a = both.owners.index(both.owners.insert(Owner { "a" }).first);
    const auto b = both.owners.index(both.owners.insert(Owner { "b" }).first);
    both.recs.insert(Rec { "first", { a, b }, b, a });
    both.recs.insert(Rec { "second", {}, a, 7 });
    return both;
}

}  // namespace

TEST(View, ResolvesMembersThroughItsContext)
{
    const auto both = make_both();
    const auto first = both.recs.view(urd::index<Rec>(0));  // the Store alone resolves only Rec
    EXPECT_EQ(first.get<&Rec::name>(), "first");
    EXPECT_EQ(first.get<&Rec::owner>(), urd::index<Owner>(1));
    EXPECT_EQ(first.get<&Rec::parts>().size(), 2U);
    EXPECT_EQ(std::get<urd::index<Owner>>(first.get<&Rec::alternative>()), urd::index<Owner>(0));

    const auto whole = view<Rec>(urd::index<Rec>(0), both);  // Both resolves owners too
    EXPECT_EQ(whole.name(), "first");
    EXPECT_EQ(whole.get<&Rec::owner>().get<&Owner::name>(), "b");
    auto names = std::vector<std::string_view>();
    for (const auto part : whole.get<&Rec::parts>())
        names.push_back(part.get<&Owner::name>());
    EXPECT_EQ(names, (std::vector<std::string_view> { "a", "b" }));
    EXPECT_EQ((std::get<view<Owner, Both>>(whole.get<&Rec::alternative>()).get<&Owner::name>()), "a");
    const auto second = view<Rec>(urd::index<Rec>(1), both);
    EXPECT_EQ(std::get<int>(second.get<&Rec::alternative>()), 7);
    EXPECT_TRUE(second.get<&Rec::parts>().empty());
}

TEST(View, ComparesAndHashesByIndexAndLayer)
{
    const auto both = make_both();
    const auto again = view<Rec>(urd::index<Rec>(0), both);
    EXPECT_EQ(view<Rec>(urd::index<Rec>(0), both), again);
    EXPECT_NE(view<Rec>(urd::index<Rec>(1), both), again);
    EXPECT_EQ(urd::Hash<view<Rec>> {}(view<Rec>(urd::index<Rec>(0), both)), urd::Hash<view<Rec>> {}(again));
    auto other = make_both();
    other.number = 1;
    EXPECT_NE(view<Rec>(urd::index<Rec>(0), other), again);  // another layer: another element
    EXPECT_TRUE(urd::Less<view<Rec>> {}(again, view<Rec>(urd::index<Rec>(1), both)));

    auto seen = Store<key<view<Rec>>>();
    seen.insert(again);
    EXPECT_TRUE(seen.contains(view<Rec>(urd::index<Rec>(0), both)));
    EXPECT_FALSE(seen.contains(view<Rec>(urd::index<Rec>(1), both)));
}

// A repository: a family of layered pools of flat records, one per record type, as the formalism of a planner keeps
// its symbols. Each phase branches a family from the previous one.
namespace formalism
{

struct Object
{
    urd::flat_string name;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Object::name> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Object::name> {}; }
};
struct Predicate
{
    urd::flat_string name;
    std::uint32_t arity = 0;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Predicate::name, &Predicate::arity> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Predicate::name, &Predicate::arity> {}; }
};
struct Parameter
{
    std::uint32_t position = 0;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Parameter::position> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Parameter::position> {}; }
};
struct Term
{
    urd::flat_variant<urd::index<Object>, Parameter> value;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Term::value> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Term::value> {}; }
};
struct Atom
{
    urd::index<Predicate> predicate;
    urd::flat_vector<urd::index<Term>> terms;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Atom::predicate, &Atom::terms> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Atom::predicate, &Atom::terms> {}; }
};

template<typename T>
using Symbols = Store<value<T>, urd::handles, urd::unique, urd::flat, urd::monotone, urd::layered>;
using Repository = urd::compose::Family<Symbols, Object, Predicate, Parameter, Term, Atom>;

Object object(std::string_view name)
{
    auto result = Object();
    result.name = urd::flat_string(name);
    return result;
}

}  // namespace formalism

template<>
struct urd::context_of<formalism::Atom>
{
    using type = formalism::Repository;
};
template<typename V>
struct urd::view_mixin<formalism::Atom, V>
{
    [[nodiscard]] auto predicate() const { return static_cast<const V&>(*this).template get<&formalism::Atom::predicate>(); }
    [[nodiscard]] auto terms() const { return static_cast<const V&>(*this).template get<&formalism::Atom::terms>(); }
};

static_assert(std::same_as<formalism::Symbols<formalism::Atom>::representation, urd::repr::LayeredPool<urd::repr::ArenaInterner<formalism::Atom>>>);
static_assert(std::same_as<formalism::Symbols<formalism::Parameter>::representation, urd::repr::LayeredPool<urd::repr::RecordInterner<formalism::Parameter>>>);
static_assert(urd::resolves<formalism::Repository, formalism::Atom> && urd::resolves<formalism::Repository, formalism::Object>
              && urd::resolves<formalism::Repository, formalism::Term>);

TEST(Family, LayersItsMembersAndResolvesEveryType)
{
    using namespace formalism;
    auto domain = Repository();
    const auto block = domain.insert(object("block")).first;
    const auto on = domain.insert(Predicate { urd::flat_string(std::string_view("on")), 2 }).first;
    auto task = Repository(&domain);
    EXPECT_EQ(task.layer(), 1U);
    EXPECT_EQ(task.parent(), &domain);
    EXPECT_EQ(task.insert(object("block")).first, block);  // the domain's object, found through the chain
    const auto table = task.insert(object("table")).first;
    EXPECT_EQ(table, urd::index<Object>(1));
    EXPECT_EQ(task.find(object("table")), table);
    EXPECT_TRUE(task.find(object("floor")).is_none());
    EXPECT_TRUE(task.contains(block) && task.contains(table) && !task.contains(urd::index<Object>(2)));

    auto term = Term();
    term.value = block;
    const auto block_term = task.insert(term).first;
    term.value = Parameter { 0 };
    const auto parameter_term = task.insert(term).first;
    auto atom = Atom();
    atom.predicate = on;
    atom.terms.push_back(block_term);
    atom.terms.push_back(parameter_term);
    const auto on_block = task.insert(atom).first;

    const auto seen = task.view(on_block);  // view<Atom> through the task
    EXPECT_EQ(seen.predicate().get<&Predicate::name>(), "on");
    EXPECT_EQ(seen.predicate().context().layer(), 0U);  // the domain owns the predicate
    EXPECT_EQ(seen.context().layer(), 1U);              // the task owns the atom
    auto names = std::vector<std::string>();
    for (const auto each : seen.terms())
    {
        const auto resolved = each.get<&Term::value>();
        if (std::holds_alternative<urd::view<Object, Repository>>(resolved))
            names.emplace_back(std::get<urd::view<Object, Repository>>(resolved).get<&Object::name>());
        else
            names.push_back("?" + std::to_string(std::get<Parameter>(resolved).position));
    }
    EXPECT_EQ(names, (std::vector<std::string> { "block", "?0" }));

    // Views of a domain record are the same element, asked of the domain or the task; a third layer reads through.
    EXPECT_EQ(task.view(block), domain.view(block));
    auto plan = Repository(&task);
    EXPECT_EQ(plan.layer(), 2U);
    EXPECT_EQ(plan.view(on_block).predicate().get<&Predicate::name>(), "on");
    EXPECT_EQ(plan[table].name, urd::flat_string(std::string_view("table")));
    EXPECT_EQ(plan.insert(object("table")).first, table);
    // The factory insert on a family: a record that holds the index it is given.
    auto given = urd::index<Parameter>();
    const auto [first_parameter, inserted] = plan.insert(Parameter { 7 },
                                                         [&given](urd::index<Parameter> at)
                                                         {
                                                             given = at;
                                                             return Parameter { 7 };
                                                         });
    EXPECT_TRUE(inserted);
    EXPECT_EQ(given, first_parameter);
    EXPECT_EQ(first_parameter, urd::index<Parameter>(0));  // no parameter record was interned in the domain or the task
    EXPECT_EQ(plan.get<Parameter>().size(), 1U);
}
