// A builder: its record's members in another memory organization (urd/elements/builder.hpp). A node has a
// leaf and a list of leaves; its builder holds each as a child, interned (its index), built (a builder of its own) or
// a record not interned yet (both held by their handles), and reads as the node does.
#include "urd/elements/builder.hpp"
#include "urd/elements/children.hpp"
#include "urd/elements/clone.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/content.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/view.hpp"
#include "urd/memory.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/store.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <list>
#include <memory>
#include <optional>
#include <span>
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
    [[nodiscard]] decltype(auto) weight() const { return self().template member<&urd::record_of<V>::weight>(); }
};

template<typename V>
struct urd::view_mixin<Node, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] decltype(auto) first() const { return self().template member<&urd::record_of<V>::first>(); }

    [[nodiscard]] decltype(auto) rest() const { return self().template member<&urd::record_of<V>::rest>(); }

    [[nodiscard]] decltype(auto) depth() const { return self().template member<&urd::record_of<V>::depth>(); }
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

    /// Its number, as a repository's: its views compare by identity (urd::HasLayers).
    [[nodiscard]] std::size_t layer() const noexcept { return 0; }

    /// A builder of a leaf, shared (urd::Unpacks, urd::UnpacksRecords): of the one stored under `at`, or of a record it
    /// does not hold; counted, the last one handed out kept weakly.
    [[nodiscard]] std::shared_ptr<const urd::builder<Leaf, Nodes>> unpack(urd::index<Leaf> at) const
    {
        ++key_unpacks;
        return shared(leaves[at.value].weight);
    }

    [[nodiscard]] std::shared_ptr<const urd::builder<Leaf, Nodes>> unpack(const urd::data<Leaf>& leaf) const
    {
        ++record_unpacks;
        return shared(leaf.weight);
    }

    [[nodiscard]] std::shared_ptr<const urd::builder<Leaf, Nodes>> shared(int weight) const
    {
        auto built = std::make_shared<urd::builder<Leaf, Nodes>>();
        built->weight = weight;
        last_unpacked = built;
        return built;
    }

    mutable int key_unpacks = 0;
    mutable int record_unpacks = 0;
    mutable std::weak_ptr<const urd::builder<Leaf, Nodes>> last_unpacked;
};

using LeafBuilder = urd::builder<Leaf, Nodes>;
using NodeBuilder = urd::builder<Node, Nodes>;

using LeafPool = urd::Store<urd::slots<LeafBuilder>>;
using LeafRecords = urd::Store<urd::slots<urd::data<Leaf>>>;

/// The weights of a node's list, read element by element in whichever form it holds them.
std::vector<int> weights_of(const auto& node_view)
{
    auto weights = std::vector<int> {};
    for (const auto& leaf : node_view.rest())
        weights.push_back(urd::visit_view(leaf, [](const auto& view) { return view.weight(); }));
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
    return urd::visit_view(leaf, [](const auto& view) { return view.weight(); });
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
static_assert(std::same_as<urd::built_t<urd::index_variant<boost::mp11::mp_list<Leaf>>, Nodes>, std::variant<urd::child<Leaf, Nodes>>>);

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
    EXPECT_EQ(weight_of(view.first()), 3);
    EXPECT_EQ(weights_of(view), (std::vector<int> { 7, 3, 11 }));

    // A record's member reads as a view of its kind, a builder's as a child view (of the views of its forms): both read
    // as a Leaf (urd::ReadsAs); the builder view itself is a view of a Node.
    static_assert(urd::ReadsAs<decltype(nodes.view(urd::index<Leaf>(0))), Leaf>);
    static_assert(urd::ReadsAs<decltype(view.first()), Leaf>);
    static_assert(!urd::ViewOf<decltype(view.first()), Leaf>);
    static_assert(urd::ViewOf<decltype(view), Node> && !urd::ReadsAs<decltype(view), Leaf>);

    // A child copied from a reading: an interned one as it is; a built or held one is its holder's, not copied.
    auto copied = urd::child<Leaf, Nodes> {};
    urd::copy_child(copied, view.first());
    EXPECT_EQ(weight_of(urd::resolve(std::as_const(copied), nodes)), 3);
    EXPECT_THROW(urd::copy_child(copied, urd::resolve(std::as_const(node.rest[0]), nodes)), std::bad_variant_access);
    EXPECT_THROW(urd::copy_child(copied, urd::resolve(std::as_const(node.rest[2]), nodes)), std::bad_variant_access);

    // A reading's interned view: an interned one's, else none (a built or held record has no key).
    EXPECT_EQ(urd::interned_view(view.first()).index(), urd::index<Leaf>(0));
    EXPECT_THROW((void) urd::interned_view(urd::resolve(std::as_const(node.rest[0]), nodes)), std::bad_variant_access);

    // A reading is the interned record only where it is interned: a built one of the same weight is another record.
    EXPECT_TRUE(urd::is_interned_as(view.first(), nodes.view(urd::index<Leaf>(0))));
    EXPECT_TRUE(urd::is_interned_as(nodes.view(urd::index<Leaf>(0)), nodes.view(urd::index<Leaf>(0))));
    EXPECT_FALSE(urd::is_interned_as(urd::resolve(std::as_const(node.rest[2]), nodes), nodes.view(urd::index<Leaf>(0))));
}

TEST(ChildView, ReadsEachFormWithItsKindsGettersAndComparesAsItsForm)
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

    // A builder's child reads as a child view, with the kind's getters whatever its form: built, interned, held.
    static_assert(urd::ChildView<decltype(view.first())>);
    auto weights = std::vector<int> {};
    for (const auto leaf : view.rest())
        weights.push_back(leaf.weight());
    EXPECT_EQ(weights, (std::vector<int> { 7, 3, 11 }));
    EXPECT_EQ(view.first().weight(), 3);

    // It compares as the view of its form: two readings of one interned leaf are equal; a built leaf is another reading,
    // whatever its fields.
    EXPECT_EQ(view.first(), urd::resolve(std::as_const(node.rest[1]), nodes));
    EXPECT_NE(view.first(), urd::resolve(std::as_const(node.rest[0]), nodes));

    // Its form, where a reader needs it: interned (its key), built (its builder's view).
    EXPECT_EQ(urd::interned_view(view.first()).index(), urd::index<Leaf>(0));
    EXPECT_EQ(urd::built_view(urd::resolve(std::as_const(node.rest[0]), nodes)).weight(), 7);
}

// A child view unpacked: its built form the builder itself, its interned and held forms the builder their context unpacks
// them into, kept while a copy of the view lives.
TEST(ChildView, UnpacksEachFormThroughItsContext)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    auto records = LeafRecords {};
    auto leaf_record = records.make_handle();
    leaf_record->weight = 11;

    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    urd::builder_of(node.rest.emplace_back(), pool).weight = 7;
    node.rest.emplace_back(urd::index<Leaf>(0));
    node.rest.emplace_back(std::in_place_index<2>, std::move(leaf_record));

    const auto built = urd::unpacked(urd::resolve(std::as_const(node.rest[0]), nodes));
    EXPECT_EQ(&urd::record(built), &urd::built(node.rest[0]));
    EXPECT_EQ(nodes.key_unpacks + nodes.record_unpacks, 0);

    {
        auto interned = std::optional(urd::unpacked(urd::resolve(std::as_const(node.rest[1]), nodes)));
        EXPECT_EQ(nodes.key_unpacks, 1);
        EXPECT_EQ(interned->weight(), 3);
        const auto copy = *interned;
        interned.reset();
        EXPECT_FALSE(nodes.last_unpacked.expired());
        EXPECT_EQ(copy.weight(), 3);
    }
    EXPECT_TRUE(nodes.last_unpacked.expired());

    const auto held = urd::unpacked(urd::resolve(std::as_const(node.rest[2]), nodes));
    EXPECT_EQ(nodes.record_unpacks, 1);
    EXPECT_EQ(nodes.key_unpacks, 1);
    EXPECT_EQ(held.weight(), 11);
}

// A child view packed: the record of the form it holds, its own: a built one its builder packed (its index the
// builder's), an interned one its stored record read out (its index the key), a held one a copy of its record.
TEST(ChildView, PacksTheRecordOfEachForm)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    auto records = LeafRecords {};
    auto leaf_record = records.make_handle();
    leaf_record->weight = 11;

    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    auto& built = urd::builder_of(node.rest.emplace_back(), pool);
    built.weight = 7;
    built.index = urd::index<Leaf>(4);
    node.rest.emplace_back(urd::index<Leaf>(0));
    node.rest.emplace_back(std::in_place_index<2>, std::move(leaf_record));

    static_assert(urd::Packable<decltype(urd::resolve(std::as_const(node.rest[0]), nodes))>);

    const auto packed_built = urd::pack(urd::resolve(std::as_const(node.rest[0]), nodes));
    static_assert(std::same_as<decltype(packed_built), const urd::data<Leaf>>);
    EXPECT_EQ(packed_built.weight, 7);
    EXPECT_EQ(packed_built.index, urd::index<Leaf>(4));

    const auto packed_interned = urd::pack(urd::resolve(std::as_const(node.rest[1]), nodes));
    EXPECT_EQ(packed_interned.weight, 3);
    EXPECT_EQ(packed_interned.index, urd::index<Leaf>(0));

    const auto packed_held = urd::pack(urd::resolve(std::as_const(node.rest[2]), nodes));
    EXPECT_EQ(packed_held.weight, 11);
    EXPECT_NE(&packed_held, &*std::get<2>(node.rest[2]));
}

TEST(ChildView, AChildRangeIsAsCapableAsItsFormsRanges)
{
    // Random access where every form's range has it: indexed, iterator arithmetic, sized.
    using Indexed = urd::child_range<int, std::vector<int>, std::span<const int>>;
    static_assert(std::ranges::random_access_range<Indexed> && std::ranges::sized_range<Indexed>);
    const auto values = std::vector<int> { 5, 6, 7 };
    const auto range = Indexed(Indexed::form_type(std::in_place_index<1>, std::span<const int>(values)));
    EXPECT_EQ(range[1], 6);
    EXPECT_EQ(*(range.begin() + 2), 7);
    EXPECT_EQ(range.end() - range.begin(), 3);
    EXPECT_EQ(range.size(), 3U);
    EXPECT_TRUE(range.begin() < range.end());

    // Forward only where one form's range is.
    using Listed = urd::child_range<int, std::vector<int>, std::list<int>>;
    static_assert(std::ranges::forward_range<Listed> && !std::ranges::random_access_range<Listed>);
    const auto listed = Listed(Listed::form_type(std::in_place_index<1>, std::list<int> { 1, 2 }));
    EXPECT_EQ(std::vector<int>(listed.begin(), listed.end()), (std::vector<int> { 1, 2 }));
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

// A pooled builder shared (urd::share): its handle in a slot of the holders, whose control blocks the holders pool too;
// the builder goes back to its pool with the last copy, and what the share keeps alive goes only after that.
TEST(Builder, ASharedHandleGivesItsRecordBackWithTheLastCopy)
{
    auto pool = LeafPool {};
    auto holders = urd::Store<urd::slots<urd::handle<LeafBuilder>>> {};
    const LeafBuilder* leaf = nullptr;
    auto back_before_keep = false;
    auto keep = std::shared_ptr<int>(new int(0),
                                     [&](const int* kept)
                                     {
                                         // The builder's slot is free again when what the share keeps goes.
                                         back_before_keep = pool.make_handle().get() == leaf;
                                         delete kept;
                                     });
    const auto kept = std::weak_ptr<int>(keep);
    {
        auto held = pool.make_handle();
        held->weight = 4;
        leaf = held.get();
        const auto shared = urd::share(holders, std::move(held), std::move(keep));
        const auto copy = shared;  // NOLINT(performance-unnecessary-copy-initialization): a copy of the share is what is read
        EXPECT_EQ(copy.get(), leaf);
        EXPECT_EQ(copy->weight, 4);
        EXPECT_FALSE(kept.expired());
    }
    EXPECT_TRUE(kept.expired());
    EXPECT_TRUE(back_before_keep);
    const auto again = pool.make_handle();
    EXPECT_EQ(again.get(), leaf);
    EXPECT_EQ(again->weight, 0);
}

// A builder and the record it interns to read the same content: they hash and compare alike by it (urd::hash_content,
// urd::equal_content), a child built in the one and interned in the other; another depth is other content.
TEST(Builder, HashesAndComparesByContentAsItsRecord)
{
    auto nodes = Nodes {};
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    nodes.leaves.push_back(stored_leaf(urd::index<Leaf>(1), 5));
    auto record = urd::data<Node> {};
    record.first = urd::index<Leaf>(1);
    record.rest.push_back(urd::index<Leaf>(0));
    record.depth = 4;

    auto pool = LeafPool {};
    auto node = NodeBuilder {};
    node.first = urd::index<Leaf>(1);
    urd::builder_of(node.rest.emplace_back(), pool).weight = 3;
    node.depth = 4;

    const auto built = urd::borrowed_view<Node, Nodes, NodeBuilder> { &node, &nodes };
    const auto stored = urd::borrowed_view<Node, Nodes> { &record, &nodes };
    EXPECT_EQ(urd::hash_content(built), urd::hash_content(stored));
    EXPECT_TRUE(urd::equal_content(built, stored));
    node.depth = 5;
    EXPECT_FALSE(urd::equal_content(built, stored));
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

namespace
{
/// A storage of pools of many kinds, as urd::ChecksOutBuilders names it: a builder of a kind it pools (here a leaf)
/// checked out of that kind's pool.
struct LeafStorage
{
    template<typename T>
        requires std::same_as<T, Leaf>
    [[nodiscard]] urd::built_child<T, Nodes> checkout()
    { return leaves.make_handle(); }

    LeafPool leaves;
};
}  // namespace

static_assert(urd::ChecksOutBuilders<LeafStorage, Leaf, Nodes>);
static_assert(!urd::ChecksOutBuilders<LeafStorage, Node, Nodes>);  // a kind it does not pool

TEST(Builder, AChildIsBuiltFromAStorageOfItsKind)
{
    auto storage = LeafStorage {};
    auto node = NodeBuilder {};
    urd::builder_of(node.first, storage).weight = 5;
    ASSERT_EQ(node.first.index(), 1U);
    EXPECT_EQ(std::get<1>(node.first)->weight, 5);
    EXPECT_EQ(&urd::builder_of(node.first, storage), std::get<1>(node.first).get());
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

namespace
{
struct Crate;
struct Shelf;
}  // namespace

/// A leaf read through a shelf, which keeps its leaves' records in a pool of its own.
template<>
struct urd::builder<Leaf, Shelf>
{
    urd::key<Leaf> index;
    int weight = 0;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"weight", &builder::weight>> {}; }
};

/// A crate's leaf is built as the leaf's builder, held in place: its record holds only the leaf's key, which the shelf
/// reads into it (made only through its context).
template<>
struct urd::built_form<urd::key<Leaf>, Shelf>
{
    using type = urd::builder<Leaf, Shelf>;
};

template<>
struct urd::data<Crate>
{
    urd::key<Leaf> leaf;
    int count = 0;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"leaf", &data::leaf>, urd::field<"count", &data::count>> {}; }
};

template<>
struct urd::builder<Crate, Shelf>
{
    urd::built_t<urd::key<Leaf>, Shelf> leaf;
    urd::built_t<int, Shelf> count = 0;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"leaf", &builder::leaf>, urd::field<"count", &builder::count>> {}; }
};

namespace
{
/// Leaves by their keys, each read into a builder in place (urd::UnpacksBuilt); counted.
struct Shelf
{
    std::vector<urd::data<Leaf>> leaves;

    void unpack_built(urd::key<Leaf> at, urd::builder<Leaf, Shelf>& built) const
    {
        ++unpacks;
        built.weight = leaves[at.value].weight;
    }

    mutable int unpacks = 0;
};
}  // namespace

// A builder held in place, of a record whose record holds only its key, is built from that key as its context reads the
// record into it (urd::UnpacksBuilt); the other fields as they are.
TEST(Builder, IsBuiltInPlaceFromAKeyItsContextUnpacks)
{
    static_assert(urd::UnpacksBuilt<Shelf, urd::key<Leaf>, urd::builder<Leaf, Shelf>>);
    auto shelf = Shelf {};
    shelf.leaves.push_back(stored_leaf(urd::index<Leaf>(0), 3));
    shelf.leaves.push_back(stored_leaf(urd::index<Leaf>(1), 5));
    auto record = urd::data<Crate> {};
    record.leaf = urd::key<Leaf>(1);
    record.count = 2;

    auto crate = urd::builder<Crate, Shelf> {};
    urd::build_from(urd::borrowed_view<Crate, Shelf> { &record, &shelf }, crate);
    EXPECT_EQ(crate.leaf.weight, 5);
    EXPECT_EQ(crate.count, 2);
    EXPECT_EQ(shelf.unpacks, 1);
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
    [[nodiscard]] decltype(auto) values() const { return self().template member<&urd::record_of<V>::values>(); }
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

// A record's flat list of move-only records (a record holding its records in place) is cloned element by element.
TEST(Builder, AFlatListOfMoveOnlyRecordsIsCloned)
{
    static_assert(!urd::CopiedAsIs<urd::flat_vector<urd::data<Node>>>);
    auto rest = urd::flat_vector<urd::index<Leaf>> {};
    rest.push_back(urd::index<Leaf>(2));
    auto nodes = urd::flat_vector<urd::data<Node>> {};
    nodes.emplace_back(urd::index<Leaf>(1), std::move(rest), 3);
    const auto copy = urd::clone(nodes);
    ASSERT_EQ(copy.size(), 1U);
    EXPECT_EQ(copy[0].first, nodes[0].first);
    EXPECT_TRUE(copy[0].rest == nodes[0].rest);
    EXPECT_EQ(copy[0].depth, 3);
}

// An owning view hands its record out (release): the record to keep or fill without the view.
TEST(Builder, AnOwningViewReleasesItsRecord)
{
    auto view = urd::owning_view<Leaf, Nodes, urd::data<Leaf>>(urd::data<Leaf>(7), nullptr);
    const auto record = std::move(view).release();
    EXPECT_EQ(record.weight, 7);
}
