// A builder: its record's members in another memory organization (urd/elements/builder.hpp). A node has a
// leaf and a list of leaves; its builder holds each as a child, interned (its index), built (a builder of its own) or
// a record not interned yet (both held by their handles), and reads as the node does.
#include "urd/elements/builder.hpp"
#include "urd/elements/children.hpp"
#include "urd/elements/clone.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/view.hpp"
#include "urd/memory.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/store.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>
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
    urd::key<Leaf> index;
    int weight = 0;

    data() = default;

    explicit data(int weight_) : index(), weight(weight_) {}

    static constexpr bool copyable() noexcept
    {
        static_assert(std::is_trivially_copyable_v<data>, "a copyable record is trivially copyable");
        return true;
    }

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"weight", &data::weight>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::weight> {}; }

    [[nodiscard]] urd::key<Leaf> key() const noexcept { return urd::key<Leaf>(index); }
};

template<>
struct urd::data<Node>
{
    urd::key<Node> index;
    urd::index<Leaf> first;
    urd::flat_vector<urd::index<Leaf>> rest;
    int depth = 0;

    data() = default;

    data(urd::index<Leaf> first_, urd::flat_vector<urd::index<Leaf>> rest_, int depth_) : index(), first(first_), rest(std::move(rest_)), depth(depth_) {}

    data(const data& other) = delete;
    data& operator=(const data& other) = delete;
    data(data&& other) = default;
    data& operator=(data&& other) = default;
    ~data() = default;

    static constexpr auto fields() noexcept
    { return urd::Fields<urd::field<"first", &data::first>, urd::field<"rest", &data::rest>, urd::field<"depth", &data::depth>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::first, &data::rest, &data::depth> {}; }

    [[nodiscard]] urd::key<Node> key() const noexcept { return urd::key<Node>(index); }
};

template<typename V>
struct urd::view_mixin<Leaf, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] decltype(auto) get_weight() const { return self().template member<&urd::record_of<V>::weight>(); }
};

template<typename V>
struct urd::view_mixin<Node, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] decltype(auto) get_first() const { return self().template member<&urd::record_of<V>::first>(); }

    [[nodiscard]] decltype(auto) get_rest() const { return self().template member<&urd::record_of<V>::rest>(); }

    [[nodiscard]] decltype(auto) get_depth() const { return self().template member<&urd::record_of<V>::depth>(); }
};

template<>
struct urd::builder<Leaf, Nodes>
{
    urd::key<Leaf> index;
    urd::built_t<int, Nodes> weight = 0;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"weight", &builder::weight>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }
};

template<>
struct urd::builder<Node, Nodes>
{
    urd::key<Node> index;
    urd::built_t<urd::index<Leaf>, Nodes> first;
    urd::built_t<urd::flat_vector<urd::index<Leaf>>, Nodes> rest;
    urd::built_t<int, Nodes> depth = 0;

    static constexpr auto fields() noexcept
    { return urd::Fields<urd::field<"first", &builder::first>, urd::field<"rest", &builder::rest>, urd::field<"depth", &builder::depth>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }
};

namespace
{
/// A leaf record stored at `at`.
urd::data<Leaf> stored_leaf(urd::index<Leaf> at, int weight)
{
    auto leaf = urd::data<Leaf>(weight);
    leaf.index = at;
    return leaf;
}

struct Nodes
{
    std::vector<urd::data<Leaf>> leaves;

    const urd::data<Leaf>& operator[](urd::index<Leaf> at) const { return leaves[at.value]; }

    [[nodiscard]] urd::index_view<Leaf, Nodes> view(urd::index<Leaf> at) const { return { at, this }; }

    /// A leaf held in place, as its view here (the context owning leaves).
    [[nodiscard]] urd::borrowed_view<Leaf, Nodes> view(const urd::data<Leaf>& leaf) const { return { &leaf, this }; }

    const urd::data<Leaf>& operator[](LeafKey key) const { return leaves[key.slot]; }

    [[nodiscard]] urd::index_view<Leaf, Nodes, LeafKey> view(LeafKey key) const { return { key, this }; }
};

using LeafBuilder = urd::builder<Leaf, Nodes>;
using NodeBuilder = urd::builder<Node, Nodes>;

using LeafPool = urd::Store<urd::slots<LeafBuilder>>;
using LeafRecords = urd::Store<urd::slots<urd::data<Leaf>>>;

/// The weights of a node's list, read element by element in whichever form it holds them.
std::vector<int> weights_of(const auto& node_view)
{
    auto weights = std::vector<int> {};
    for (const auto& leaf : node_view.get_rest())
        weights.push_back(urd::visit_view(leaf, [](const auto& view) { return view.get_weight(); }));
    return weights;
}

/// Inserts leaves (built, or records not interned yet) and nodes into Nodes, each a new index; an interned leaf is
/// Nodes' already.
struct InsertIntoNodes
{
    Nodes* nodes;
    std::vector<urd::data<Node>>* stored;

    urd::index<Leaf> operator()(urd::index<Leaf> leaf) const { return leaf; }

    urd::index<Leaf> operator()(const LeafBuilder& leaf) const
    {
        const auto at = urd::index<Leaf>(static_cast<std::uint32_t>(nodes->leaves.size()));
        nodes->leaves.push_back(stored_leaf(at, leaf.weight));
        return at;
    }

    urd::index<Leaf> operator()(const urd::data<Leaf>& leaf) const
    {
        const auto at = urd::index<Leaf>(static_cast<std::uint32_t>(nodes->leaves.size()));
        nodes->leaves.push_back(stored_leaf(at, leaf.weight));
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

/// The weight of a leaf, read as it is resolved: a view, or a child in whichever form it is held.
int weight_of(const auto& leaf)
{
    return urd::visit_view(leaf, [](const auto& view) { return view.get_weight(); });
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

// A record member's built form: keys children, flat containers their std counterparts, values as they are.
static_assert(std::same_as<urd::built_t<urd::index<Leaf>, Nodes>, urd::child<Leaf, Nodes>>);
static_assert(std::same_as<urd::built_t<urd::flat_vector<urd::index<Leaf>>, Nodes>, std::vector<urd::child<Leaf, Nodes>>>);
static_assert(
    std::same_as<urd::built_t<urd::flat_optional<urd::flat_pair<urd::index<Leaf>, int>>, Nodes>, std::optional<std::pair<urd::child<Leaf, Nodes>, int>>>);
static_assert(std::same_as<urd::built_t<urd::flat_variant<urd::index<Leaf>, int>, Nodes>, std::variant<urd::child<Leaf, Nodes>, int>>);
static_assert(std::same_as<urd::built_t<urd::flat_string, Nodes>, urd::flat_string>);

// A record's list of values is written from a builder's (urd::store): the same values, in another container.
TEST(Builder, StoresAListOfValuesFromAnotherContainer)
{
    auto stored = urd::flat_vector<int> {};
    urd::store(stored, std::vector<int> { 1, 2, 3 });
    EXPECT_EQ(stored.size(), 3U);
    EXPECT_EQ(stored[2], 3);
}

namespace
{
struct Label  // a key of no record: a value
{
};
}  // namespace

static_assert(std::same_as<urd::built_t<urd::index<Label>, Nodes>, urd::index<Label>>);
static_assert(std::same_as<urd::built_t<urd::data<Leaf>, Nodes>, urd::builder<Leaf, Nodes>>);
static_assert(std::same_as<urd::built_t<urd::index_variant<boost::mp11::mp_list<Leaf>>, Nodes>, urd::child_variant<boost::mp11::mp_list<Leaf>, Nodes>>);

TEST(Builder, ReadsItsChildrenInEachForm)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    auto records = LeafRecords {};
    auto leaf_record = records.make_handle();
    leaf_record->weight = 11;

    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    node.first = urd::index<Leaf>(0);
    urd::builder_of(node.rest.emplace_back(), pool).weight = 7;
    node.rest.emplace_back(urd::index<Leaf>(0));
    node.rest.emplace_back(std::in_place_index<2>, std::move(leaf_record));

    const auto view = urd::resolve(node, nodes);
    static_assert(std::same_as<decltype(view), const urd::borrowed_view<Node, Nodes, NodeBuilder>>);
    EXPECT_EQ(weight_of(view.get_first()), 3);
    EXPECT_EQ(weights_of(view), (std::vector<int> { 7, 3, 11 }));

    // A record's member reads as a view of its kind, a builder's as a variant of the views of its forms: both read as
    // a Leaf (urd::ReadsAs); the builder view itself is a view of a Node.
    static_assert(urd::ReadsAs<decltype(nodes.view(urd::index<Leaf>(0))), Leaf>);
    static_assert(urd::ReadsAs<decltype(view.get_first()), Leaf>);
    static_assert(!urd::ViewOf<decltype(view.get_first()), Leaf>);
    static_assert(urd::ViewOf<decltype(view), Node> && !urd::ReadsAs<decltype(view), Leaf>);

    // A child copied from a reading: an interned one as it is; a built or held one is its holder's, not copied.
    auto copied = urd::child<Leaf, Nodes> {};
    urd::copy_child(copied, view.get_first());
    EXPECT_EQ(weight_of(urd::resolve(std::as_const(copied), nodes)), 3);
    EXPECT_THROW(urd::copy_child(copied, urd::resolve(std::as_const(node.rest[0]), nodes)), std::bad_variant_access);
    EXPECT_THROW(urd::copy_child(copied, urd::resolve(std::as_const(node.rest[2]), nodes)), std::bad_variant_access);

    // A reading's interned view: an interned one's, else none (a built or held record has no key).
    EXPECT_EQ(urd::interned_view(view.get_first()).index(), urd::index<Leaf>(0));
    EXPECT_THROW((void) urd::interned_view(urd::resolve(std::as_const(node.rest[0]), nodes)), std::bad_variant_access);

    // A reading is the interned record only where it is interned: a built one of the same weight is another record.
    EXPECT_TRUE(urd::is_interned_as(view.get_first(), nodes.view(urd::index<Leaf>(0))));
    EXPECT_TRUE(urd::is_interned_as(nodes.view(urd::index<Leaf>(0)), nodes.view(urd::index<Leaf>(0))));
    EXPECT_FALSE(urd::is_interned_as(urd::resolve(std::as_const(node.rest[2]), nodes), nodes.view(urd::index<Leaf>(0))));
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
    // The same slot, emptied as it came back.
    const auto again = pool.make_handle();
    EXPECT_EQ(again.get(), first);
    EXPECT_EQ(again->weight, 0);
}

TEST(Builder, AHandleGivesItsChildrenBackWithIt)
{
    auto leaves = LeafPool {};
    auto nodes = urd::Store<urd::slots<NodeBuilder>> {};
    const LeafBuilder* leaf = nullptr;
    {
        auto node = nodes.make_handle();
        auto& built = urd::builder_of(node->rest.emplace_back(), leaves);
        built.weight = 7;
        leaf = &built;
    }
    // The node went back emptied, its list cleared: its built leaf is back in its pool, emptied too.
    const auto again = leaves.make_handle();
    EXPECT_EQ(again.get(), leaf);
    EXPECT_EQ(again->weight, 0);
}

TEST(Builder, AChildIsBuiltFromItsPool)
{
    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    auto nodes = Nodes {};
    urd::builder_of(node.first, pool).weight = 5;
    ASSERT_EQ(node.first.index(), 1U);
    EXPECT_EQ(std::get<1>(node.first)->weight, 5);
    EXPECT_EQ(&urd::builder_of(node.first, pool), std::get<1>(node.first).get());
}

TEST(Builder, InternsItsChildrenFirst)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    auto records = LeafRecords {};
    auto leaf_record = records.make_handle();
    leaf_record->weight = 11;
    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    node.first = urd::index<Leaf>(0);
    urd::builder_of(node.rest.emplace_back(), pool).weight = 7;
    node.rest.emplace_back(std::in_place_index<2>, std::move(leaf_record));
    node.rest.emplace_back(urd::index<Leaf>(0));
    node.depth = 2;

    auto stored = std::vector<urd::data<Node>> {};
    auto record = urd::data<Node> {};
    const auto at = urd::intern(node, record, InsertIntoNodes { &nodes, &stored });
    ASSERT_EQ(at, urd::index<Node>(0));
    const auto& interned = stored[0];
    EXPECT_EQ(interned.first, urd::index<Leaf>(0));
    ASSERT_EQ(interned.rest.size(), 3U);
    EXPECT_EQ(nodes.leaves[interned.rest[0].value].weight, 7);
    EXPECT_EQ(nodes.leaves[interned.rest[1].value].weight, 11);
    EXPECT_EQ(interned.rest[2], urd::index<Leaf>(0));
    EXPECT_EQ(interned.depth, 2);
}

TEST(Builder, AChildsKeyIsTheOneItsViewNames)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    const auto keyed = urd::child<Leaf, Nodes, LeafKey>(LeafKey { 0 });
    const auto read = urd::resolve(keyed, nodes);
    EXPECT_EQ(weight_of(read), 3);
    EXPECT_EQ(urd::inserted_index(nodes.view(LeafKey { 0 })), LeafKey { 0 });
    EXPECT_EQ(weight_of(nodes.view(LeafKey { 0 })), 3);  // a view as it is
}

// A child owns no memory of its own: a pooled form's record belongs to its pool. A variant owns what it holds.
TEST(Builder, AChildOwnsNoMemoryOfItsOwn)
{
    auto nodes = Nodes {};
    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    urd::builder_of(node.first, pool).weight = 5;
    EXPECT_EQ(urd::memory_usage(node.first), 0U);
    // A builder owns what its fields own: its list of children, not their records.
    node.rest.resize(8);
    EXPECT_EQ(urd::memory_usage(node), urd::memory_usage(node.rest));
    EXPECT_GE(urd::memory_usage(node), 8 * sizeof(urd::child<Leaf, Nodes>));
    const auto text = std::variant<int, std::string>(std::string(200, 'x'));
    EXPECT_GE(urd::memory_usage(text), 200U);
}

// A child may own its record on the heap (urd::make_owned: where no pool is at hand, as a builder made in Python): the
// handle owns what its record owns, and is cloned onto the heap.
TEST(Builder, AnOwnedChildOwnsItsRecord)
{
    auto node = NodeBuilder {};
    node.first.emplace<1>(urd::make_owned<LeafBuilder>());
    std::get<1>(node.first)->weight = 4;
    EXPECT_TRUE(urd::is_owned(std::get<1>(node.first)));
    EXPECT_GE(urd::memory_usage(node.first), sizeof(LeafBuilder));
    const auto copy = urd::clone(node);
    EXPECT_TRUE(urd::is_owned(std::get<1>(copy.first)));
    EXPECT_NE(std::get<1>(copy.first).get(), std::get<1>(node.first).get());
    EXPECT_EQ(std::get<1>(copy.first)->weight, 4);
}

namespace
{
/// A value made for a key (its width, say), as a keyed Store of slots makes one.
struct Sized
{
    explicit Sized(int width_) : width(width_) {}

    int width = 0;
    int uses = 0;
};
}  // namespace

// A keyed Store of slots hands out handles too: a slot given back under its key is handed out again for that key only.
TEST(Builder, AKeyedHandleGoesBackUnderItsKey)
{
    auto sized = urd::Store<urd::slots<Sized>, urd::keyed<int>>();
    const Sized* two = nullptr;
    {
        auto held = sized.make_handle(2);
        held->uses = 1;
        two = held.get();
    }
    auto other = sized.make_handle(3);
    EXPECT_NE(other.get(), two);
    EXPECT_EQ(other->width, 3);
    const auto again = sized.make_handle(2);
    EXPECT_EQ(again.get(), two);
    EXPECT_EQ(again->uses, 1);  // as it was given back
}

// A builder is filled from a record read through its context, each child as the view f gives (here: as it is), and is
// cleared for reuse.
TEST(Builder, IsBuiltFromARecordAndClearedForReuse)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(1), 5));
    auto record = urd::data<Node> {};
    record.first = urd::index<Leaf>(1);
    record.rest.push_back(urd::index<Leaf>(0));
    record.depth = 4;
    const auto source = urd::borrowed_view<Node, Nodes> { &record, &nodes };

    auto node = NodeBuilder {};
    urd::build_from(source, node, [](const auto& leaf) { return leaf; });
    EXPECT_EQ(weight_of(urd::resolve(node.first, nodes)), 5);
    ASSERT_EQ(node.rest.size(), 1U);
    EXPECT_EQ(weight_of(urd::resolve(node.rest[0], nodes)), 3);
    EXPECT_EQ(node.depth, 4);

    urd::clear_fields(node);
    EXPECT_TRUE(node.rest.empty());
    EXPECT_EQ(node.depth, 0);
}

// A record's variant members, inside a vector, map to a builder's and back (each overload reaches the others).
TEST(Builder, MapsVariantsInsideVectorsBothWays)
{
    using Term = urd::flat_variant<urd::index<Leaf>, int>;
    using BuiltTerm = std::variant<urd::child<Leaf, Nodes>, int>;
    auto nodes = Nodes {};
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    auto terms = urd::flat_vector<Term> {};
    terms.push_back(Term { urd::index<Leaf>(0) });
    terms.push_back(Term { 7 });

    auto built = std::vector<BuiltTerm> {};
    urd::build_member(built, terms, nodes);
    ASSERT_EQ(built.size(), 2U);
    EXPECT_EQ(weight_of(urd::resolve(std::get<0>(built[0]), nodes)), 3);
    EXPECT_EQ(std::get<1>(built[1]), 7);
}

// A builder that holds a built or held child is move-only (its child owns its record by a unique handle); urd::clone
// copies it deeply: each built or held child into a slot of its own of the same pool, interned children and values as
// they are.
TEST(Builder, IsClonedWithItsChildren)
{
    static_assert(!std::copy_constructible<NodeBuilder> && !urd::CopiedAsIs<NodeBuilder>);
    static_assert(urd::CopiedAsIs<LeafBuilder>);  // no child: copied as it is

    auto nodes = Nodes {};
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    auto records = LeafRecords {};
    auto leaf_record = records.make_handle();
    leaf_record->weight = 11;
    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    node.first = urd::index<Leaf>(0);
    urd::builder_of(node.rest.emplace_back(), pool).weight = 7;
    node.rest.emplace_back(urd::index<Leaf>(0));
    node.rest.emplace_back(std::in_place_index<2>, std::move(leaf_record));
    node.depth = 2;

    auto copy = urd::clone(node);
    EXPECT_EQ(weights_of(urd::resolve(copy, nodes)), (std::vector<int> { 7, 3, 11 }));
    EXPECT_EQ(weight_of(urd::resolve(std::as_const(copy.first), nodes)), 3);
    EXPECT_EQ(copy.depth, 2);
    // The built and the held child each have a record of their own, in the same pool.
    EXPECT_NE(std::get<1>(copy.rest[0]).get(), std::get<1>(node.rest[0]).get());
    EXPECT_NE(std::get<2>(copy.rest[2]).get(), std::get<2>(node.rest[2]).get());
    std::get<1>(copy.rest[0])->weight = 8;
    std::get<2>(copy.rest[2])->weight = 12;
    EXPECT_EQ(weights_of(urd::resolve(node, nodes)), (std::vector<int> { 7, 3, 11 }));
    EXPECT_EQ(weights_of(urd::resolve(copy, nodes)), (std::vector<int> { 8, 3, 12 }));

    // A cloned slot goes back to its pool with its copy.
    const auto* cloned = std::get<1>(copy.rest[0]).get();
    copy = NodeBuilder {};
    EXPECT_EQ(&urd::builder_of(copy.first, pool), cloned);

    // A keyed slot is drawn under its key, so its handle is not cloned.
    auto sized = urd::Store<urd::slots<Sized>, urd::keyed<int>>();
    const auto two = sized.make_handle(2);
    EXPECT_THROW((void) urd::clone(two), std::logic_error);
}

namespace
{
/// A record whose values are children or plain values by their position (urd::element_children): even positions name
/// leaves of Nodes, odd ones are plain numbers.
struct Mixed;
}  // namespace

template<>
struct urd::data<Mixed>
{
    urd::key<Mixed> index;
    urd::flat_vector<std::uint32_t> values;

    data() = default;

    explicit data(urd::flat_vector<std::uint32_t> values_) : index(), values(std::move(values_)) {}

    data(const data& other) = delete;
    data& operator=(const data& other) = delete;
    data(data&& other) = default;
    data& operator=(data&& other) = default;
    ~data() = default;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"values", &data::values>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::values> {}; }

    [[nodiscard]] urd::key<Mixed> key() const noexcept { return urd::key<Mixed>(index); }
};

template<typename V>
struct urd::view_mixin<Mixed, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] decltype(auto) get_values() const { return self().template member<&urd::record_of<V>::values>(); }
};

template<>
struct urd::builder<Mixed, Nodes>
{
    urd::key<Mixed> index;
    urd::built_t<urd::flat_vector<std::uint32_t>, Nodes> values;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"values", &builder::values>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }
};

template<>
struct urd::element_children<&urd::data<Mixed>::values>
{
    using child_type = urd::index<Leaf>;

    template<typename R, typename C>
    [[nodiscard]] static bool is_child(const R& /*stored*/, const C& /*context*/, std::size_t position) noexcept
    { return position % 2 == 0; }

    [[nodiscard]] static child_type child(std::uint32_t element) noexcept { return child_type(element); }

    [[nodiscard]] static std::uint32_t element(child_type child) noexcept { return child.value; }
};

static_assert(urd::HasElementChildren<&urd::data<Mixed>::values> && !urd::HasElementChildren<&urd::data<Node>::rest>);

// A member whose elements are children by their position is built element by element: each child as f gives for the
// view of what it names, each plain value as it is.
TEST(Builder, BuildsElementChildrenByTheirPosition)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(1), 5));
    const auto record = urd::data<Mixed>(urd::flat_vector<std::uint32_t> { 0, 7, 1, 9 });
    const auto source = urd::borrowed_view<Mixed, Nodes> { &record, &nodes };

    auto built = urd::builder<Mixed, Nodes> {};
    urd::build_from(source, built, [](const urd::index_view<Leaf, Nodes>& leaf) { return urd::index<Leaf>(leaf.index().value + 10); });
    EXPECT_EQ(built.values, (std::vector<std::uint32_t> { 10, 7, 11, 9 }));
}
