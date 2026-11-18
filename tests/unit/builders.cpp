// A builder: its record's members in another memory organization (urd/elements/builder.hpp). A node has a
// leaf and a list of leaves; its builder holds each as a child, interned (the view its context makes) or built (a
// builder of its own, held by its handle), and reads as the node does.
#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/view.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/store.hpp"

#include <gtest/gtest.h>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
struct Leaf;
struct Node;
struct Nodes;
}  // namespace

template<>
struct urd::data<Leaf>
{
    urd::index<Leaf> index;
    int weight = 0;

    static constexpr bool copyable() noexcept { return true; }

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"weight", &data::weight>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::weight> {}; }
};

template<>
struct urd::data<Node>
{
    urd::index<Node> index;
    urd::index<Leaf> first;
    urd::flat_vector<urd::index<Leaf>> rest;
    int depth = 0;

    static constexpr auto fields() noexcept
    { return urd::Fields<urd::field<"first", &data::first>, urd::field<"rest", &data::rest>, urd::field<"depth", &data::depth>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::first, &data::rest, &data::depth> {}; }
};

template<typename V>
struct urd::view_mixin<Leaf, V>
{
    [[nodiscard]] auto get_weight() const { return static_cast<const V&>(*this).template member<&urd::record_of<V>::weight>(); }
};

template<typename V>
struct urd::view_mixin<Node, V>
{
    [[nodiscard]] auto get_first() const { return static_cast<const V&>(*this).template member<&urd::record_of<V>::first>(); }

    [[nodiscard]] auto get_rest() const { return static_cast<const V&>(*this).template member<&urd::record_of<V>::rest>(); }
};

template<>
struct urd::builder<Leaf, Nodes>
{
    urd::index<Leaf> index;
    int weight = 0;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"weight", &builder::weight>> {}; }
};

template<>
struct urd::builder<Node, Nodes>
{
    urd::index<Node> index;
    urd::child<Leaf, Nodes> first;
    std::vector<urd::child<Leaf, Nodes>> rest;
    int depth = 0;

    static constexpr auto fields() noexcept
    { return urd::Fields<urd::field<"first", &builder::first>, urd::field<"rest", &builder::rest>, urd::field<"depth", &builder::depth>> {}; }
};

namespace
{
struct Nodes
{
    std::vector<urd::data<Leaf>> leaves;

    const urd::data<Leaf>& operator[](urd::index<Leaf> at) const { return leaves[at.value]; }

    [[nodiscard]] urd::index_view<Leaf, Nodes> view(urd::index<Leaf> at) const { return { at, this }; }
};

using LeafBuilder = urd::builder<Leaf, Nodes>;
using NodeBuilder = urd::builder<Node, Nodes>;

using LeafPool = urd::Store<urd::slots<LeafBuilder>>;

/// The weight of a child, interned or built.
int weight_of(const auto& child)
{
    return std::visit([](const auto& leaf) { return leaf.get_weight(); }, child);
}

}  // namespace

TEST(Builder, ReadsItsChildrenInternedOrBuilt)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(urd::data<Leaf> { urd::index<Leaf>(0), 3 });

    auto pool = LeafPool {};
    auto built = pool.make_handle();
    built->weight = 7;
    auto node = NodeBuilder {};
    node.first = nodes.view(urd::index<Leaf>(0));
    node.rest.emplace_back(std::move(built));
    node.rest.emplace_back(nodes.view(urd::index<Leaf>(0)));

    const auto view = urd::resolve(node, nodes);
    static_assert(std::same_as<decltype(view), const urd::borrowed_view<Node, Nodes, NodeBuilder>>);
    EXPECT_EQ(weight_of(view.get_first()), 3);
    auto weights = std::vector<int> {};
    for (const auto& child : view.get_rest())
        weights.push_back(weight_of(child));
    EXPECT_EQ(weights, (std::vector<int> { 7, 3 }));
}

TEST(Builder, AHandleGivesItsRecordBackToItsPool)
{
    auto pool = LeafPool {};
    const LeafBuilder* first = nullptr;
    {
        auto held = pool.make_handle();
        held->weight = 3;
        first = held.get();
    }
    const auto again = pool.make_handle();
    EXPECT_EQ(again.get(), first);
    EXPECT_EQ(again->weight, 3);
}

TEST(Builder, AChildIsBuiltFromItsPool)
{
    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    urd::builder_of(node.first, pool).weight = 5;
    ASSERT_EQ(node.first.index(), 0U);
    EXPECT_EQ(std::get<0>(node.first)->weight, 5);
    EXPECT_EQ(&urd::builder_of(node.first, pool), std::get<0>(node.first).get());
}
