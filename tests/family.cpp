// urd::utils::Family: one Store per type, the context through which views of records holding indices of several types
// resolve; layered where its members layer.

#include "urd/repr/pools/pools.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/wrappers/layered_pool.hpp"
#include "urd/store.hpp"
#include "urd/utils/family.hpp"

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct Owner
{
    std::string name;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Owner::name> {}; }
};
struct Rec
{
    std::string name;
    std::vector<urd::index<Owner>> parts;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Rec::name, &Rec::parts> {}; }
};

template<typename T>
using Interned = urd::Store<urd::set<T>, urd::positional, urd::monotone>;
using Records = urd::utils::Family<Interned, Owner, Rec>;

// A family resolves every member's type; views of a record resolve its indices through it.
static_assert(urd::resolves<Records, Owner> && urd::resolves<Records, Rec>);

using Row = urd::Store<urd::sequence<int>, urd::frozen>;
template<typename T>
using Layered = urd::Store<urd::pool<T>, urd::monotone, urd::layered>;
using Rows = urd::utils::Family<Layered, Row>;

}  // namespace

TEST(Family, InsertsFindsAndResolvesViewsByType)
{
    auto family = Records();
    const auto [a, inserted] = family.insert(Owner { "a" });
    EXPECT_TRUE(inserted);
    const auto b = family.insert(Owner { "b" }).first;
    EXPECT_EQ(family.insert(Owner { "a" }).first, a);
    const auto rec = family.insert(Rec { "r", { b, a } }).first;
    EXPECT_EQ(family.find(Owner { "b" }), b);
    EXPECT_TRUE(family.find(Owner { "c" }).is_none());
    EXPECT_TRUE(family.contains(rec));
    EXPECT_EQ(family[a].name, "a");

    const auto view = family.view(rec);
    auto names = std::vector<std::string_view>();
    for (const auto part : view.get<&Rec::parts>())
        names.push_back(part.get<&Owner::name>());
    EXPECT_EQ(names, (std::vector<std::string_view> { "b", "a" }));

    auto sizes = std::vector<std::size_t>();
    family.for_each([&](const auto& member) { sizes.push_back(member.size()); });
    EXPECT_EQ(sizes, (std::vector<std::size_t> { 2, 1 }));
}

TEST(Family, LayersEachMemberOverItsParents)
{
    auto root = Rows(nullptr);
    const auto first = root.insert(Row { 1, 2 }).first;
    auto child = Rows(&root);
    EXPECT_EQ(child.layer(), 1U);
    EXPECT_EQ(child.parent(), &root);
    EXPECT_EQ(child.insert(Row { 1, 2 }).first, first);  // found in the parent
    const auto second = child.insert(Row { 3 }).first;
    EXPECT_NE(second, first);
    EXPECT_TRUE(root.find(Row { 3 }).is_none());
    EXPECT_EQ(child.find(Row { 3 }), second);
}
