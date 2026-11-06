// urd::for_each_child and urd::map_children: the child indices of a record, in every nested shape of its cista members,
// as the views their context makes.

#include "urd/elements/children.hpp"
#include "urd/elements/view.hpp"

#include <boost/mp11/list.hpp>
#include <cista/containers/variant.h>
#include <gtest/gtest.h>
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
    int unrelated = 0;
    static constexpr auto cista_members() noexcept
    { return urd::Members<&Node::self, &Node::single, &Node::matrix, &Node::variants, &Node::pairs, &Node::unrelated> {}; }
};

struct Context
{
    template<typename T>
    [[nodiscard]] urd::view<T, Context> view(urd::index<T> at) const
    { return { at, *this }; }
};

// Node is listed, but a record's own type is never its child.
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
    node.unrelated = 9;
    return node;
}

TEST(Children, VisitsEveryNestedShapeInMemberOrder)
{
    const auto context = Context();
    auto visited = std::vector<LeafIndex::value_type>();
    urd::for_each_child<Children, Node>(make_node(), context, [&](urd::view<Leaf, Context> child) { visited.push_back(child.index().value); });
    EXPECT_EQ(visited, (std::vector<LeafIndex::value_type> { 1, 2, 3, 4, 5, 6 }));
}

TEST(Children, MapsEveryNestedShapeAndKeepsTheRest)
{
    const auto context = Context();
    const auto mapped = urd::map_children<Children, Node>(make_node(), context,
                                                          [](urd::view<Leaf, Context> child) { return LeafIndex(child.index().value + 100); });
    EXPECT_EQ(mapped.single.value, 101U);
    EXPECT_EQ(mapped.matrix[0][0].value, 102U);
    EXPECT_EQ(mapped.matrix[0][1].value, 103U);
    EXPECT_EQ(cista::get<LeafIndex>(mapped.variants[0]).value, 104U);
    EXPECT_EQ(cista::get<int>(mapped.variants[1]), 7);
    EXPECT_EQ(mapped.pairs[0].first.value, 105U);
    EXPECT_EQ(mapped.pairs[0].second.value, 106U);
    EXPECT_EQ(mapped.unrelated, 9);
    EXPECT_EQ(mapped.self.value, 0U);
}

}  // namespace
