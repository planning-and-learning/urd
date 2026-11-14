// A sequence held of an incomplete type, as std::vector allows: a recursive type holds a Store of itself (rule I5).
// Each header a Store here needs is included.

#include "urd/elements/hash.hpp"
#include "urd/memory.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/store.hpp"

#include <compare>
#include <concepts>
#include <gtest/gtest.h>
#include <variant>

namespace
{

/// A tree: each node holds its children, and per level the nodes below it.
struct Node
{
    int value = 0;
    urd::Store<urd::sequence<Node>> children;
    urd::Store<urd::sequence<urd::Store<urd::sequence<Node>>>> levels;

    // Written out: a comparison defaulted in the class would ask the Stores', which ask Node's.
    friend bool operator==(const Node& lhs, const Node& rhs) { return lhs.value == rhs.value && lhs.children == rhs.children; }
    friend std::strong_ordering operator<=>(const Node& lhs, const Node& rhs)
    {
        if (const auto by_value = lhs.value <=> rhs.value; by_value != 0)
            return by_value;
        return lhs.children <=> rhs.children;
    }

    static constexpr auto memory_members() noexcept { return urd::Members<&Node::children, &Node::levels> {}; }
};

/// Two types that hold each other through a variant.
struct Leaf;
struct Branch;
struct Forest
{
    urd::Store<urd::sequence<std::variant<Leaf, Branch>>> trees;
};
struct Leaf
{
    int value = 0;
};
struct Branch
{
    Forest below;
};

struct Opaque
{
    int value = 0;
};

static_assert(std::equality_comparable<urd::Store<urd::sequence<Node>>>);
static_assert(std::three_way_comparable<urd::Store<urd::sequence<Node>>>);
static_assert(!std::equality_comparable<urd::Store<urd::sequence<Opaque>>>, "a Store compares where its elements do");
static_assert(!std::three_way_comparable<urd::Store<urd::sequence<Opaque>>>);

Node tree(int value, int depth)
{
    auto node = Node { .value = value, .children = {}, .levels = {} };
    if (depth > 0)
    {
        node.children.reserve(2);
        node.children.push_back(tree(value + 1, depth - 1));
        node.children.push_back(tree(value + 2, depth - 1));
    }
    return node;
}

TEST(IncompleteElement, ARecursiveTypeHoldsASequenceOfItself)
{
    auto root = tree(0, 3);
    EXPECT_EQ(root.children.size(), 2U);
    EXPECT_EQ(root.children[1].children[0].value, 3);

    auto count = 0;
    for (const auto& child : root.children)
        count += static_cast<int>(child.children.size());
    EXPECT_EQ(count, 4);

    root.levels.emplace_back();
    root.levels.back().push_back(Node {});
    EXPECT_EQ(root.levels[0].size(), 1U);
    EXPECT_GT(urd::memory_usage(root), 0U);
}

TEST(IncompleteElement, ComparesAsItsElementsDo)
{
    const auto lhs = tree(0, 2);
    auto rhs = tree(0, 2);
    EXPECT_EQ(lhs, rhs);
    EXPECT_EQ(lhs.children, rhs.children);
    rhs.children[1].value = 9;
    EXPECT_NE(lhs.children, rhs.children);
    EXPECT_LT(lhs.children, rhs.children);
}

TEST(IncompleteElement, AVariantOfIncompleteTypes)
{
    auto forest = Forest {};
    forest.trees.push_back(Leaf { 4 });
    forest.trees.push_back(Branch {});
    std::get<Branch>(forest.trees[1]).below.trees.push_back(Leaf { 5 });
    EXPECT_EQ(forest.trees.size(), 2U);
    EXPECT_EQ(std::get<Leaf>(std::get<Branch>(forest.trees[1]).below.trees[0]).value, 5);
}

}  // namespace
