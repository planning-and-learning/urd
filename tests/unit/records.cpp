// What a record and its builder need not say: a record or a builder that names no identifying members is identified by
// all its fields; its cista members are its index and its fields (urd::cista_members_of); a field says what the record
// promises of it; a key of several parts is built as a child like an index. And a builder that holds a record's key
// only built (urd::built_child: its builder's handle, never null) is cleared, interned, packed and built from a record
// as one that holds a child.
#include "urd/elements/builder.hpp"
#include "urd/elements/children.hpp"
#include "urd/elements/clone.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/equal_to.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/view.hpp"
#include "urd/format.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/store.hpp"
#include "urd/tags.hpp"

#include <concepts>
#include <cstdint>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
struct Twig
{
    static constexpr std::string_view name = "Twig";
};

struct Bough
{
    static constexpr std::string_view name = "Bough";
};
struct Grid;
struct Cell;
struct Trees;
}  // namespace

/// A cell is numbered within its grid: its key is of two parts.
template<>
struct urd::key_of<Cell>
{
    using type = urd::key<Grid, Cell>;
};

template<>
struct urd::data<Twig>
{
    urd::key<Twig> index;
    int weight = 0;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"weight", &data::weight>> {}; }

    static constexpr auto cista_members() noexcept { return urd::cista_members_of<data>(); }
};

template<>
struct urd::data<Cell>
{
    urd::key<Grid, Cell> index;
    int value = 0;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"value", &data::value>> {}; }

    static constexpr auto cista_members() noexcept { return urd::cista_members_of<data>(); }
};

template<>
struct urd::data<Bough>
{
    urd::key<Bough> index;
    urd::index<Twig> tip;
    urd::flat_vector<urd::index<Twig>> twigs;
    urd::index<Twig> spare;
    int depth = 0;

    data() = default;
    data(const data& other) = delete;
    data& operator=(const data& other) = delete;
    data(data&& other) = default;
    data& operator=(data&& other) = default;
    ~data() = default;

    static constexpr auto fields() noexcept
    {
        return urd::Fields<urd::field<"tip", &data::tip>,
                           urd::field<"twigs", &data::twigs, urd::sorted, urd::unique>,
                           urd::field<"spare", &data::spare>,
                           urd::field<"depth", &data::depth>> {};
    }

    static constexpr auto cista_members() noexcept { return urd::cista_members_of<data>(); }
};

template<>
struct urd::builder<Twig, Trees>
{
    urd::key<Twig> index;
    urd::built_t<int, Trees> weight = 0;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"weight", &builder::weight>> {}; }
};

/// A bough's builder holds its tip and its twigs only built, its spare as a child.
template<>
struct urd::builder<Bough, Trees>
{
    urd::key<Bough> index;
    urd::built_only_t<urd::index<Twig>, Trees> tip = urd::make_owned<urd::builder<Twig, Trees>>();
    urd::built_only_t<urd::flat_vector<urd::index<Twig>>, Trees> twigs;
    urd::built_t<urd::index<Twig>, Trees> spare;
    urd::built_t<int, Trees> depth = 0;

    static constexpr auto fields() noexcept
    {
        return urd::Fields<urd::field<"tip", &builder::tip>,
                           urd::field<"twigs", &builder::twigs>,
                           urd::field<"spare", &builder::spare>,
                           urd::field<"depth", &builder::depth>> {};
    }
};

template<typename V>
struct urd::view_mixin<Twig, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] auto get_weight() const { return self().template member<&urd::record_of<V>::weight>(); }
};

namespace
{
struct Trees
{
    std::vector<urd::data<Twig>> twigs;

    const urd::data<Twig>& operator[](urd::index<Twig> at) const { return twigs[at.value]; }

    [[nodiscard]] urd::index_view<Twig, Trees> view(urd::index<Twig> at) const { return { at, this }; }

    [[nodiscard]] std::size_t layer() const noexcept { return 0; }

    /// Stores a twig of `weight`, its key.
    urd::index<Twig> add(int weight)
    {
        const auto at = urd::index<Twig>(static_cast<std::uint32_t>(twigs.size()));
        auto& twig = twigs.emplace_back();
        twig.index = at;
        twig.weight = weight;
        return at;
    }
};

using TwigBuilder = urd::builder<Twig, Trees>;
using BoughBuilder = urd::builder<Bough, Trees>;

/// Interns a twig in whichever form a bough's builder holds it; an interned one is Trees' already.
struct InternTwigs
{
    Trees* trees;

    urd::index<Twig> operator()(urd::index<Twig> twig) const { return twig; }

    urd::index<Twig> operator()(const TwigBuilder& twig) const { return trees->add(twig.weight); }

    urd::index<Twig> operator()(const urd::data<Twig>& twig) const { return trees->add(twig.weight); }

    urd::index<Bough> operator()(const urd::data<Bough>& /*bough*/) const { return urd::index<Bough>(0); }
};
}  // namespace

// A record and a builder that name no identifying members are identified by all their fields.
static_assert(!urd::DeclaresIdentity<urd::data<Twig>>);
static_assert(urd::Identifiable<urd::data<Twig>>);
static_assert(urd::Identifiable<BoughBuilder>);
static_assert(std::same_as<decltype(urd::identifying_members_of<urd::data<Twig>>()), urd::Members<&urd::data<Twig>::weight>>);
// Its cista members: its index, then its fields.
static_assert(std::same_as<decltype(urd::data<Twig>::cista_members()), urd::Members<&urd::data<Twig>::index, &urd::data<Twig>::weight>>);
// A field says what its record promises of it.
static_assert([]<typename Tip, typename Twigs, typename... Rest>(urd::Fields<Tip, Twigs, Rest...>)
              // NOLINTNEXTLINE(misc-redundant-expression): the two promises of Twigs differ in their template argument
              { return Twigs::template promises<urd::sorted> && Twigs::template promises<urd::unique> && !Tip::template promises<urd::sorted>; }(
                  urd::data<Bough>::fields()));
// A record's key of several parts is built as a child, as an index is; its interned view holds that key.
static_assert(std::same_as<urd::built_t<urd::key<Grid, Cell>, Trees>, urd::child<Cell, Trees, urd::key<Grid, Cell>>>);
static_assert(std::same_as<urd::built_t<urd::index<Twig>, Trees>, urd::child<Twig, Trees>>);
static_assert(std::same_as<urd::index_view<Cell, Trees>, urd::index_view<Cell, Trees, urd::key<Grid, Cell>>>);
// A builder with a field held only built is not made from its record alone (its context builds it).
static_assert(!urd::made_from_record<urd::data<Bough>, Trees>);
static_assert(urd::made_from_record<urd::data<Twig>, Trees>);
static_assert(std::same_as<urd::built_only_t<urd::index<Twig>, Trees>, urd::built_child<Twig, Trees>>);
static_assert(std::same_as<urd::built_only_t<urd::flat_vector<urd::index<Twig>>, Trees>, std::vector<urd::built_child<Twig, Trees>>>);
static_assert(std::same_as<urd::built_only_t<int, Trees>, int>);
static_assert(urd::BuiltChild<urd::built_child<Twig, Trees>> && !urd::Child<urd::built_child<Twig, Trees>> && urd::Child<urd::child<Twig, Trees>>);
static_assert(std::same_as<urd::kind_of_t<BoughBuilder>, Bough> && std::same_as<urd::builder_context_t<BoughBuilder>, Trees>);

TEST(Record, IsIdentifiedByAllItsFieldsWhereItNamesNone)
{
    auto light = urd::data<Twig> {};
    light.weight = 1;
    auto other = urd::data<Twig> {};
    other.weight = 1;
    other.index = urd::index<Twig>(7);  // its own key identifies no record
    auto heavy = urd::data<Twig> {};
    heavy.weight = 2;
    EXPECT_TRUE(urd::EqualTo<> {}(light, other));
    EXPECT_FALSE(urd::EqualTo<> {}(light, heavy));
    EXPECT_EQ(urd::Hash<> {}(light), urd::Hash<> {}(other));
}

TEST(BuiltChild, IsClearedInPlaceItsHandleKept)
{
    auto bough = BoughBuilder {};
    const auto* tip = bough.tip.get();
    bough.tip->weight = 5;
    bough.twigs.push_back(urd::make_owned<TwigBuilder>());
    bough.depth = 3;

    urd::clear_fields(bough);

    EXPECT_EQ(bough.tip.get(), tip);
    EXPECT_EQ(bough.tip->weight, 0);
    EXPECT_TRUE(bough.twigs.empty());
    EXPECT_EQ(bough.depth, 0);
}

TEST(BuiltChild, IsInternedAsAChildsBuilderIs)
{
    auto trees = Trees {};
    const auto stored = trees.add(9);
    auto bough = BoughBuilder {};
    bough.tip->weight = 5;
    bough.twigs.push_back(urd::make_owned<TwigBuilder>());
    bough.twigs.back()->weight = 7;
    bough.spare = stored;
    bough.depth = 2;

    auto record = urd::data<Bough> {};
    urd::intern(bough, record, InternTwigs { &trees });

    EXPECT_EQ(trees.twigs[record.tip.value].weight, 5);
    ASSERT_EQ(record.twigs.size(), 1U);
    EXPECT_EQ(trees.twigs[record.twigs[0].value].weight, 7);
    EXPECT_EQ(record.spare, stored);
    EXPECT_EQ(record.depth, 2);
}

TEST(BuiltChild, IsPackedAsItsBuildersIndex)
{
    auto bough = BoughBuilder {};
    bough.index = urd::index<Bough>(4);
    bough.tip->index = urd::index<Twig>(3);
    bough.twigs.push_back(urd::make_owned<TwigBuilder>());  // not interned: no index
    bough.spare = urd::index<Twig>(1);
    bough.depth = 6;

    const auto record = urd::pack(bough);

    EXPECT_EQ(record.index, urd::index<Bough>(4));
    EXPECT_EQ(record.tip, urd::index<Twig>(3));
    ASSERT_EQ(record.twigs.size(), 1U);
    EXPECT_TRUE(record.twigs[0].is_none());
    EXPECT_EQ(record.spare, urd::index<Twig>(1));
    EXPECT_EQ(record.depth, 6);

    // Through its view: the builder the view holds.
    auto trees = Trees {};
    const auto view = urd::borrowed_view<Bough, Trees, BoughBuilder> { &bough, &trees };
    EXPECT_EQ(&urd::record(view), &bough);
    EXPECT_EQ(urd::pack(view).tip, urd::index<Twig>(3));
}

TEST(BuiltChild, IsBuiltFromARecordsKeyAndFromAnotherBuilder)
{
    auto trees = Trees {};
    const auto five = trees.add(5);
    const auto seven = trees.add(7);
    auto record = urd::data<Bough> {};
    record.tip = seven;
    record.twigs.push_back(five);
    record.twigs.push_back(seven);
    record.spare = five;
    record.depth = 1;
    const auto source = urd::borrowed_view<Bough, Trees> { &record, &trees };

    auto bough = BoughBuilder {};
    const auto* tip = bough.tip.get();
    urd::build_from(source, bough, [](const auto& twig) { return twig; });

    EXPECT_EQ(bough.tip.get(), tip);  // filled in place
    EXPECT_EQ(bough.tip->weight, 7);
    ASSERT_EQ(bough.twigs.size(), 2U);
    EXPECT_EQ(bough.twigs[0]->weight, 5);
    EXPECT_EQ(bough.twigs[1]->weight, 7);
    EXPECT_EQ(std::get<urd::index<Twig>>(bough.spare), five);
    EXPECT_EQ(bough.depth, 1);

    // From another builder of the record, through its view.
    auto copy = BoughBuilder {};
    urd::build_from(urd::borrowed_view<Bough, Trees, BoughBuilder> { &bough, &trees }, copy);
    EXPECT_NE(copy.tip.get(), bough.tip.get());
    EXPECT_EQ(copy.tip->weight, 7);
    ASSERT_EQ(copy.twigs.size(), 2U);
    EXPECT_EQ(copy.twigs[1]->weight, 7);
    EXPECT_EQ(copy.depth, 1);
}

TEST(Child, HoldsTheBuilderItsMakerGivesWhereItHoldsNone)
{
    auto bough = BoughBuilder {};
    auto made = 0;
    const auto make = [&made]() -> urd::built_child<Twig, Trees>
    {
        ++made;
        return urd::make_owned<TwigBuilder>();
    };
    auto& first = urd::builder_of(bough.spare, make);
    first.weight = 4;
    auto& again = urd::builder_of(bough.spare, make);
    EXPECT_EQ(&again, &first);
    EXPECT_EQ(made, 1);
    EXPECT_EQ(again.weight, 4);
}

TEST(BuiltChild, IsWrittenAsTheBuilderItHolds)
{
    auto bough = BoughBuilder {};
    bough.tip->weight = 5;
    bough.depth = 2;
    const auto written = fmt::format("{}", bough);
    EXPECT_NE(written.find("tip=Twig("), std::string::npos) << written;
    EXPECT_NE(written.find("weight=5"), std::string::npos) << written;
    EXPECT_NE(written.find("depth=2"), std::string::npos) << written;
}

TEST(BuiltChild, IsTakenLeavingItsBuilderAsNew)
{
    auto bough = BoughBuilder {};
    bough.tip->weight = 5;
    bough.twigs.push_back(urd::make_owned<TwigBuilder>());
    bough.depth = 2;

    const auto taken = urd::take(bough);

    EXPECT_EQ(taken.tip->weight, 5);
    EXPECT_EQ(taken.twigs.size(), 1U);
    EXPECT_EQ(taken.depth, 2);
    ASSERT_NE(bough.tip, nullptr);  // never null: fit to be filled again
    EXPECT_EQ(bough.tip->weight, 0);
    EXPECT_TRUE(bough.twigs.empty());
    EXPECT_EQ(bough.depth, 0);
}
