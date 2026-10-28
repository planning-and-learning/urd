// Copied from yggdrasil (tests/unit/containers/tree_vector_set.cpp); names moved from ygg to urd::repr, and the sets
// hold rows of a vector type, with yggdrasil's default arity 2 spelled out. The concurrent test comes with the concurrent
// node tables.

#include "urd/repr/tree_vector.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace urd::repr::tests
{
namespace
{

struct CustomLeaf
{
    std::uint32_t first;
    std::uint32_t second;

    friend bool operator==(const CustomLeaf&, const CustomLeaf&) = default;
    static constexpr auto identifying_members() noexcept { return urd::Members<&CustomLeaf::first, &CustomLeaf::second> {}; }
};

TEST(CommonTreeVectorSetTest, RoundTripsAndSharesTrees)
{
    {
        auto singleton_set = TreeVectorSet<std::vector<std::uint32_t>, 2> {};
        constexpr auto singleton = std::array<std::uint32_t, 1> { 7 };
        const auto singleton_index = singleton_set.insert(singleton);
        auto singleton_output = std::array<std::uint32_t, 1> {};
        singleton_set.read(singleton_index, singleton_output);
        EXPECT_EQ(singleton_output, singleton);
        EXPECT_EQ(singleton_index.root, singleton.front());
        EXPECT_EQ(singleton_set.num_leaves(), 0);
        EXPECT_EQ(singleton_set.num_nodes(), 0);
    }

    auto set = TreeVectorSet<std::vector<std::uint32_t>, 2> {};

    const auto empty = set.insert(std::span<const std::uint32_t> {});
    EXPECT_EQ(empty, TreeHandle {});
    EXPECT_TRUE(set.empty());
    set.read(empty, std::span<std::uint32_t> {});

    constexpr auto repeated = std::array<std::uint32_t, 4> { 1, 2, 1, 2 };
    const auto repeated_index = set.insert(repeated);
    EXPECT_EQ(repeated_index.size, repeated.size());
    EXPECT_EQ(set.num_leaves(), 0);
    EXPECT_EQ(set.num_nodes(), 2);

    auto output = std::array<std::uint32_t, repeated.size()> {};
    set.read(repeated_index, output);
    EXPECT_EQ(output, repeated);

    constexpr auto shared_subtree = std::array<std::uint32_t, 2> { 1, 2 };
    const auto shared_subtree_index = set.insert(shared_subtree);
    auto shared_subtree_output = std::array<std::uint32_t, shared_subtree.size()> {};
    set.read(shared_subtree_index, shared_subtree_output);
    EXPECT_EQ(shared_subtree_output, shared_subtree);
    EXPECT_EQ(set.num_leaves(), 0);
    EXPECT_EQ(set.num_nodes(), 2);

    EXPECT_EQ(set.insert(repeated), repeated_index);
    EXPECT_EQ(set.insert(shared_subtree), shared_subtree_index);
    EXPECT_THROW(set.read(repeated_index, std::span<std::uint32_t>(output).first(3)), std::invalid_argument);

    set.clear();
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.num_leaves(), 0);
    EXPECT_EQ(set.num_nodes(), 0);
}

TEST(CommonTreeVectorSetTest, SupportsDifferentLeafTypesAndLengths)
{
    auto float_set = TreeVectorSet<std::vector<double>, 2> {};
    constexpr auto floats = std::array<double, 5> { 1.5, 2.5, 3.5, 4.5, 5.5 };
    const auto float_index = float_set.insert(floats);
    auto float_output = std::array<double, floats.size()> {};
    float_set.read(float_index, float_output);
    EXPECT_EQ(float_output, floats);
    EXPECT_EQ(float_set.num_leaves(), floats.size());

    auto custom_set = TreeVectorSet<std::vector<CustomLeaf>, 2> {};
    constexpr auto custom = std::array<CustomLeaf, 3> { CustomLeaf { 1, 2 }, CustomLeaf { 3, 4 }, CustomLeaf { 5, 6 } };
    const auto custom_index = custom_set.insert(custom);
    auto custom_output = std::array<CustomLeaf, custom.size()> {};
    custom_set.read(custom_index, custom_output);
    EXPECT_EQ(custom_output, custom);
    EXPECT_EQ(custom_set.num_leaves(), custom.size());
}

TEST(CommonTreeVectorSetTest, StoresUintLeavesInline)
{
    auto set = TreeVectorSet<std::vector<std::uint32_t>, 2> {};

    // Values coinciding with node indices and the full value range must round-trip.
    constexpr auto max = std::numeric_limits<std::uint32_t>::max();
    constexpr auto vectors = std::array {
        std::array<std::uint32_t, 5> { 0, 1, 0, 1, 2 },
        std::array<std::uint32_t, 5> { 0, max, 0, max, 0 },
        std::array<std::uint32_t, 5> { 1, 0, 2, 1, 0 },
    };
    auto indices = std::array<TreeHandle, vectors.size()> {};
    for (std::size_t i = 0; i < vectors.size(); ++i)
    {
        indices[i] = set.insert(vectors[i]);
        auto output = std::array<std::uint32_t, 5> {};
        set.read(indices[i], output);
        EXPECT_EQ(output, vectors[i]);
    }
    for (std::size_t i = 0; i < vectors.size(); ++i)
    {
        EXPECT_EQ(set.insert(vectors[i]), indices[i]);
        for (std::size_t j = i + 1; j < vectors.size(); ++j)
            EXPECT_NE(indices[i], indices[j]);
    }

    for (const auto value : { std::uint32_t { 0 }, std::uint32_t { 1 }, max })
    {
        const auto single = std::array<std::uint32_t, 1> { value };
        const auto index = set.insert(single);
        EXPECT_EQ(index, (TreeHandle { value, 1 }));
        auto output = std::array<std::uint32_t, 1> {};
        set.read(index, output);
        EXPECT_EQ(output, single);
    }

    EXPECT_EQ(set.num_leaves(), 0);
}

TEST(CommonTreeVectorSetTest, WritesMatchFullInsert)
{
    const auto expect_writes = [](auto set, auto values, auto writes)
    {
        using Value = typename decltype(values)::value_type;
        const auto base = set.insert(values);
        const auto nodes_before = set.num_nodes();
        const auto written = set.insert(base, std::span(writes));
        for (const auto& write : writes)
            values[write.position] = write.value;
        auto output = std::vector<Value>(values.size());
        set.read(written, output);
        EXPECT_EQ(output, values);
        EXPECT_EQ(written, set.insert(values));
        EXPECT_LE(set.num_nodes() - nodes_before, writes.size() * static_cast<std::size_t>(std::bit_width(values.size())));
    };

    // Inline leaves, a value equal to its position's old value, and the last position.
    expect_writes(TreeVectorSet<std::vector<std::uint32_t>, 2> {},
                  std::vector<std::uint32_t> { 1, 2, 3, 4, 5, 6, 7 },
                  std::vector<urd::Write<std::uint32_t>> { { 0, 9 }, { 3, 4 }, { 6, 0 } });
    // Leaf table leaves.
    expect_writes(TreeVectorSet<std::vector<double>, 2> {},
                  std::vector<double> { 1.5, 2.5, 3.5, 4.5, 5.5 },
                  std::vector<urd::Write<double>> { { 1, -2.0 }, { 4, 8.25 } });
    // No writes and single-value vectors.
    expect_writes(TreeVectorSet<std::vector<std::uint32_t>, 2> {}, std::vector<std::uint32_t> { 1, 2, 3 }, std::vector<urd::Write<std::uint32_t>> {});
    expect_writes(TreeVectorSet<std::vector<double>, 2> {}, std::vector<double> { 1.0 }, std::vector<urd::Write<double>> { { 0, 3.0 } });
}

/// Round trips, canonical trees and write deltas for vectors whose sizes leave unused child slots on the right edge.
template<typename Value, std::size_t Arity>
void expect_arity_vectors()
{
    SCOPED_TRACE(testing::Message() << "Arity " << Arity << ", " << sizeof(Value) << "-byte values");
    auto rng = std::mt19937_64 { Arity };
    auto set = TreeVectorSet<std::vector<Value>, Arity> {};
    static_assert(decltype(set)::arity == Arity);

    auto sizes = std::vector<std::uint32_t> { 1, 2 };
    for (std::uint32_t power = Arity; power <= Arity * Arity * Arity; power *= Arity)
        for (const auto size : { power - 1, power, power + 1 })
            sizes.push_back(size);

    for (const auto size : sizes)
    {
        // Few distinct values, so that subtrees are shared.
        auto values = std::vector<Value>(size);
        for (auto& value : values)
            value = Value(rng() % 4);

        const auto index = set.insert(values);
        auto output = std::vector<Value>(size);
        set.read(index, output);
        EXPECT_EQ(output, values);
        EXPECT_EQ(set.insert(values), index);

        auto writes = std::vector<urd::Write<Value>> {};
        for (std::uint32_t position = 0; position < size; ++position)
            if (rng() % 5 == 0)
                writes.push_back(urd::Write<Value> { position, Value(rng() % 7) });
        const auto nodes_before = set.num_nodes();
        const auto written = set.insert(index, std::span<const urd::Write<Value>>(writes));
        for (const auto& write : writes)
            values[write.position] = write.value;
        set.read(written, output);
        EXPECT_EQ(output, values);
        EXPECT_EQ(written, set.insert(values));
        const auto bits = static_cast<std::size_t>(std::countr_zero(Arity));
        const auto levels = (static_cast<std::size_t>(std::bit_width(size)) + bits - 1) / bits + 1;
        EXPECT_LE(set.num_nodes() - nodes_before, writes.size() * levels);
    }
}

TEST(CommonTreeVectorSetTest, SupportsArities)
{
    expect_arity_vectors<std::uint32_t, 2>();
    expect_arity_vectors<std::uint32_t, 4>();
    expect_arity_vectors<std::uint32_t, 8>();
    expect_arity_vectors<double, 4>();  // Leaf table leaves.
    expect_arity_vectors<double, 8>();
}

}  // namespace
}  // namespace urd::repr::tests
