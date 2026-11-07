// urd::for_each_child and urd::map_children: the child indices of a record, in every nested shape of its cista members,
// as the views their context makes.

#include "urd/elements/children.hpp"
#include "urd/elements/view.hpp"

#include <boost/mp11/list.hpp>
#include <cista/containers/optional.h>
#include <cista/containers/pair.h>
#include <cista/containers/variant.h>
#include <concepts>
#include <gtest/gtest.h>
#include <variant>
#include <vector>

namespace
{

struct Leaf
{
};
using LeafIndex = urd::index<Leaf>;

struct Pair
{
    LeafIndex first;
    LeafIndex second;
    static constexpr auto cista_members() noexcept { return urd::Members<&Pair::first, &Pair::second> {}; }
};

struct Node
{
    urd::index<Node> self;
    LeafIndex single;
    urd::flat_vector<urd::flat_vector<LeafIndex>> matrix;
    urd::flat_vector<urd::flat_variant<LeafIndex, int>> variants;
    urd::flat_vector<Pair> pairs;
    urd::flat_vector<urd::flat_variant<LeafIndex, urd::index<Node>>> operands;
    urd::flat_optional<LeafIndex> present;
    urd::flat_optional<LeafIndex> absent;
    urd::flat_pair<LeafIndex, urd::index<Node>> link;
    int unrelated = 0;
    static constexpr auto cista_members() noexcept
    {
        return urd::Members<&Node::self,
                            &Node::single,
                            &Node::matrix,
                            &Node::variants,
                            &Node::pairs,
                            &Node::operands,
                            &Node::present,
                            &Node::absent,
                            &Node::link,
                            &Node::unrelated> {};
    }
};

struct Context
{
    template<typename T>
    [[nodiscard]] urd::index_view<T, Context> view(urd::index<T> at) const
    { return { at, this }; }
};

// Node is listed: a nested index<Node> is a child, Node's own handle (its direct index<Node> member) is not.
using Children = boost::mp11::mp_list<Leaf, Node>;

Node make_node()
{
    auto node = Node();
    node.self = urd::index<Node>(0);
    node.single = LeafIndex(1);
    node.matrix.emplace_back();
    node.matrix[0].push_back(LeafIndex(2));
    node.matrix[0].push_back(LeafIndex(3));
    node.variants.push_back(LeafIndex(4));
    node.variants.push_back(7);
    node.pairs.push_back(Pair { LeafIndex(5), LeafIndex(6) });
    node.operands.push_back(LeafIndex(10));
    node.operands.push_back(urd::index<Node>(11));
    node.present = LeafIndex(12);
    node.link = { LeafIndex(13), urd::index<Node>(14) };
    node.unrelated = 9;
    return node;
}

TEST(Children, VisitsEveryNestedShapeInMemberOrder)
{
    const auto context = Context();
    auto visited = std::vector<LeafIndex::value_type>();
    auto nodes = std::vector<LeafIndex::value_type>();
    urd::for_each_child<Children, Node>(make_node(),
                                        context,
                                        [&]<typename U>(urd::index_view<U, Context> child)
                                        {
                                            visited.push_back(child.index().value);
                                            if constexpr (std::same_as<U, Node>)
                                                nodes.push_back(child.index().value);
                                        });
    EXPECT_EQ(visited, (std::vector<LeafIndex::value_type> { 1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 14 }));
    EXPECT_EQ(nodes, (std::vector<LeafIndex::value_type> { 11, 14 }));
}

TEST(Children, MapsEveryNestedShapeAndKeepsTheRest)
{
    const auto context = Context();
    const auto mapped = urd::map_children<Children, Node>(make_node(),
                                                          context,
                                                          []<typename U>(urd::index_view<U, Context> child)
                                                          { return urd::index<U>(child.index().value + 100); });
    EXPECT_EQ(mapped.single.value, 101U);
    EXPECT_EQ(mapped.matrix[0][0].value, 102U);
    EXPECT_EQ(mapped.matrix[0][1].value, 103U);
    EXPECT_EQ(cista::get<LeafIndex>(mapped.variants[0]).value, 104U);
    EXPECT_EQ(cista::get<int>(mapped.variants[1]), 7);
    EXPECT_EQ(mapped.pairs[0].first.value, 105U);
    EXPECT_EQ(mapped.pairs[0].second.value, 106U);
    EXPECT_EQ(cista::get<LeafIndex>(mapped.operands[0]).value, 110U);
    EXPECT_EQ(cista::get<urd::index<Node>>(mapped.operands[1]).value, 111U);
    EXPECT_EQ(mapped.present->value, 112U);
    EXPECT_FALSE(mapped.absent.has_value());
    EXPECT_EQ(mapped.link.first.value, 113U);
    EXPECT_EQ(mapped.link.second.value, 114U);
    EXPECT_EQ(mapped.unrelated, 9);
    // The record's own handle is kept.
    EXPECT_EQ(mapped.self.value, 0U);
}

TEST(Children, MapsChildrenThroughViewsToo)
{
    const auto context = Context();
    const auto mapped = urd::map_children<Children, Node>(make_node(), context, [](auto child) { return child; });
    EXPECT_EQ(mapped.single.value, 1U);
    EXPECT_EQ(cista::get<urd::index<Node>>(mapped.operands[1]).value, 11U);
}

using Operand = std::variant<urd::index_view<Leaf, Context>, urd::index_view<Node, Context>>;

// Each variant slot (outside another variant) is handed to the callback whole, resolved, and may become another
// alternative; the children outside variants are mapped as map_children maps them; the record's own handle is kept.
TEST(Children, MapsWholeVariantSlots)
{
    const auto context = Context();
    auto slots = std::vector<std::size_t>();
    const auto mapped = urd::map_variant_children<Children, Node>(
        make_node(),
        context,
        [&]<typename A>(const A& child)
        {
            if constexpr (std::same_as<A, Operand>)
            {
                slots.push_back(child.index());
                // Every operand becomes a Node.
                return std::visit([](auto operand) { return urd::index<Node>(operand.index().value + 200); }, child);
            }
            else if constexpr (urd::variant_value<A>)
            {
                slots.push_back(child.index());
                return child;
            }
            else
                return decltype(child.index())(child.index().value + 100);
        });
    EXPECT_EQ(slots, (std::vector<std::size_t> { 0, 1, 0, 1 }));
    EXPECT_EQ(mapped.single.value, 101U);
    EXPECT_EQ(mapped.matrix[0][1].value, 103U);
    EXPECT_EQ(mapped.pairs[0].second.value, 106U);
    EXPECT_EQ(mapped.present->value, 112U);
    EXPECT_EQ(mapped.link.second.value, 114U);
    EXPECT_EQ(mapped.self.value, 0U);
    EXPECT_EQ(cista::get<LeafIndex>(mapped.variants[0]).value, 4U);
    EXPECT_EQ(cista::get<int>(mapped.variants[1]), 7);
    EXPECT_EQ(cista::get<urd::index<Node>>(mapped.operands[0]).value, 210U);
    EXPECT_EQ(cista::get<urd::index<Node>>(mapped.operands[1]).value, 211U);
}

}  // namespace
