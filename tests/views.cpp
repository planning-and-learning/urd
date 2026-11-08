// urd::index<T> and urd::index_view<T, C>: a strong index minted by the Store that numbers Ts, and a reference made of
// an index and the context that resolves it, which resolves the record's members through the same context.

#include "urd/elements/flat.hpp"
#include "urd/elements/view.hpp"
#include "urd/repr/containers/bit_rows.hpp"
#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/pools/pools.hpp"
#include "urd/repr/pools/tree_bitset.hpp"
#include "urd/repr/pools/tree_vector.hpp"
#include "urd/repr/records/arena.hpp"
#include "urd/repr/records/records.hpp"
#include "urd/repr/sequences/jagged.hpp"
#include "urd/repr/sequences/row_pool.hpp"
#include "urd/repr/sequences/segmented.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/repr/wrappers/layer.hpp"
#include "urd/repr/wrappers/layered_pool.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace
{

using urd::map;
using urd::pool;
using urd::sequence;
using urd::set;
using urd::slots;
using urd::Store;
using urd::index_view;

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

using Owners = Store<set<Owner>, urd::positional, urd::monotone>;
using Recs = Store<set<Rec>, urd::positional, urd::monotone>;

}  // namespace

// A named accessor: what a library writes once per record type, before its views are made.
template<typename V>
struct urd::view_mixin<Rec, V>
{
    [[nodiscard]] std::string_view name() const { return static_cast<const V&>(*this).template get<&Rec::name>(); }
    [[nodiscard]] std::size_t part(std::size_t at) const { return static_cast<const V&>(*this).data().parts.at(at).value; }
};

namespace
{

/// Two interners as one context: it resolves both record types, numbered as its caller says.
struct Both
{
    explicit Both(urd::layer number) : owners(number), recs(number) {}

    Owners owners;
    Recs recs;

    [[nodiscard]] const Owner& operator[](urd::index<Owner> at) const { return owners[at]; }
    [[nodiscard]] const Rec& operator[](urd::index<Rec> at) const { return recs[at]; }
    [[nodiscard]] std::size_t layer() const { return recs.layer(); }
    [[nodiscard]] urd::index_view<Owner, Both> view(urd::index<Owner> at) const { return { at, this }; }
    [[nodiscard]] urd::index_view<Rec, Both> view(urd::index<Rec> at) const { return { at, this }; }
    [[nodiscard]] urd::borrowed_view<Rec, Both, Rec> view(const Rec& record) const { return { &record, this }; }
    urd::borrowed_view<Rec, Both, Rec> view(const Rec&& record) const = delete;
};

}  // namespace

// The default context of Rec.
template<>
struct urd::context_of<Rec>
{
    using type = Both;
};
static_assert(std::same_as<index_view<Rec>, index_view<Rec, Both>>);

namespace
{

// The index: a strong type per element type, none by default, a dense key.
static_assert(!std::same_as<urd::index<Owner>, urd::index<Rec>> && !std::convertible_to<urd::index<Owner>, urd::index<Rec>>
              && !std::convertible_to<urd::index<Owner>, std::uint32_t>);
static_assert(urd::indexable<urd::index<Owner>> && urd::hashable<urd::index<Owner>> && urd::orderable<urd::index<Owner>>);
static_assert(!urd::index_of<urd::index<Owner>>::is_index(urd::index<Owner>()) && urd::index_of<urd::index<Owner>>::to_index(urd::index<Owner>(3)) == 3);
static_assert(std::same_as<Store<map<urd::index<Owner>, int>, urd::dense>::representation, urd::repr::IndexMap<urd::index<Owner>, int, false>>);
static_assert(std::same_as<Store<set<urd::index<Owner>>, urd::dense>::representation, urd::repr::DenseSet<urd::index<Owner>>>);

// Handles are indices: a pool's and an interner's.
static_assert(std::same_as<Store<pool<Store<sequence<int>, urd::frozen>>>::handle_type, urd::index<Store<sequence<int>, urd::frozen>>>);
static_assert(std::same_as<Store<slots<int>>::handle_type, urd::index<int>>);
static_assert(std::same_as<Owners::index_type, urd::index<Owner>>);

// Contexts: a positional set resolves its own type; Both resolves two; a hash set resolves nothing.
static_assert(urd::resolves<Owners, Owner> && !urd::resolves<Owners, Rec> && urd::resolves<Both, Owner> && urd::resolves<Both, Rec>);
static_assert(!urd::resolves<Store<set<Owner>>, Owner>);

// What the members resolve to, in each context.
static_assert(std::same_as<urd::resolved_t<urd::index<Owner>, Both>, index_view<Owner, Both>>);
static_assert(std::same_as<urd::resolved_t<std::string, Both>, std::string>);  // text as it is
static_assert(std::ranges::random_access_range<urd::resolved_t<std::vector<urd::index<Owner>>, Both>>
              && std::same_as<std::ranges::range_value_t<urd::resolved_t<std::vector<urd::index<Owner>>, Both>>, index_view<Owner, Both>>);
static_assert(std::same_as<urd::resolved_t<std::variant<urd::index<Owner>, int>, Both>, std::variant<index_view<Owner, Both>, int>>);
// A range's readout is a named type: it can be spelled with a context that is not complete yet.
struct Later;
static_assert(sizeof(urd::resolved_range<std::vector<urd::index<Owner>>, Later>) > 0);
// A readout is a borrowed range (its iterators outlive it), random access where what it reads is.
static_assert(std::ranges::borrowed_range<urd::resolved_t<std::vector<urd::index<Owner>>, Both>>);
static_assert(std::ranges::borrowed_range<urd::transformed_range<std::vector<int>, int (*)(int)>>
              && std::ranges::random_access_range<urd::transformed_range<std::vector<int>, int (*)(int)>>
              && std::ranges::sized_range<urd::transformed_range<std::vector<int>, int (*)(int)>>);

// Compositions resolve at any depth: ranges of ranges, of optionals, of pairs; ranges of plain values stay as they are.
static_assert(std::same_as<std::ranges::range_value_t<std::ranges::range_value_t<urd::resolved_t<std::vector<std::vector<urd::index<Owner>>>, Both>>>,
                           index_view<Owner, Both>>);
static_assert(std::same_as<std::ranges::range_value_t<urd::resolved_t<std::vector<std::optional<urd::index<Owner>>>, Both>>,
                           std::optional<index_view<Owner, Both>>>);
static_assert(std::same_as<urd::resolved_t<std::pair<urd::index<Owner>, std::vector<urd::index<Rec>>>, Both>::first_type, index_view<Owner, Both>>);
static_assert(std::same_as<urd::resolved_t<std::vector<int>, Both>, std::vector<int>>);
static_assert(urd::Hashable<urd::resolved_t<std::vector<urd::index<Owner>>, Both>> && urd::orderable<urd::resolved_t<std::vector<urd::index<Owner>>, Both>>);

// Views are hashable and comparable, by index and layer number.
static_assert(urd::hashable<index_view<Rec, Both>> && urd::hashable<index_view<Owner, Owners>> && urd::orderable<index_view<Rec, Both>>);
// A context without a number gives its views no identity.
struct Unnumbered
{
};
static_assert(!urd::Identifiable<index_view<Rec, Unnumbered>> && !urd::Identifiable<urd::borrowed_view<Rec, Unnumbered, Rec>>);

}  // namespace

TEST(Index, APositionalSetMintsIndices)
{
    auto owners = Owners(urd::layer { 0 });
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
    auto counts = Store<map<urd::index<Owner>, int>, urd::dense>();
    counts.insert_or_assign(urd::index<Owner>(2), 5);
    EXPECT_TRUE(counts.contains(urd::index<Owner>(2)));
    EXPECT_FALSE(counts.contains(urd::index<Owner>(1)));
    EXPECT_FALSE(counts.contains(urd::index<Owner>()));  // none is no index
    EXPECT_EQ(counts.at(urd::index<Owner>(2)), 5);
}

namespace
{

Both make_both(urd::layer number = urd::layer { 0 })
{
    auto both = Both(number);
    const auto a = both.owners.index(both.owners.insert(Owner { "a" }).first);
    const auto b = both.owners.index(both.owners.insert(Owner { "b" }).first);
    both.recs.insert(Rec { "first", { a, b }, b, a });
    both.recs.insert(Rec { "second", {}, a, 7 });
    return both;
}

}  // namespace

namespace
{
// A tag whose stored record is the slot urd::data<Tag> (Serializable), read through a context that holds the records;
// and a tag whose record is not Serializable, which no context resolves.
struct Item;
struct Bare;
}  // namespace

template<>
struct urd::data<Item>
{
    int weight = 0;
    urd::index<Owner> owner;
    static constexpr auto cista_members() noexcept { return urd::Members<&data::weight, &data::owner> {}; }
    static constexpr auto identifying_members() noexcept { return urd::Members<&data::weight, &data::owner> {}; }
};
template<>
struct urd::data<Bare>
{
    int weight = 0;
};

namespace
{
struct Items
{
    std::vector<urd::data<Item>> records;
    Owners owners { urd::layer { 4 } };

    const urd::data<Item>& operator[](urd::index<Item> at) const { return records[at.value]; }
    const Owner& operator[](urd::index<Owner> at) const { return owners[at]; }
    [[nodiscard]] std::size_t layer() const noexcept { return 5; }
    [[nodiscard]] urd::index_view<Item, Items> view(urd::index<Item> at) const { return { at, this }; }
    [[nodiscard]] urd::index_view<Owner, Items> view(urd::index<Owner> at) const { return { at, this }; }
};
struct Bares
{
    std::vector<urd::data<Bare>> records;
    const urd::data<Bare>& operator[](urd::index<Bare> at) const { return records[at.value]; }
    [[nodiscard]] std::size_t layer() const noexcept { return 6; }
    [[nodiscard]] urd::index_view<Bare, Bares> view(urd::index<Bare> at) const { return { at, this }; }
};
static_assert(urd::stored_record<Item> && !urd::stored_record<Bare>);
static_assert(urd::resolves<Items, Item> && urd::resolves<Items, Owner> && !urd::resolves<Bares, Bare>);
static_assert(std::same_as<urd::resolved_t<urd::data<Item>, Items>, urd::borrowed_view<Item, Items>>);

}  // namespace

TEST(View, ReadsTheStoredRecordOfItsTag)
{
    auto items = Items {};
    const auto owner = items.owners.index(items.owners.insert(Owner { "ada" }).first);
    items.records.push_back(urd::data<Item> { .weight = 3, .owner = owner });

    const auto item = items.view(urd::index<Item>(0));
    static_assert(std::same_as<decltype(item.data()), const urd::data<Item>&>);
    EXPECT_EQ(item.data().weight, 3);
    EXPECT_EQ(item.get<&urd::data<Item>::owner>().data().name, "ada");
    const auto held = urd::resolve(items.records[0], items);
    EXPECT_EQ(held.get<&urd::data<Item>::weight>(), 3);
    EXPECT_EQ(held.context().layer(), 5U);
}

TEST(View, ResolvesCompositionsAtAnyDepth)
{
    const auto both = make_both();
    const auto a = urd::index<Owner>(0);
    const auto b = urd::index<Owner>(1);
    const auto groups = std::vector<std::pair<urd::index<Owner>, std::vector<std::optional<urd::index<Owner>>>>> {
        { a, { b, std::nullopt } },
        { b, {} },
    };
    auto names = std::vector<std::string>();
    for (const auto [owner, members] : urd::resolve(groups, both))
    {
        names.push_back(owner.get<&Owner::name>());
        for (const auto member : members)
            names.push_back(member ? member->get<&Owner::name>() : std::string("-"));
    }
    EXPECT_EQ(names, (std::vector<std::string> { "a", "b", "-", "b" }));

    // A decoded row read through a temporary: the readout holds a copy of it.
    auto rows = urd::repr::BitRows<urd::index<Owner>, false>(urd::bit_width { 1 });
    rows.push_back(std::vector { b, a });
    const auto row = urd::resolve(rows[0], both);
    EXPECT_EQ(row[0].get<&Owner::name>(), "b");
    EXPECT_EQ(row[1].get<&Owner::name>(), "a");
}

TEST(View, BorrowsARecordNotInserted)
{
    const auto both = make_both();
    const auto record = Rec { "offered", { urd::index<Owner>(1), urd::index<Owner>(0) }, urd::index<Owner>(0), 3 };
    const auto offered = both.view(record);
    EXPECT_EQ(&offered.data(), &record);
    EXPECT_EQ(offered.get<&Rec::owner>().get<&Owner::name>(), "a");
    auto names = std::vector<std::string_view>();
    for (const auto part : offered.get<&Rec::parts>())
        names.push_back(part.get<&Owner::name>());
    EXPECT_EQ(names, (std::vector<std::string_view> { "b", "a" }));
    // Equal records borrowed through the same context are equal; an inserted one is a view, of another type.
    const auto copy = record;
    using Borrowed = urd::borrowed_view<Rec, Both, Rec>;
    EXPECT_TRUE(urd::EqualTo<Borrowed> {}(offered, both.view(copy)));
    EXPECT_EQ(urd::Hash<Borrowed> {}(offered), urd::Hash<Borrowed> {}(both.view(copy)));
}

TEST(View, ToStoredInvertsResolve)
{
    const auto both = make_both();
    const auto a = urd::index<Owner>(0);
    const auto b = urd::index<Owner>(1);
    const auto groups = std::vector<std::pair<urd::index<Owner>, std::vector<std::optional<urd::index<Owner>>>>> {
        { a, { b, std::nullopt } },
        { b, {} },
    };
    // Each part read back as its flat counterpart: views as indices, ranges as flat vectors, pairs and optionals as cista's.
    const auto stored = urd::to_stored(urd::resolve(groups, both));
    static_assert(std::same_as<std::remove_const_t<decltype(stored)>,
                               urd::flat_vector<urd::flat_pair<urd::index<Owner>, urd::flat_vector<urd::flat_optional<urd::index<Owner>>>>>>);
    ASSERT_EQ(stored.size(), 2U);
    EXPECT_EQ(stored[0].first, a);
    ASSERT_EQ(stored[0].second.size(), 2U);
    EXPECT_EQ(*stored[0].second[0], b);
    EXPECT_FALSE(stored[0].second[1].has_value());
    EXPECT_EQ(stored[1].first, b);
    EXPECT_TRUE(stored[1].second.empty());

    // A variant as the flat variant of its alternatives' stored forms; a value as it is.
    const auto alternative = urd::resolve(std::variant<urd::index<Owner>, int>(b), both);
    const auto variant = urd::to_stored(alternative);
    static_assert(std::same_as<std::remove_const_t<decltype(variant)>, urd::flat_variant<urd::index<Owner>, int>>);
    EXPECT_EQ(cista::get<urd::index<Owner>>(variant), b);
    EXPECT_EQ(urd::to_stored(7), 7);
    static_assert(std::same_as<urd::stored_t<index_view<Owner, Both>>, urd::index<Owner>>);

    // A borrowed record as a copy of it.
    const auto record = Rec { "offered", { a }, b, 3 };
    const auto copy = urd::to_stored(both.view(record));
    static_assert(std::same_as<std::remove_const_t<decltype(copy)>, Rec>);
    EXPECT_EQ(copy.name, "offered");
    EXPECT_EQ(copy.owner, b);
}

TEST(View, ClearMembersKeepsBuffers)
{
    struct Builder
    {
        urd::flat_vector<int> values;
        urd::index<Owner> owner;
        static constexpr auto cista_members() noexcept { return urd::Members<&Builder::values, &Builder::owner> {}; }
    };
    auto builder = Builder { { 1, 2, 3 }, urd::index<Owner>(4) };
    const auto* const buffer = builder.values.data();
    urd::clear_members(builder);
    EXPECT_TRUE(builder.values.empty());
    EXPECT_EQ(builder.values.data(), buffer);
    EXPECT_TRUE(builder.owner.is_none());
}

TEST(View, ResolvesMembersThroughItsContext)
{
    const auto both = make_both();
    const auto whole = index_view<Rec>(urd::index<Rec>(0), &both);  // Both resolves owners too
    EXPECT_EQ(whole.name(), "first");
    EXPECT_EQ(whole.part(1), whole.data().parts[1].value);
    EXPECT_EQ(whole.get<&Rec::owner>().get<&Owner::name>(), "b");
    auto names = std::vector<std::string_view>();
    for (const auto part : whole.get<&Rec::parts>())
        names.push_back(part.get<&Owner::name>());
    EXPECT_EQ(names, (std::vector<std::string_view> { "a", "b" }));
    EXPECT_EQ((std::get<index_view<Owner, Both>>(whole.get<&Rec::alternative>()).get<&Owner::name>()), "a");
    const auto second = index_view<Rec>(urd::index<Rec>(1), &both);
    EXPECT_EQ(std::get<int>(second.get<&Rec::alternative>()), 7);
    EXPECT_TRUE(second.get<&Rec::parts>().empty());
}

TEST(View, ComparesAndHashesByIndexAndLayer)
{
    const auto both = make_both();
    const auto again = index_view<Rec>(urd::index<Rec>(0), &both);
    const auto equal = urd::EqualTo<index_view<Rec>> {};
    EXPECT_TRUE(equal(index_view<Rec>(urd::index<Rec>(0), &both), again));
    EXPECT_FALSE(equal(index_view<Rec>(urd::index<Rec>(1), &both), again));
    EXPECT_EQ(urd::Hash<index_view<Rec>> {}(index_view<Rec>(urd::index<Rec>(0), &both)), urd::Hash<index_view<Rec>> {}(again));
    const auto other = make_both(urd::layer { 1 });
    EXPECT_FALSE(equal(index_view<Rec>(urd::index<Rec>(0), &other), again));  // another layer: another element
    EXPECT_TRUE(urd::Less<index_view<Rec>> {}(again, index_view<Rec>(urd::index<Rec>(1), &both)));

    auto seen = Store<set<index_view<Rec>>>();
    seen.insert(again);
    EXPECT_TRUE(seen.contains(index_view<Rec>(urd::index<Rec>(0), &both)));
    EXPECT_FALSE(seen.contains(index_view<Rec>(urd::index<Rec>(1), &both)));
}

TEST(View, ComparesWithItsOperators)
{
    const auto both = make_both();
    const auto first = index_view<Rec>(urd::index<Rec>(0), &both);
    const auto second = index_view<Rec>(urd::index<Rec>(1), &both);
    EXPECT_EQ(first, index_view<Rec>(urd::index<Rec>(0), &both));
    EXPECT_NE(first, second);
    EXPECT_LT(first, second);
    EXPECT_EQ(first <=> second, std::strong_ordering::less);
    const auto other = make_both(urd::layer { 1 });
    EXPECT_NE(first, index_view<Rec>(urd::index<Rec>(0), &other));

    // Borrowed views compare by the record and the layer.
    const auto record = Rec { "offered", {}, urd::index<Owner>(0), 3 };
    const auto copy = record;
    EXPECT_EQ(both.view(record), both.view(copy));
    EXPECT_NE(both.view(record), other.view(copy));
    EXPECT_EQ(both.view(record) <=> other.view(copy), std::strong_ordering::less);
}

// A handle names what it locates.
struct PairHandle
{
    using located = Owner;
    urd::index<Owner> first;
    std::uint32_t second;
};
static_assert(urd::handle<urd::index<Owner>> && urd::handle<PairHandle> && !urd::handle<int> && !urd::handle<Rec>);

template<typename C>
concept views_a_temporary = requires(const C& context) { context.view(Rec {}); };

TEST(View, BorrowsAHeldRecord)
{
    // A record held outside the context (a form of T) is pointed at; a temporary one is not viewed. A handle (an
    // index) is resolved through the context instead.
    static_assert(!views_a_temporary<Both>);
    const auto both = make_both();
    const auto record = Rec { "offered", {}, urd::index<Owner>(0), 3 };
    EXPECT_EQ(&both.view(record).data(), &record);
    const auto by_handle = urd::index_view<Owner, Both>(urd::index<Owner>(1), &both);
    EXPECT_EQ(&by_handle.data(), &both[urd::index<Owner>(1)]);
}

namespace
{
template<typename V>
concept names_index = requires(const V& v) { v.index(); };
}  // namespace

TEST(View, SharesARecord)
{
    // A record the view points at through a std::shared_ptr lives as long as its views; views of equal records are
    // equal. It names no index: only an index_view does.
    using Shared = urd::shared_view<Rec, Both, Rec>;
    static_assert(!names_index<Shared> && !names_index<urd::borrowed_view<Rec, Both, Rec>> && !names_index<urd::owning_view<Rec, Both, Rec>>
                  && names_index<index_view<Rec>>);
    static_assert(std::same_as<decltype(std::declval<index_view<Rec>>().index()), urd::index<Rec>>);
    const auto both = make_both();
    auto record = std::make_shared<Rec>(Rec { "shared", {}, urd::index<Owner>(0), 3 });
    const auto weak = std::weak_ptr<Rec>(record);
    const auto shared = Shared(record, &both);
    record.reset();
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(shared.name(), "shared");
    EXPECT_EQ(shared.get<&Rec::owner>().get<&Owner::name>(), "a");
    const auto equal = Shared(std::make_shared<const Rec>(shared.data()), &both);
    EXPECT_NE(&equal.data(), &shared.data());
    EXPECT_EQ(equal, shared);
}

TEST(View, OwnsACopyOfARecord)
{
    // A record computed rather than stored is held by value: the view reads its own copy, and compares by the record
    // and the layer.
    using Owning = urd::owning_view<Rec, Both, Rec>;
    const auto both = make_both();
    auto record = Rec { "computed", { urd::index<Owner>(1) }, urd::index<Owner>(1), 3 };
    const auto owning = Owning(record, &both);
    EXPECT_NE(&owning.data(), &record);
    record.name = "changed";
    EXPECT_EQ(owning.name(), "computed");
    EXPECT_EQ(owning.get<&Rec::owner>().get<&Owner::name>(), "b");
    EXPECT_EQ(owning.get<&Rec::parts>()[0].get<&Owner::name>(), "b");
    EXPECT_EQ(owning, Owning(owning.data(), &both));
    const auto other = make_both(urd::layer { 1 });
    EXPECT_NE(owning, Owning(owning.data(), &other));
}

TEST(TransformedRange, ItsIteratorsOutliveIt)
{
    // Unlike std::views::transform, the iterators hold the function (here a function object holding a value): they
    // outlive the range they came from.
    struct Shift
    {
        int offset = 0;
        [[nodiscard]] int operator()(int value) const { return value + offset; }
    };
    const auto values = std::vector<int> { 1, 2, 3 };
    const auto shifted = [&values] { return urd::transform(values, Shift { 10 }); };
    const auto begin = shifted().begin();
    const auto end = shifted().end();
    EXPECT_EQ(end - begin, 3);
    EXPECT_EQ(begin[2], 13);
    EXPECT_EQ(*std::ranges::next(begin), 12);
    const auto found = std::ranges::find(shifted(), 12);
    static_assert(!std::same_as<decltype(found), const std::ranges::dangling>);
    EXPECT_EQ(found - begin, 1);
    EXPECT_TRUE(std::ranges::equal(shifted(), std::vector<int> { 11, 12, 13 }));
    EXPECT_EQ(shifted().size(), 3U);
    EXPECT_EQ(shifted().at(1), 12);
    EXPECT_THROW((void) shifted().at(3), std::out_of_range);
}

TEST(TransformedRange, EndsWhereWhatItReadsEnds)
{
    // A range with a sentinel ends at it.
    const auto squares = urd::transform(std::views::iota(1), [](int value) { return value * value; });
    static_assert(!std::ranges::common_range<decltype(squares)> && std::ranges::random_access_range<decltype(squares)>);
    EXPECT_NE(squares.begin(), squares.end());
    EXPECT_TRUE(std::ranges::equal(squares | std::views::take(3), std::vector<int> { 1, 4, 9 }));
}
