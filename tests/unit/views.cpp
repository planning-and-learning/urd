// urd::index<T> and urd::index_view<T, C>: a strong index minted by the Store that numbers Ts, and a reference made of
// an index and the context that resolves it, which resolves the record's members through the same context.

#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/view.hpp"
#include "urd/format.hpp"
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
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <fmt/ranges.h>
#include <gtest/gtest.h>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{

using urd::index_view;
using urd::map;
using urd::pool;
using urd::sequence;
using urd::set;
using urd::slots;
using urd::Store;

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
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] std::string_view name() const { return self().template member<&Rec::name>(); }

    [[nodiscard]] std::size_t part(std::size_t at) const { return self().template member<&Rec::parts>().at(at).index().value; }

    static constexpr auto functions() noexcept
    { return urd::Functions<urd::function<"name", &view_mixin::name>, urd::function<"part", &view_mixin::part, urd::arg<"at">>> {}; }
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

    [[nodiscard]] urd::borrowed_view<Rec, Both, Rec> view(const Rec&& record) const = delete;
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
// NOLINTNEXTLINE(bugprone-sizeof-expression): sizeof needs the type complete, which is what is asserted
static_assert(sizeof(urd::resolved_range<std::vector<urd::index<Owner>>, Later>) > 0);
// A readout is a borrowed range (its iterators outlive it), random access where what it reads is.
static_assert(std::ranges::borrowed_range<urd::resolved_t<std::vector<urd::index<Owner>>, Both>>);
static_assert(std::ranges::borrowed_range<urd::transformed_range<std::vector<int>, int (*)(int)>>
              && std::ranges::random_access_range<urd::transformed_range<std::vector<int>, int (*)(int)>>
              && std::ranges::sized_range<urd::transformed_range<std::vector<int>, int (*)(int)>>);

// Compositions resolve at any depth: ranges of ranges, of optionals, of pairs; ranges of plain values stay as they are.
static_assert(std::same_as<std::ranges::range_value_t<std::ranges::range_value_t<urd::resolved_t<std::vector<std::vector<urd::index<Owner>>>, Both>>>,
                           index_view<Owner, Both>>);
static_assert(
    std::same_as<std::ranges::range_value_t<urd::resolved_t<std::vector<std::optional<urd::index<Owner>>>, Both>>, std::optional<index_view<Owner, Both>>>);
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
// A tag whose stored record is the slot urd::data<Tag> (Serializable), read through a context that holds the records.
// (A record that is not Serializable, which no context resolves: tests/rules/compile_fail_view.cpp.)
struct Item;
}  // namespace

template<>
struct urd::data<Item>
{
    urd::index<Item> index;
    int weight = 0;
    urd::index<Owner> owner;

    data() = default;

    data(int weight_, urd::index<Owner> owner_) : index(), weight(weight_), owner(owner_) {}

    static constexpr bool copyable() noexcept
    {
        static_assert(std::is_trivially_copyable_v<data>, "a copyable record is trivially copyable");
        return true;
    }

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"weight", &data::weight>, urd::field<"owner", &data::owner>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::weight, &data::owner> {}; }
};

template<typename V>
struct urd::view_mixin<Item, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] auto get_weight() const noexcept { return self().template member<&urd::record_of<V>::weight>(); }

    [[nodiscard]] auto get_owner() const noexcept { return self().template member<&urd::record_of<V>::owner>(); }
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

    /// An item held in place, as its view here (the context owning items).
    [[nodiscard]] urd::borrowed_view<Item, Items> view(const urd::data<Item>& item) const { return { &item, this }; }
};

static_assert(urd::stored_record<Item>);
static_assert(urd::resolves<Items, Item> && urd::resolves<Items, Owner>);
static_assert(std::same_as<urd::resolved_t<urd::data<Item>, Items>, urd::borrowed_view<Item, Items>>);

}  // namespace

TEST(View, ReadsTheStoredRecordOfItsTag)
{
    auto items = Items {};
    const auto owner = items.owners.index(items.owners.insert(Owner { "ada" }).first);
    items.records.emplace_back(3, owner);

    const auto item = items.view(urd::index<Item>(0));
    static_assert(std::same_as<decltype(urd::detail::view_access::record(item)), const urd::data<Item>&>);
    EXPECT_EQ(item.member<&urd::data<Item>::weight>(), 3);
    EXPECT_EQ(item.member<&urd::data<Item>::owner>().member<&Owner::name>(), "ada");
    const auto held = urd::resolve(items.records[0], items);
    EXPECT_EQ(held.member<&urd::data<Item>::weight>(), 3);
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
        names.push_back(owner.member<&Owner::name>());
        for (const auto member : members)
            names.push_back(member ? member->member<&Owner::name>() : std::string("-"));
    }
    EXPECT_EQ(names, (std::vector<std::string> { "a", "b", "-", "b" }));

    // A decoded row read through a temporary: the readout holds a copy of it.
    auto rows = urd::repr::UniformRows<urd::repr::BitCodec<urd::index<Owner>, false>, false>(urd::bit_width { 1 });
    rows.push_back(std::vector { b, a });
    const auto row = urd::resolve(rows[0], both);
    EXPECT_EQ(row[0].member<&Owner::name>(), "b");
    EXPECT_EQ(row[1].member<&Owner::name>(), "a");
}

TEST(View, BorrowsARecordNotInserted)
{
    const auto both = make_both();
    const auto record = Rec { "offered", { urd::index<Owner>(1), urd::index<Owner>(0) }, urd::index<Owner>(0), 3 };
    const auto offered = both.view(record);
    EXPECT_EQ(&urd::detail::view_access::record(offered), &record);
    EXPECT_EQ(offered.member<&Rec::owner>().member<&Owner::name>(), "a");
    auto names = std::vector<std::string_view>();
    for (const auto part : offered.member<&Rec::parts>())
        names.push_back(part.member<&Owner::name>());
    EXPECT_EQ(names, (std::vector<std::string_view> { "b", "a" }));
    // Equal records borrowed through the same context are equal; an inserted one is a view, of another type.
    const auto copy = record;  // NOLINT(performance-unnecessary-copy-initialization): an equal record at another address is under test
    using Borrowed = urd::borrowed_view<Rec, Both, Rec>;
    EXPECT_TRUE(urd::EqualTo<Borrowed> {}(offered, both.view(copy)));
    EXPECT_EQ(urd::Hash<Borrowed> {}(offered), urd::Hash<Borrowed> {}(both.view(copy)));
}

TEST(View, StoredInvertsResolve)
{
    const auto both = make_both();
    const auto a = urd::index<Owner>(0);
    const auto b = urd::index<Owner>(1);
    const auto groups = std::vector<std::pair<urd::index<Owner>, std::vector<std::optional<urd::index<Owner>>>>> {
        { a, { b, std::nullopt } },
        { b, {} },
    };
    // Each part written as the member's type stores it: views as indices, ranges as lists, pairs and optionals as cista's.
    using Groups = urd::flat_vector<urd::flat_pair<urd::index<Owner>, urd::flat_vector<urd::flat_optional<urd::index<Owner>>>>>;
    const auto stored = urd::stored<Groups>(urd::resolve(groups, both));
    ASSERT_EQ(stored.size(), 2U);
    EXPECT_EQ(stored[0].first, a);
    ASSERT_EQ(stored[0].second.size(), 2U);
    EXPECT_EQ(*stored[0].second[0], b);
    EXPECT_FALSE(stored[0].second[1].has_value());
    EXPECT_EQ(stored[1].first, b);
    EXPECT_TRUE(stored[1].second.empty());

    // A variant readout goes to the alternative of the member that takes it, whichever its position; a value as it is.
    const auto alternative = urd::resolve(std::variant<urd::index<Owner>, int>(b), both);
    EXPECT_EQ(cista::get<urd::index<Owner>>(urd::stored<urd::flat_variant<urd::index<Owner>, int>>(alternative)), b);
    EXPECT_EQ(cista::get<urd::index<Owner>>(urd::stored<urd::flat_variant<int, urd::index<Owner>>>(alternative)), b);
    EXPECT_EQ(cista::get<int>(urd::stored<urd::flat_variant<int, urd::index<Owner>>>(7)), 7);
    EXPECT_EQ(urd::stored<int>(7), 7);

    // One of the member's alternatives is that alternative, though another (a nested variant) would take it too; the
    // member's own variant keeps each alternative as it is.
    using Inner = urd::flat_variant<urd::index<Owner>, int>;
    using Outer = urd::flat_variant<urd::index<Owner>, Inner>;
    EXPECT_EQ(urd::stored<Outer>(b).index(), 0U);
    const auto inner = urd::stored<Outer>(Inner(b));
    ASSERT_EQ(inner.index(), 1U);
    EXPECT_EQ(cista::get<urd::index<Owner>>(cista::get<Inner>(inner)), b);
    EXPECT_EQ(urd::stored<Outer>(inner).index(), 1U);

    // store writes into a member as its type stores it.
    auto member = urd::flat_optional<urd::index<Owner>>();
    urd::store(member, both.view(b));
    EXPECT_EQ(*member, b);

    // append adds to a list as its element type stores it.
    auto list = urd::flat_vector<urd::index<Owner>>();
    urd::append(list, both.view(a));
    urd::append(list, b);
    ASSERT_EQ(list.size(), 2U);
    EXPECT_EQ(list[0], a);
    EXPECT_EQ(list[1], b);

    // What a member does not store does not compile: no conversion of values, no view of another element, no copy of a
    // record out of its view.
    static_assert(!urd::stores<double, int>);
    static_assert(!urd::stores<urd::index<Owner>, int>);
    static_assert(urd::stores<urd::index<Owner>, decltype(both.view(b))>);
    static_assert(!urd::stores<Rec, decltype(both.view(std::declval<const Rec&>()))>);
}

TEST(View, MakeBuildsARecordFromOneReadoutPerField)
{
    auto items = Items();
    const auto owner = items.owners.index(items.owners.insert(Owner { "o" }).first);
    const auto record = urd::make<Item>(3, items.view(owner));
    EXPECT_EQ(record.weight, 3);
    EXPECT_EQ(record.owner, owner);
}

TEST(View, ClearFieldsKeepsBuffers)
{
    struct Builder
    {
        urd::flat_vector<int> values;
        urd::index<Owner> owner;

        static constexpr auto fields() noexcept { return urd::Fields<urd::field<"values", &Builder::values>, urd::field<"owner", &Builder::owner>> {}; }
    };

    auto builder = Builder { { 1, 2, 3 }, urd::index<Owner>(4) };
    const auto* const buffer = builder.values.data();
    urd::clear_fields(builder);
    EXPECT_TRUE(builder.values.empty());
    EXPECT_EQ(builder.values.data(), buffer);
    EXPECT_TRUE(builder.owner.is_none());
}

/// A record's fields: their names in order, their members as its identifying members, each read through a view.
TEST(Fields, NameMembersInOrderReadThroughAView)
{
    using Data = urd::data<Item>;
    static_assert(std::same_as<decltype(Data::identifying_members()), urd::Members<&Data::weight, &Data::owner>>);
    auto items = Items {};
    const auto owner = items.owners.index(items.owners.insert(Owner { "ada" }).first);
    items.records.emplace_back(3, owner);
    const auto item = items.view(urd::index<Item>(0));

    auto names = std::vector<std::string_view>();
    urd::for_each_field<Data>([&]<typename F>() { names.push_back(F::name); });
    EXPECT_EQ(names, (std::vector<std::string_view> { "weight", "owner" }));
    using OwnerField = urd::field<"owner", &Data::owner>;
    EXPECT_EQ(item.member<OwnerField::member>().member<&Owner::name>(), "ada");
    EXPECT_STREQ(OwnerField::prefixed<"get_">.data(), "get_owner");
}

TEST(View, ResolvesMembersThroughItsContext)
{
    const auto both = make_both();
    const auto whole = index_view<Rec>(urd::index<Rec>(0), &both);  // Both resolves owners too
    EXPECT_EQ(whole.name(), "first");
    EXPECT_EQ(whole.part(1), both[urd::index<Rec>(0)].parts[1].value);
    EXPECT_EQ(whole.member<&Rec::owner>().member<&Owner::name>(), "b");
    auto names = std::vector<std::string_view>();
    for (const auto part : whole.member<&Rec::parts>())
        names.push_back(part.member<&Owner::name>());
    EXPECT_EQ(names, (std::vector<std::string_view> { "a", "b" }));
    EXPECT_EQ((std::get<index_view<Owner, Both>>(whole.member<&Rec::alternative>()).member<&Owner::name>()), "a");
    const auto second = index_view<Rec>(urd::index<Rec>(1), &both);
    EXPECT_EQ(std::get<int>(second.member<&Rec::alternative>()), 7);
    EXPECT_TRUE(second.member<&Rec::parts>().empty());
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
    const auto copy = record;  // NOLINT(performance-unnecessary-copy-initialization): an equal record at another address is under test
    EXPECT_EQ(both.view(record), both.view(copy));
    EXPECT_NE(both.view(record), other.view(copy));
    EXPECT_EQ(both.view(record) <=> other.view(copy), std::strong_ordering::less);
}

template<typename C>
concept views_a_temporary = requires(const C& context) { context.view(Rec {}); };

TEST(View, BorrowsAHeldRecord)
{
    // A record held outside the context (a form of T) is pointed at; a temporary one is not viewed. An index is
    // resolved through the context instead.
    static_assert(!views_a_temporary<Both>);
    const auto both = make_both();
    const auto record = Rec { "offered", {}, urd::index<Owner>(0), 3 };
    EXPECT_EQ(&urd::detail::view_access::record(both.view(record)), &record);
    const auto by_index = urd::index_view<Owner, Both>(urd::index<Owner>(1), &both);
    EXPECT_EQ(&by_index.member<&Owner::name>(), &both[urd::index<Owner>(1)].name);  // read in place, in its context
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
    EXPECT_EQ(shared.member<&Rec::owner>().member<&Owner::name>(), "a");
    const auto equal = Shared(std::make_shared<const Rec>(urd::detail::view_access::record(shared)), &both);
    EXPECT_NE(&urd::detail::view_access::record(equal), &urd::detail::view_access::record(shared));
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
    EXPECT_NE(&urd::detail::view_access::record(owning), &record);
    record.name = "changed";
    EXPECT_EQ(owning.name(), "computed");
    EXPECT_EQ(owning.member<&Rec::owner>().member<&Owner::name>(), "b");
    EXPECT_EQ(owning.member<&Rec::parts>()[0].member<&Owner::name>(), "b");
    EXPECT_EQ(owning, Owning(urd::detail::view_access::record(owning), &both));
    const auto other = make_both(urd::layer { 1 });
    EXPECT_NE(owning, Owning(urd::detail::view_access::record(owning), &other));
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

TEST(TransformedRange, TakesItsStateAsAContext)
{
    // A capturing lambda is neither default-constructible nor assignable, as iterators must be: the state it would
    // capture comes as a context instead, and the iterators, holding their own copy of it, outlive the range expression.
    const auto values = std::vector<int> { 1, 2, 3 };
    auto offset = 10;  // not a constant, so that a lambda would have to capture it
    const auto shifted = [&values, offset] { return urd::transform(values, offset, [](int by, int value) { return value + by; }); };
    static_assert(!std::semiregular<decltype([offset](int value) { return value + offset; })>);
    auto begin = shifted().begin();
    const auto end = shifted().end();
    EXPECT_EQ(end - begin, 3);
    EXPECT_EQ(begin[2], 13);
    begin = std::ranges::next(begin);
    EXPECT_EQ(*begin, 12);
    auto copy = decltype(begin) {};
    copy = begin;
    EXPECT_EQ(*copy, 12);
    EXPECT_TRUE(std::ranges::equal(shifted(), std::vector<int> { 11, 12, 13 }));
}

TEST(TransformedRange, EndsWhereWhatItReadsEnds)
{
    // A range with a sentinel ends at it.
    const auto squares = urd::transform(std::views::iota(1), [](int value) { return value * value; });
    static_assert(!std::ranges::common_range<decltype(squares)> && std::ranges::random_access_range<decltype(squares)>);
    EXPECT_NE(squares.begin(), squares.end());
    EXPECT_TRUE(std::ranges::equal(squares | std::views::take(3), std::vector<int> { 1, 4, 9 }));
}

namespace
{
// Records written with fmt as urd writes them (urd/format.hpp): a record of fields of each kind, and one inside it.
struct Inner
{
    static constexpr std::string_view name = "Inner";
};

struct Shown
{
    static constexpr std::string_view name = "Shown";
};

struct Empty
{
    static constexpr std::string_view name = "Empty";
};

template<typename A, typename B>
struct Twin
{
    static constexpr std::string_view name = urd::joined_name<"Twin", A::name, B::name>;
};
}  // namespace

template<>
struct urd::data<Inner>
{
    int value = 0;

    data() = default;

    explicit data(int value_) : value(value_) {}

    static constexpr bool copyable() noexcept
    {
        static_assert(std::is_trivially_copyable_v<data>, "a copyable record is trivially copyable");
        return true;
    }

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"value", &data::value>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::value> {}; }
};

template<>
struct urd::data<Shown>
{
    int count = 0;
    urd::flat_optional<int> limit;
    urd::flat_variant<int, bool> choice;
    urd::flat_vector<urd::flat_variant<int, bool>> choices;
    urd::data<Inner> inner;

    data() = default;

    data(int count_,
         urd::flat_optional<int> limit_,
         urd::flat_variant<int, bool> choice_,
         urd::flat_vector<urd::flat_variant<int, bool>> choices_,
         urd::data<Inner> inner_) :
        count(count_),
        limit(std::move(limit_)),
        choice(std::move(choice_)),
        choices(std::move(choices_)),
        inner(inner_)
    {
    }

    data(const data& other) = delete;
    data& operator=(const data& other) = delete;
    data(data&& other) = default;
    data& operator=(data&& other) = default;
    ~data() = default;

    static constexpr auto fields() noexcept
    {
        return urd::Fields<urd::field<"count", &data::count>,
                           urd::field<"limit", &data::limit>,
                           urd::field<"choice", &data::choice>,
                           urd::field<"choices", &data::choices>,
                           urd::field<"inner", &data::inner>> {};
    }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::count, &data::limit, &data::choice, &data::choices, &data::inner> {}; }
};

template<>
struct urd::data<Empty>
{
    data() = default;

    static constexpr bool copyable() noexcept
    {
        static_assert(std::is_trivially_copyable_v<data>, "a copyable record is trivially copyable");
        return true;
    }

    static constexpr auto fields() noexcept { return urd::Fields<> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<> {}; }
};

template<typename V>
struct urd::view_mixin<Inner, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] auto get_value() const noexcept { return self().template member<&urd::record_of<V>::value>(); }
};

template<typename V>
struct urd::view_mixin<Shown, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] auto get_count() const noexcept { return self().template member<&urd::record_of<V>::count>(); }

    [[nodiscard]] auto get_limit() const noexcept { return self().template member<&urd::record_of<V>::limit>(); }

    [[nodiscard]] auto get_choice() const noexcept { return self().template member<&urd::record_of<V>::choice>(); }

    [[nodiscard]] auto get_choices() const noexcept { return self().template member<&urd::record_of<V>::choices>(); }

    [[nodiscard]] auto get_inner() const noexcept { return self().template member<&urd::record_of<V>::inner>(); }
};

template<typename V>
struct urd::view_mixin<Empty, V>
{
private:
    [[maybe_unused]] const V& self() const noexcept { return static_cast<const V&>(*this); }
};

TEST(Format, WritesARecordFieldByField)
{
    auto shown = urd::data<Shown>();
    shown.count = 3;
    shown.choice = true;
    shown.choices.push_back(urd::flat_variant<int, bool>(1));
    shown.choices.push_back(urd::flat_variant<int, bool>(false));
    shown.inner.value = 7;
    EXPECT_EQ(fmt::format("{}", shown),
              "Shown(\n"
              "    count=3\n"
              "    limit=none\n"
              "    choice=true\n"
              "    choices=[\n"
              "        1\n"
              "        false\n"
              "    ]\n"
              "    inner=Inner(\n"
              "        value=7\n"
              "    )\n"
              ")");
    shown.limit = 2;
    EXPECT_NE(fmt::format("{}", shown).find("limit=2"), std::string::npos);
    EXPECT_EQ(fmt::format("{}", urd::data<Empty>()), "Empty()");
    // In a range, as itself: not quoted as a string.
    EXPECT_EQ(fmt::format("{}", std::array { urd::data<Empty>() }), "[Empty()]");
}

TEST(Format, BeginsALineAtTheIndentationOfItsScope)
{
    EXPECT_EQ(urd::new_line(), "\n");
    {
        const auto deeper = urd::scoped_indent();
        EXPECT_EQ(urd::new_line(), "\n    ");
        {
            const auto deepest = urd::scoped_indent();
            EXPECT_EQ(urd::new_line(), "\n        ");
        }
    }
    EXPECT_EQ(urd::new_line(), "\n");
}

TEST(Format, NamesATagAsItDeclares)
{
    EXPECT_EQ(urd::name<Shown>(), "Shown");
    EXPECT_EQ((urd::name<Twin<Inner, Twin<Shown, Empty>>>()), "TwinInnerTwinShownEmpty");
}

namespace
{
struct Keyed
{
};
}  // namespace

template<>
struct urd::data<Keyed>
{
    int number = 0;
    urd::flat_vector<int> values;

    data() = default;

    data(int number_, urd::flat_vector<int> values_) : number(number_), values(std::move(values_)) {}

    data(const data& other) = delete;
    data& operator=(const data& other) = delete;
    data(data&& other) = default;
    data& operator=(data&& other) = default;
    ~data() = default;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"number", &data::number>, urd::field<"values", &data::values>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::number, &data::values> {}; }
};

template<typename V>
struct urd::view_mixin<Keyed, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] decltype(auto) get_number() const { return self().template member<&urd::record_of<V>::number>(); }

    [[nodiscard]] decltype(auto) get_values() const { return self().template member<&urd::record_of<V>::values>(); }
};

namespace
{
// A context that keeps its records in a layout of its own: no data<Keyed> to hand out, each field read by the key.
struct ByKey
{
    std::array<int, 3> storage { 4, 5, 6 };

    template<auto Member>
    [[nodiscard]] auto field(urd::index<Keyed> /*at*/) const
    {
        if constexpr (std::same_as<decltype(Member), int urd::data<Keyed>::*>)
            return 2;
        else
            return std::span<const int>(storage);
    }

    [[nodiscard]] std::size_t layer() const noexcept { return 9; }
};
}  // namespace

TEST(View, AViewReadsByTheKeyWhereItsContextHandsOutNoRecord)
{
    const auto context = ByKey();
    const auto view = urd::index_view<Keyed, ByKey>(urd::index<Keyed>(0), &context);
    static_assert(urd::ReadsFieldsByKey<ByKey, urd::index<Keyed>, &urd::data<Keyed>::values> && !urd::HandsOutRecords<ByKey, urd::index<Keyed>>);
    static_assert(std::same_as<urd::record_of<decltype(view)>, urd::data<Keyed>>);
    // A field read by the key lives for the read only: what is read from it is a value, never a reference into it.
    static_assert(!std::is_reference_v<decltype(view.template member<&urd::data<Keyed>::values>())>);
    const auto values = view.template member<&urd::data<Keyed>::values>();
    ASSERT_EQ(values.size(), 3U);
    EXPECT_EQ(values[2], 6);
    EXPECT_EQ(view.template member<&urd::data<Keyed>::number>(), 2);
}
