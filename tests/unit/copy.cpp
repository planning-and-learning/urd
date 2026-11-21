// urd::copy (urd/elements/copy.hpp): a record of one layer written into another. A layer sees itself and the layers
// it lies over (their keys name the same records in it), so their records are its own already; a record of any other
// layer (a sibling) is copied, its children first, into the target.
#include "urd/elements/copy.hpp"

#include "urd/elements/builder.hpp"
#include "urd/elements/children.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/view.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/store.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

namespace
{
struct Leaf;
struct Node;
struct Pin;
class Layer;

/// A key of its own for a leaf (as a row's is its pool and its id), held as it is by a pin's builder.
struct LeafKey
{
    urd::index<Leaf> leaf;

    friend bool operator==(const LeafKey&, const LeafKey&) = default;
};
}  // namespace

template<>
struct urd::data<Leaf>
{
    urd::index<Leaf> index;
    int weight = 0;

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

    data() = default;
    data(const data& other) = delete;
    data& operator=(const data& other) = delete;
    data(data&& other) = default;
    data& operator=(data&& other) = default;
    ~data() = default;

    static constexpr auto fields() noexcept
    { return urd::Fields<urd::field<"first", &data::first>, urd::field<"rest", &data::rest>, urd::field<"depth", &data::depth>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::first, &data::rest, &data::depth> {}; }
};

template<>
struct urd::data<Pin>
{
    urd::index<Pin> index;
    LeafKey leaf;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"leaf", &data::leaf>> {}; }

    static constexpr auto identifying_members() noexcept { return urd::members_of(fields()); }
};

template<>
struct urd::builder<Leaf, Layer>
{
    urd::index<Leaf> index;
    urd::built_t<int, Layer> weight = 0;

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"weight", &builder::weight>> {}; }
};

template<>
struct urd::builder<Node, Layer>
{
    urd::index<Node> index;
    urd::built_t<urd::index<Leaf>, Layer> first;
    urd::built_t<urd::flat_vector<urd::index<Leaf>>, Layer> rest;
    urd::built_t<int, Layer> depth = 0;

    static constexpr auto fields() noexcept
    { return urd::Fields<urd::field<"first", &builder::first>, urd::field<"rest", &builder::rest>, urd::field<"depth", &builder::depth>> {}; }
};

template<>
struct urd::builder<Pin, Layer>
{
    urd::index<Pin> index;
    urd::built_t<LeafKey, Layer> leaf;  // a key held as it is: the builder holds no child of it

    static constexpr auto fields() noexcept { return urd::Fields<urd::field<"leaf", &builder::leaf>> {}; }
};

template<typename V>
struct urd::view_mixin<Leaf, V>
{
private:
    [[nodiscard]] const V& self() const noexcept { return static_cast<const V&>(*this); }

public:
    [[nodiscard]] decltype(auto) get_weight() const { return self().template member<&urd::record_of<V>::weight>(); }
};

/// A leaf's key reads as the leaf, through its context; it is written from that view.
template<>
struct urd::resolution<LeafKey>
{
    template<typename C>
    static auto of(LeafKey key, const C& context)
    { return context.view(key.leaf); }

    template<std::same_as<LeafKey> R>
    static LeafKey stored(const R& readout) noexcept
    { return readout; }

    template<urd::IndexedBy<urd::index<Leaf>> V>
    static LeafKey stored(const V& view) noexcept
    { return LeafKey { view.index() }; }
};

namespace
{
/// A layer of leaves and nodes over a parent's (their indices continue the parent's): the context of its views, which
/// sees itself and the layers it lies over.
class Layer
{
public:
    explicit Layer(const Layer* parent = nullptr, int realm = 0) :
        m_parent(parent),
        m_realm(realm),
        m_leaf_base(parent == nullptr ? 0 : parent->m_leaf_base + parent->m_leaves.size()),
        m_node_base(parent == nullptr ? 0 : parent->m_node_base + parent->m_nodes.size())
    {
    }

    [[nodiscard]] bool sees(const Layer& source) const noexcept
    {
        for (const Layer* layer = this; layer != nullptr; layer = layer->m_parent)
            if (layer == &source)
                return true;
        return false;
    }

    /// Whether this layer may hold copies of `source`'s records: of its realm (as a repository's nested ones must see
    /// the source's), else std::invalid_argument.
    void check_compatible(const Layer& source) const
    {
        if (source.m_realm != m_realm)
            throw std::invalid_argument("Layer: another realm");
    }

    const urd::data<Leaf>& operator[](urd::index<Leaf> at) const
    {
        const auto& layer = leaf_owner(at);
        return layer.m_leaves[at.value - layer.m_leaf_base];
    }

    const urd::data<Node>& operator[](urd::index<Node> at) const
    {
        const auto& layer = node_owner(at);
        return layer.m_nodes[at.value - layer.m_node_base];
    }

    [[nodiscard]] urd::index_view<Leaf, Layer> view(urd::index<Leaf> at) const { return { at, &leaf_owner(at) }; }

    [[nodiscard]] urd::index_view<Node, Layer> view(urd::index<Node> at) const { return { at, &node_owner(at) }; }

    template<typename Storage>
    std::pair<urd::index_view<Leaf, Layer>, bool> insert(const urd::builder<Leaf, Layer>& built, Storage& /*storage*/)
    {
        for (const Layer* layer = this; layer != nullptr; layer = layer->m_parent)
            for (const auto& leaf : layer->m_leaves)
                if (leaf.weight == built.weight)
                    return { view(leaf.index), false };
        auto& leaf = m_leaves.emplace_back();
        leaf.index = urd::index<Leaf>(static_cast<std::uint32_t>(m_leaf_base + m_leaves.size() - 1));
        leaf.weight = built.weight;
        return { view(leaf.index), true };
    }

    template<typename Storage>
    std::pair<urd::index_view<Node, Layer>, bool> insert(const urd::builder<Node, Layer>& built, Storage& /*storage*/)
    {
        auto& node = m_nodes.emplace_back();
        node.index = urd::index<Node>(static_cast<std::uint32_t>(m_node_base + m_nodes.size() - 1));
        node.first = std::get<0>(built.first);
        for (const auto& leaf : built.rest)
            node.rest.push_back(std::get<0>(leaf));
        node.depth = built.depth;
        return { view(node.index), true };
    }

    [[nodiscard]] std::size_t num_leaves() const noexcept { return m_leaves.size(); }

    [[nodiscard]] std::size_t num_nodes() const noexcept { return m_nodes.size(); }

private:
    [[nodiscard]] const Layer& leaf_owner(urd::index<Leaf> at) const
    { return at.value >= m_leaf_base || m_parent == nullptr ? *this : m_parent->leaf_owner(at); }

    [[nodiscard]] const Layer& node_owner(urd::index<Node> at) const
    { return at.value >= m_node_base || m_parent == nullptr ? *this : m_parent->node_owner(at); }

    const Layer* m_parent;
    int m_realm;
    std::size_t m_leaf_base;
    std::size_t m_node_base;
    std::vector<urd::data<Leaf>> m_leaves;
    std::vector<urd::data<Node>> m_nodes;
};

/// The builders a copy draws.
struct Storage
{
    urd::Store<urd::slots<urd::builder<Leaf, Layer>>> leaves;
    urd::Store<urd::slots<urd::builder<Node, Layer>>> nodes;

    template<typename T>
    auto checkout()
    {
        if constexpr (std::same_as<T, Leaf>)
            return leaves.make_handle();
        else
            return nodes.make_handle();
    }
};

static_assert(urd::Sees<Layer> && urd::ChecksCompatibility<Layer>);
static_assert(!urd::Sees<Storage>);
static_assert(std::same_as<urd::view_context_t<urd::index_view<Leaf, Layer>>, Layer>);  // the context a view reads through
static_assert(std::same_as<urd::view_context_t<const urd::borrowed_view<Pin, Layer>&>, Layer>);

urd::index<Leaf> leaf_in(Layer& layer, Storage& storage, int weight)
{
    auto built = storage.checkout<Leaf>();
    built->weight = weight;
    return layer.insert(*built, storage).first.index();
}

/// A node of `layer` over leaves `first` and `rest`.
urd::index<Node> node_in(Layer& layer, Storage& storage, urd::index<Leaf> first, urd::index<Leaf> rest)
{
    auto built = storage.checkout<Node>();
    built->first = first;
    built->rest.emplace_back(rest);
    built->depth = 2;
    return layer.insert(*built, storage).first.index();
}
}  // namespace

TEST(Copy, ARecordOfALayerItSeesIsItsOwn)
{
    auto storage = Storage {};
    auto root = Layer();
    const auto shared = leaf_in(root, storage, 1);
    const auto node = node_in(root, storage, shared, shared);
    auto layer = Layer(&root);

    EXPECT_TRUE(layer.sees(root));
    EXPECT_FALSE(root.sees(layer));
    EXPECT_EQ(urd::copy(root.view(node), layer, storage), node);
    EXPECT_EQ(urd::copy(layer.view(node), root, storage), node);  // a view of a record is its owner's
    EXPECT_EQ(layer.num_nodes(), 0U);
}

TEST(Copy, ARecordOfASiblingIsCopiedChildrenFirst)
{
    auto storage = Storage {};
    auto root = Layer();
    const auto shared = leaf_in(root, storage, 1);
    auto source = Layer(&root);
    auto target = Layer(&root);
    const auto own = leaf_in(source, storage, 7);
    const auto node = node_in(source, storage, own, shared);

    EXPECT_FALSE(target.sees(source));
    const auto copied = urd::copy(source.view(node), target, storage);
    EXPECT_EQ(target.num_nodes(), 1U);
    EXPECT_EQ(target.num_leaves(), 1U);  // its own leaf copied, the shared one seen
    EXPECT_EQ(target[copied].rest[0], shared);
    EXPECT_EQ(target.view(target[copied].first).get_weight(), 7);
    EXPECT_EQ(target[copied].depth, 2);
}

// A record of an incompatible context is not copied: the target's check throws (urd::ChecksCompatibility).
TEST(Copy, ARecordOfAnIncompatibleContextThrows)
{
    auto storage = Storage {};
    auto source = Layer(nullptr, 1);
    auto target = Layer(nullptr, 2);
    const auto leaf = leaf_in(source, storage, 3);
    EXPECT_FALSE(target.sees(source));
    EXPECT_THROW((void) urd::copy(source.view(leaf), target, storage), std::invalid_argument);
    EXPECT_EQ(target.num_leaves(), 0U);
}

// A key a builder holds as it is (urd::detail::held_key) is built as a child: from the view f gives for what it names.
TEST(Copy, AKeyHeldAsItIsIsBuiltAsAChild)
{
    auto storage = Storage {};
    auto root = Layer();
    const auto leaf = leaf_in(root, storage, 4);
    auto record = urd::data<Pin> {};
    record.leaf = LeafKey { leaf };
    const auto source = urd::borrowed_view<Pin, Layer> { &record, &root };

    auto pin = urd::builder<Pin, Layer> {};
    auto named = 0;
    urd::build_from(source,
                    pin,
                    [&named](const auto& view)
                    {
                        named = view.get_weight();
                        return view;
                    });
    EXPECT_EQ(named, 4);
    EXPECT_EQ(pin.leaf, LeafKey { leaf });
}
