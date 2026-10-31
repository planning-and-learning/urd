// urd::index<T> and urd::view<T, C>: a strong index minted by the Store that numbers Ts, and a reference made of
// an index and the context that resolves it, which resolves the record's members through the same context.

#include "urd/flat.hpp"
#include "urd/repr/arena.hpp"
#include "urd/repr/bidirectional_csr.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/csr_multimap.hpp"
#include "urd/repr/dense_multimap.hpp"
#include "urd/repr/dense_set.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/index_map.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/jagged.hpp"
#include "urd/repr/layer.hpp"
#include "urd/repr/layered_pool.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/pools.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/records.hpp"
#include "urd/repr/row_pool.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/slots.hpp"
#include "urd/repr/sparse_set.hpp"
#include "urd/repr/tree_bitset.hpp"
#include "urd/repr/tree_vector.hpp"
#include "urd/repr/unordered_flat_map.hpp"
#include "urd/repr/unordered_flat_set.hpp"
#include "urd/store.hpp"
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
