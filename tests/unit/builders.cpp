// A builder: its record's members in another memory organization (urd/elements/builder.hpp). A node has a
// leaf and a list of leaves; its builder holds each as a child, interned (its index), built (a builder of its own) or
// a record not interned yet (both held by their handles), and reads as the node does.
#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/children.hpp"
#include "urd/elements/view.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/store.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
struct Leaf;
struct Node;
struct Nodes;

/// A key of its own for a leaf (as a row's is its pool and its id): its slot in Nodes.
struct LeafKey
{
    std::uint32_t slot = 0;

    friend bool operator==(const LeafKey&, const LeafKey&) = default;
};
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

    const urd::data<Leaf>& operator[](LeafKey key) const { return leaves[key.slot]; }

    [[nodiscard]] urd::index_view<Leaf, Nodes, LeafKey> view(LeafKey key) const { return { key, this }; }
};

using LeafBuilder = urd::builder<Leaf, Nodes>;
using NodeBuilder = urd::builder<Node, Nodes>;

using LeafPool = urd::Store<urd::slots<LeafBuilder>>;
using LeafRecords = urd::Store<urd::slots<urd::data<Leaf>>>;

/// Inserts leaves (built, or records not interned yet) and nodes into Nodes, each a new index; an interned leaf is
/// Nodes' already.
struct InsertIntoNodes
{
    Nodes* nodes;
    std::vector<urd::data<Node>>* stored;

    urd::index<Leaf> operator()(const urd::index_view<Leaf, Nodes>& leaf) const { return leaf.index(); }

    urd::index<Leaf> operator()(const LeafBuilder& leaf) const
    {
        const auto at = urd::index<Leaf>(static_cast<std::uint32_t>(nodes->leaves.size()));
        nodes->leaves.push_back(urd::data<Leaf> { at, leaf.weight });
        return at;
    }

    urd::index<Leaf> operator()(const urd::data<Leaf>& leaf) const
    {
        const auto at = urd::index<Leaf>(static_cast<std::uint32_t>(nodes->leaves.size()));
        nodes->leaves.push_back(urd::data<Leaf> { at, leaf.weight });
        return at;
    }

    urd::index<Node> operator()(const urd::data<Node>& node) const
    {
        const auto at = urd::index<Node>(static_cast<std::uint32_t>(stored->size()));
        auto& copy = stored->emplace_back();
        copy.index = at;
        copy.first = node.first;
        copy.rest = node.rest;
        copy.depth = node.depth;
        return at;
    }
};

/// The weight of a child, in whichever form it is held.
int weight_of(const auto& child)
{
    return std::visit([](const auto& leaf) { return leaf.get_weight(); }, child);
}

}  // namespace

/// A leaf's key resolved as any key: the view its context makes of it.
template<>
struct urd::resolution<LeafKey>
{
    template<typename C>
    static auto of(LeafKey key, const C& context)
    { return context.view(key); }
};

TEST(Builder, ReadsItsChildrenInEachForm)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(urd::data<Leaf> { urd::index<Leaf>(0), 3 });
    auto records = LeafRecords {};
    auto leaf_record = records.make_handle();
    leaf_record->weight = 11;

    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    node.first = nodes.view(urd::index<Leaf>(0));
    urd::builder_of(node.rest.emplace_back(), pool, nodes).weight = 7;
    node.rest.emplace_back(nodes.view(urd::index<Leaf>(0)));
    node.rest.emplace_back(std::in_place_index<2>, std::move(leaf_record), &nodes);

    const auto view = urd::resolve(node, nodes);
    static_assert(std::same_as<decltype(view), const urd::borrowed_view<Node, Nodes, NodeBuilder>>);
    EXPECT_EQ(weight_of(view.get_first()), 3);
    auto weights = std::vector<int> {};
    for (const auto& child : view.get_rest())
        weights.push_back(weight_of(child));
    EXPECT_EQ(weights, (std::vector<int> { 7, 3, 11 }));
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
    auto nodes = Nodes {};
    urd::builder_of(node.first, pool, nodes).weight = 5;
    ASSERT_EQ(node.first.index(), 1U);
    EXPECT_EQ(std::get<1>(node.first).held().weight, 5);
    EXPECT_EQ(&urd::builder_of(node.first, pool, nodes), &std::get<1>(node.first).held());
}

TEST(Builder, InternsItsChildrenFirst)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(urd::data<Leaf> { urd::index<Leaf>(0), 3 });
    auto records = LeafRecords {};
    auto leaf_record = records.make_handle();
    leaf_record->weight = 11;
    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    node.first = nodes.view(urd::index<Leaf>(0));
    urd::builder_of(node.rest.emplace_back(), pool, nodes).weight = 7;
    node.rest.emplace_back(std::in_place_index<2>, std::move(leaf_record), &nodes);
    node.depth = 2;

    auto stored = std::vector<urd::data<Node>> {};
    auto record = urd::data<Node> {};
    const auto at = urd::intern(node, record, InsertIntoNodes { &nodes, &stored });
    ASSERT_EQ(at, urd::index<Node>(0));
    const auto& interned = stored[0];
    EXPECT_EQ(interned.first, urd::index<Leaf>(0));
    ASSERT_EQ(interned.rest.size(), 2U);
    EXPECT_EQ(nodes.leaves[interned.rest[0].value].weight, 7);
    EXPECT_EQ(nodes.leaves[interned.rest[1].value].weight, 11);
    EXPECT_EQ(interned.depth, 2);
}

TEST(Builder, AChildsKeyIsTheOneItsViewNames)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(urd::data<Leaf> { urd::index<Leaf>(0), 3 });
    const auto keyed = urd::child<Leaf, Nodes, LeafKey>(nodes.view(LeafKey { 0 }));
    const auto read = urd::resolve(keyed, nodes);
    EXPECT_EQ(weight_of(read), 3);
    EXPECT_EQ(urd::inserted_index(nodes.view(LeafKey { 0 })), LeafKey { 0 });
}
