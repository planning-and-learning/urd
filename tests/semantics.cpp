// urd::Hash, urd::EqualTo and urd::Less keep the laws the containers rely on, for every type they cover:
// equal values hash equally, Less is a strict weak order, and two values are equivalent under Less exactly
// when EqualTo calls them equal. One typed test per law, over random values from a small domain, so that
// equal values occur.

#include "urd/elements/equal_to.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/less.hpp"
#include "urd/repr/containers/bit_rows.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/flat_map.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/sequences/segmented.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/flat_set.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/values/std_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/store.hpp"

#include <array>
#include <cista/containers/optional.h>
#include <cista/containers/pair.h>
#include <cista/containers/string.h>
#include <cista/containers/variant.h>
#include <cista/containers/vector.h>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{

enum class Colour : std::uint8_t
{
    red,
    green,
    blue,
};

/// A record that names its identity: Hash, EqualTo and Less follow from it, with nothing in namespace std.
struct Atom
{
    int predicate = 0;
    std::vector<int> objects;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Atom::predicate, &Atom::objects> {}; }
};

constexpr int domain = 4;  // small, so that random values repeat

template<typename T>
T random_value(std::mt19937& random)
{
    auto pick = std::uniform_int_distribution<int>(0, domain - 1);
    if constexpr (std::same_as<T, int>)
        return pick(random);
    else if constexpr (std::same_as<T, double>)
    {
        const int drawn = pick(random);
        return drawn == 0 ? std::numeric_limits<double>::quiet_NaN() : drawn == 1 ? -0.0 : static_cast<double>(drawn) / 2;
    }
    else if constexpr (std::same_as<T, Colour>)
        return static_cast<Colour>(pick(random) % 3);
    else if constexpr (std::same_as<T, std::string>)
        return std::string(static_cast<std::size_t>(pick(random)), 'a');
    else if constexpr (std::same_as<T, std::vector<int>>)
    {
        auto result = std::vector<int>(static_cast<std::size_t>(pick(random) % 3));
        for (auto& element : result)
            element = pick(random);
        return result;
    }
    else if constexpr (std::same_as<T, std::optional<int>>)
        return pick(random) == 0 ? std::optional<int>() : std::optional<int>(pick(random));
    else if constexpr (std::same_as<T, std::pair<int, std::string>>)
        return { random_value<int>(random), random_value<std::string>(random) };
    else if constexpr (std::same_as<T, std::tuple<int, double>>)
        return { random_value<int>(random), random_value<double>(random) };
    else if constexpr (std::same_as<T, std::array<int, 2>>)
        return { random_value<int>(random), random_value<int>(random) };
    else if constexpr (urd::is_store<T>)
    {
        const auto elements = random_value<std::vector<int>>(random);
        return T(elements.begin(), elements.end());
    }
    else
        return Atom { random_value<int>(random), random_value<std::vector<int>>(random) };
}

using Types = testing::Types<int,
                             double,
                             Colour,
                             std::string,
                             std::vector<int>,
                             std::optional<int>,
                             std::pair<int, std::string>,
                             std::tuple<int, double>,
                             std::array<int, 2>,
                             Atom,
                             // Stores whose contents fix their order: a sorted set, a bitset.
                             urd::Store<urd::set<int>, urd::sorted, urd::frozen>,
                             urd::Store<urd::set<int>, urd::dense, urd::sorted>>;

template<typename T>
class Semantics : public testing::Test
{
};
TYPED_TEST_SUITE(Semantics, Types);

constexpr int samples = 2000;

}  // namespace

TYPED_TEST(Semantics, EqualValuesHashEqually)
{
    auto random = std::mt19937(1);
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto lhs = random_value<TypeParam>(random);
        const auto rhs = random_value<TypeParam>(random);
        if (urd::EqualTo<TypeParam> {}(lhs, rhs))
        {
            EXPECT_EQ(urd::Hash<TypeParam> {}(lhs), urd::Hash<TypeParam> {}(rhs));
        }
    }
}

TYPED_TEST(Semantics, EqualToIsAnEquivalence)
{
    auto random = std::mt19937(2);
    const auto equal = urd::EqualTo<TypeParam> {};
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto a = random_value<TypeParam>(random);
        const auto b = random_value<TypeParam>(random);
        const auto c = random_value<TypeParam>(random);
        EXPECT_TRUE(equal(a, a));
        EXPECT_EQ(equal(a, b), equal(b, a));
        if (equal(a, b) && equal(b, c))
        {
            EXPECT_TRUE(equal(a, c));
        }
    }
}

TYPED_TEST(Semantics, LessIsAStrictWeakOrderAgreeingWithEqualTo)
{
    auto random = std::mt19937(3);
    const auto less = urd::Less<TypeParam> {};
    const auto equal = urd::EqualTo<TypeParam> {};
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto a = random_value<TypeParam>(random);
        const auto b = random_value<TypeParam>(random);
        const auto c = random_value<TypeParam>(random);
        EXPECT_FALSE(less(a, a));
        if (less(a, b))
        {
            EXPECT_FALSE(less(b, a));
        }
        if (less(a, b) && less(b, c))
        {
            EXPECT_TRUE(less(a, c));
        }
        EXPECT_EQ(!less(a, b) && !less(b, a), equal(a, b));
    }
}

namespace
{

/// Identified by its members: a lookup may give them (by value) in place of the record.
struct Named
{
    std::string name;
    int arity = 0;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Named::name, &Named::arity> {}; }
};

}  // namespace

// The derived order predicates agree with Less; an identified type is looked up by its identity.
TEST(Predicates, DerivedOrdersAndTransparentIdentityAgreeWithLess)
{
    static_assert(urd::Identifiable<Named> && urd::identity_of<std::tuple<const std::string&, const int&>, Named>);
    EXPECT_TRUE(urd::Greater<int> {}(3, 2));
    EXPECT_TRUE(urd::GreaterEqual<int> {}(2, 2));
    EXPECT_TRUE(urd::LessEqual<> {}(2, 3));
    EXPECT_EQ(urd::ThreeWayCompare<double> {}(1.0, 2.0), std::strong_ordering::less);
    EXPECT_TRUE(urd::Less<> {}(std::string("a"), std::string("b")));
    EXPECT_TRUE(urd::EqualTo<> {}(4, 4));
    EXPECT_EQ(urd::hash_combine(1, 2), urd::hash_combine(1, 2));
    EXPECT_NE(urd::hash_combine(1, 2), urd::hash_combine(2, 1));

    const auto record = Named { "at", 2 };
    const auto name = std::string("at");
    const auto arity = 2;
    const auto other_arity = 3;
    const auto identity = std::tie(name, arity);
    EXPECT_EQ(urd::Hash<Named> {}(record), urd::Hash<Named> {}(identity));
    EXPECT_TRUE(urd::EqualTo<Named> {}(record, identity));
    EXPECT_TRUE(urd::EqualTo<Named> {}(identity, record));
    EXPECT_FALSE(urd::Less<Named> {}(record, identity));
    EXPECT_TRUE(urd::Less<Named> {}(record, std::tie(name, other_arity)));
    EXPECT_TRUE(urd::equal_range(std::vector<int> { 1, 2 }, std::array<int, 2> { 1, 2 }));
    EXPECT_TRUE(urd::less_range(std::vector<int> { 1, 2 }, std::vector<int> { 1, 3 }));
}

namespace
{

struct Unsupported
{
};

struct ThrowingLess
{
    friend bool operator<(const ThrowingLess&, const ThrowingLess&) { throw std::runtime_error("comparison"); }
};

/// A contiguous range whose size() or data() throws.
struct ThrowingDataRange
{
    std::span<const unsigned> values;
    bool throw_on_size;

    [[nodiscard]] auto begin() const noexcept { return values.begin(); }
    [[nodiscard]] auto end() const noexcept { return values.end(); }
    [[nodiscard]] std::size_t size() const
    {
        if (throw_on_size)
            throw std::runtime_error("range size");
        return values.size();
    }
    [[nodiscard]] const unsigned* data() const
    {
        if (!throw_on_size)
            throw std::runtime_error("range data");
        return values.data();
    }
};

struct ValidHash
{
    urd::hash_t operator()(int) const { return 0; }
};
struct InvalidHash
{
    bool operator()(int) const { return true; }
};
struct ValidPredicate
{
    bool operator()(int, int) const { return true; }
};
struct InvalidPredicate
{
    int operator()(int, int) const { return 0; }
};

template<typename T>
concept OrderedByAllPredicates = urd::LessFor<urd::Less<T>, T> && urd::LessFor<urd::LessEqual<T>, T> && urd::LessFor<urd::Greater<T>, T>
                                 && urd::LessFor<urd::GreaterEqual<T>, T> && urd::Hashable<T> && urd::EqualityComparableByEqualTo<T>
                                 && requires(const T& lhs, const T& rhs) {
                                        { urd::ThreeWayCompare<T> {}(lhs, rhs) } -> std::same_as<std::strong_ordering>;
                                    };

using Segmented = urd::Store<urd::sequence<int>, urd::stable_refs>;
using SortedSet = urd::Store<urd::set<int>, urd::sorted>;
using SortedMap = urd::Store<urd::map<int, int>, urd::sorted>;

}  // namespace

// Which types Hash, EqualTo and Less cover, and that a type they do not cover makes nothing containing it covered.
TEST(Predicates, CapabilitiesCoverTheVocabularyAndRejectUnsupportedValues)
{
    static_assert(OrderedByAllPredicates<double> && OrderedByAllPredicates<Named> && OrderedByAllPredicates<std::array<int, 2>>);
    static_assert(OrderedByAllPredicates<urd::Store<urd::sequence<int>>> && OrderedByAllPredicates<Segmented>);
    static_assert(OrderedByAllPredicates<SortedSet> && OrderedByAllPredicates<SortedMap>);
    static_assert(OrderedByAllPredicates<std::pair<int, int>> && OrderedByAllPredicates<std::tuple<int, int>>);
    static_assert(OrderedByAllPredicates<std::variant<int, unsigned>> && OrderedByAllPredicates<std::optional<int>>);
    static_assert(OrderedByAllPredicates<std::reference_wrapper<int>> && OrderedByAllPredicates<std::span<const int>>);
    static_assert(OrderedByAllPredicates<cista::offset::string> && OrderedByAllPredicates<cista::offset::vector<int>>);
    static_assert(OrderedByAllPredicates<cista::optional<int>> && OrderedByAllPredicates<cista::offset::variant<int, unsigned>>);
    static_assert(urd::OrderedByLess<std::set<int>> && urd::OrderedByLess<std::map<int, int>>);

    static_assert(urd::HashFor<ValidHash, int> && !urd::HashFor<InvalidHash, int>);
    static_assert(urd::EqualToFor<ValidPredicate, int> && !urd::EqualToFor<InvalidPredicate, int>);
    static_assert(urd::LessFor<ValidPredicate, int> && !urd::LessFor<InvalidPredicate, int>);
    static_assert(!urd::Identifiable<Unsupported>);

    const auto rejected = []<typename T>() { return !urd::Hashable<T> && !urd::EqualityComparableByEqualTo<T> && !urd::OrderedByLess<T>; };
    static_assert(rejected.operator()<Unsupported>());
    static_assert(rejected.operator()<std::array<Unsupported, 1>>());
    static_assert(rejected.operator()<urd::Store<urd::sequence<Unsupported>>>());
    static_assert(rejected.operator()<urd::Store<urd::sequence<Unsupported>, urd::stable_refs>>());
    static_assert(rejected.operator()<std::pair<int, Unsupported>>());
    static_assert(rejected.operator()<std::tuple<int, Unsupported>>());
    static_assert(rejected.operator()<std::optional<Unsupported>>());
    static_assert(rejected.operator()<std::variant<int, Unsupported>>());
    static_assert(rejected.operator()<std::span<const Unsupported>>());
    static_assert(rejected.operator()<std::reference_wrapper<Unsupported>>());
    static_assert(rejected.operator()<cista::offset::vector<Unsupported>>());
    static_assert(rejected.operator()<cista::pair<int, Unsupported>>());
    static_assert(rejected.operator()<cista::optional<Unsupported>>());
    static_assert(rejected.operator()<cista::offset::variant<int, Unsupported>>());
    static_assert(!urd::HashFor<urd::Hash<>, Unsupported> && !urd::EqualToFor<urd::EqualTo<>, Unsupported> && !urd::LessFor<urd::Less<>, Unsupported>);
    static_assert(!urd::LessFor<urd::LessEqual<Unsupported>, Unsupported> && !urd::LessFor<urd::Greater<Unsupported>, Unsupported>
                  && !urd::LessFor<urd::GreaterEqual<Unsupported>, Unsupported>);
    // Addresses and long double have no portable hash.
    static_assert(!urd::Hashable<int*> && !urd::Hashable<std::shared_ptr<int>> && !urd::Hashable<std::unique_ptr<int>>
                  && !urd::Hashable<std::weak_ptr<int>> && !urd::Hashable<long double>);
}

// Golden values: scalars hash to themselves, bytes to seed-zero xxHash64, the same with every library and compiler.
TEST(Hashing, ValuesMatchTheDefinedAlgorithms)
{
    static_assert(std::same_as<decltype(urd::Hash<int> {}(42)), urd::hash_t> && sizeof(urd::hash_t) == 8);
    EXPECT_EQ(urd::Hash<int> {}(42), 0x2aULL);
    EXPECT_EQ(urd::Hash<int> {}(-1), 0xffffffffffffffffULL);
    EXPECT_EQ(urd::Hash<double> {}(1.5), 0x3ff8000000000000ULL);
    EXPECT_EQ(urd::Hash<float> {}(1.5F), 0x3fc00000ULL);
    EXPECT_EQ(urd::Hash<double> {}(-0.0), urd::Hash<double> {}(0.0));
    EXPECT_EQ(urd::Hash<double> {}(std::numeric_limits<double>::quiet_NaN()), urd::Hash<double> {}(std::numeric_limits<double>::signaling_NaN()));
    EXPECT_NE(urd::Hash<double> {}(std::numeric_limits<double>::quiet_NaN()), urd::Hash<double> {}(0.0));
    EXPECT_EQ(urd::Hash<std::string> {}("yggdrasil"), 0x79adda5dd5464cb6ULL);
    EXPECT_EQ(urd::Hash<std::string> {}("yggdrasil"), urd::Hash<std::string_view> {}("yggdrasil"));
    EXPECT_EQ(urd::Hash<std::string> {}(""), 0xef46db3751d8e999ULL);

    alignas(std::uint64_t) constexpr std::array<char, 17> text { '_', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    EXPECT_EQ(urd::Hash<std::string_view> {}(std::string_view(text.data() + 1, 16)), 0x5c5b90c34e376d0bULL);  // unaligned

    auto bytes = std::array<unsigned char, 66> {};
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<unsigned char>(i * 37);
    const auto cases = std::array<std::pair<std::size_t, urd::hash_t>, 15> { {
        { 0, 0xef46db3751d8e999ULL },  { 1, 0x89ac406afd6cd3b7ULL },  { 3, 0x614123b469e8f204ULL },  { 4, 0x0b0552d04906db7bULL },
        { 7, 0x547bf259c8d24ca9ULL },  { 8, 0xfd161ba5079f032fULL },  { 15, 0x280ecabe993ce430ULL }, { 16, 0x1bb38a0220bb17b3ULL },
        { 17, 0xb4ff4a6c0661ca93ULL }, { 31, 0xbd93c8cc6c5f4d8eULL }, { 32, 0x7fd19ac913684e8aULL }, { 33, 0xfb548bf9f10abcd1ULL },
        { 63, 0x70781206707beec8ULL }, { 64, 0x6892b7f3d78396eeULL }, { 65, 0x4948c637675f5150ULL },
    } };
    EXPECT_EQ(urd::hashing::hash_bytes(nullptr, 0), 0xef46db3751d8e999ULL);
    for (const auto& [size, expected] : cases)
    {
        SCOPED_TRACE(size);
        EXPECT_EQ(urd::hashing::hash_bytes(bytes.data() + 1, size), expected);
    }
}

// A range of byte-hashed values hashes as its bytes, whatever holds them; other values hash element by element.
TEST(Hashing, RangesHashTheirValuesWhateverHoldsThem)
{
    const auto check = []<typename T>()
    {
        for (const auto size : { 0U, 1U, 7U, 8U, 17U, 65U, 1000U })
        {
            SCOPED_TRACE(size);
            auto values = urd::Store<urd::sequence<T>, urd::contiguous> {};
            auto cista_values = cista::offset::vector<T> {};
            auto segmented = urd::Store<urd::sequence<T>, urd::stable_refs> {};
            for (auto i = 0U; i < size; ++i)
            {
                const auto value = static_cast<T>(static_cast<int>(i % 251) - 125);
                values.push_back(value);
                cista_values.emplace_back(value);
                segmented.push_back(value);
            }
            const auto expected = urd::hashing::hash_bytes(values.data(), values.size() * sizeof(T));
            EXPECT_EQ(urd::Hash<> {}(values), expected);
            EXPECT_EQ(urd::Hash<> {}(std::span<const T>(values)), expected);
            EXPECT_EQ(urd::Hash<> {}(cista_values), expected);
            EXPECT_EQ(urd::Hash<> {}(segmented), expected);
            EXPECT_EQ(urd::hash_range(values | std::views::transform([](T value) { return value; })), expected);
            if constexpr (std::unsigned_integral<T>)
            {
                auto rows = urd::repr::BitRows<T, false>(urd::bit_width { static_cast<std::uint8_t>(std::numeric_limits<T>::digits) });
                rows.push_back(std::span<const T>(values));
                EXPECT_EQ(urd::hash_range(rows[0]), expected);
            }
        }
    };
    enum class Code : std::uint16_t
    {
    };
    check.operator()<std::uint8_t>();
    check.operator()<std::uint64_t>();
    check.operator()<std::int16_t>();
    check.operator()<std::byte>();
    check.operator()<Code>();

    auto fixed = std::array<std::uint8_t, 4> { 0, 1, 127, 255 };
    EXPECT_EQ(urd::Hash<> {}(std::span<std::uint8_t, 4>(fixed)), urd::hashing::hash_bytes(fixed.data(), fixed.size()));
    const auto ints = urd::Store<urd::sequence<int>> { 1, 2, 3, 4 };
    auto even_squares = ints | std::views::filter([](int value) { return value % 2 == 0; }) | std::views::transform([](int value) { return value * value; });
    EXPECT_EQ(urd::hash_range(even_squares), urd::hash_range(urd::Store<urd::sequence<int>> { 4, 16 }));  // not sized
    EXPECT_NE(urd::hash_range(std::array { 0 }), urd::hash_range(std::array { 0, 0 }));
    const auto set = SortedSet { 1, 2 };
    EXPECT_EQ(urd::hash_range(set), urd::Hash<SortedSet> {}(set));
    const auto map = SortedMap { { 1, 2 } };
    EXPECT_EQ(urd::hash_range(map), urd::Hash<SortedMap> {}(map));

    // Floats, booleans (packed or not) and records hash by their Hash, not by their bytes.
    const auto elementwise = [](const auto& values)
    {
        auto seed = urd::hash_t { values.size() };
        for (const auto& value : values)
            urd::hash_combine(seed, value);
        return seed;
    };
    const auto floats = std::array { -0.0, 1.5, std::numeric_limits<double>::quiet_NaN() };
    EXPECT_EQ(urd::hash_range(floats), elementwise(floats));
    EXPECT_EQ(urd::hash_range(floats), urd::hash_range(std::array { 0.0, 1.5, std::numeric_limits<double>::signaling_NaN() }));
    const auto booleans = std::array { false, true, false };
    auto packed = std::vector<bool> { false, true, false };
    EXPECT_EQ(urd::hash_range(booleans), elementwise(booleans));
    EXPECT_EQ(urd::hash_range(packed), elementwise(booleans));
    EXPECT_EQ(urd::Hash<std::vector<bool>> {}(packed), elementwise(booleans));
    struct Digit
    {
        std::uint32_t value;
        [[nodiscard]] std::uint32_t digit() const noexcept { return value % 10; }
        static constexpr auto identifying_members() noexcept { return urd::Members<&Digit::digit> {}; }
    };
    static_assert(std::has_unique_object_representations_v<Digit>);
    EXPECT_EQ(urd::hash_range(std::array { Digit { 1 }, Digit { 2 } }), urd::hash_range(std::array { Digit { 11 }, Digit { 22 } }));
}

// Hash, EqualTo and Less tell apart what the type tells apart: a variant's alternative (even of one type), an
// optional's engagement; NaN equals NaN and orders after the numbers; a reference compares what it refers to.
TEST(Predicates, DistinguishAlternativesEngagementAndReferencedValues)
{
    const auto distinct = []<typename T>(const T& lhs, const T& rhs)
    {
        EXPECT_NE(urd::Hash<T> {}(lhs), urd::Hash<T> {}(rhs));
        EXPECT_FALSE(urd::EqualTo<T> {}(lhs, rhs));
        EXPECT_TRUE(urd::Less<T> {}(lhs, rhs));
        EXPECT_FALSE(urd::Less<T> {}(rhs, lhs));
    };
    distinct(std::variant<int, unsigned> { 9 }, std::variant<int, unsigned> { 0U });
    distinct(std::variant<int, int>(std::in_place_index<0>, 9), std::variant<int, int>(std::in_place_index<1>, 0));
    distinct(std::optional<int>(), std::optional<int>(0));
    auto low = 1;
    auto high = 2;
    distinct(std::ref(low), std::ref(high));

    using CistaVariant = cista::offset::variant<int, unsigned>;
    auto first = cista::offset::variant<int, int> {};
    first.emplace<0>(9);
    auto second = cista::offset::variant<int, int> {};
    second.emplace<1>(9);
    distinct(first, second);
    distinct(CistaVariant { 9 }, CistaVariant { 9U });
    EXPECT_TRUE(urd::EqualTo<CistaVariant> {}(CistaVariant {}, CistaVariant {}));
    EXPECT_EQ(urd::Hash<CistaVariant> {}(CistaVariant {}), urd::Hash<CistaVariant> {}(CistaVariant {}));
    EXPECT_FALSE(urd::EqualTo<CistaVariant> {}(CistaVariant {}, CistaVariant { 9 }));
    EXPECT_NE(urd::Hash<CistaVariant> {}(CistaVariant {}), urd::Hash<CistaVariant> {}(CistaVariant { 9 }));
    EXPECT_FALSE(urd::EqualTo<cista::optional<int>> {}(cista::optional<int> { 7 }, cista::optional<int> {}));
    EXPECT_NE(urd::Hash<cista::optional<int>> {}(cista::optional<int> { 0 }), urd::Hash<cista::optional<int>> {}(cista::optional<int> {}));

    const auto nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(urd::EqualTo<double> {}(nan, std::numeric_limits<double>::signaling_NaN()));
    EXPECT_TRUE(urd::Less<double> {}(1.0, nan));
    EXPECT_FALSE(urd::Less<double> {}(nan, 1.0));
    EXPECT_TRUE(urd::less_range(std::array { nan, 1.0 }, std::array { nan, 2.0 }));
    EXPECT_TRUE(urd::equal_range(std::array { nan }, std::array { nan }));
}

// Containers without an urd specialization compare by their own operators: cista's, a segmented vector's, a bitset
// span's. Their Hash is that of their values.
TEST(Predicates, ForeignContainersCompareAndHashByTheirValues)
{
    const auto ordered = []<typename T>(const T& lhs, const T& same, const T& rhs)
    {
        EXPECT_TRUE(urd::EqualTo<T> {}(lhs, same));
        EXPECT_FALSE(urd::EqualTo<T> {}(lhs, rhs));
        EXPECT_TRUE(urd::Less<T> {}(lhs, rhs));
        EXPECT_FALSE(urd::Less<T> {}(rhs, lhs));
        EXPECT_EQ(urd::Hash<T> {}(lhs), urd::Hash<T> {}(same));
        EXPECT_NE(urd::Hash<T> {}(lhs), urd::Hash<T> {}(rhs));
    };
    ordered(cista::offset::string("ab"), cista::offset::string("ab"), cista::offset::string("ac"));
    ordered(cista::offset::vector<int> { 1, 2 }, cista::offset::vector<int> { 1, 2 }, cista::offset::vector<int> { 1, 3 });
    ordered(cista::optional<int> { 7 }, cista::optional<int> { 7 }, cista::optional<int> { 8 });
    ordered(Segmented { 1, 2 }, Segmented { 1, 2 }, Segmented { 1, 3 });
    ordered(SortedSet { 1, 2 }, SortedSet { 1, 2 }, SortedSet { 1, 3 });
    ordered(SortedMap { { 1, 2 } }, SortedMap { { 1, 2 } }, SortedMap { { 1, 3 } });
    EXPECT_TRUE(urd::Less<std::set<int>> {}({ 1, 2 }, { 1, 3 }));
    EXPECT_TRUE((urd::Less<std::map<int, int>> {}({ { 1, 2 } }, { { 1, 3 } })));

    const auto lhs_blocks = std::array<std::uint64_t, 1> { 0b1010 };
    const auto rhs_blocks = std::array<std::uint64_t, 1> { 0b1010 };
    const auto other_blocks = std::array<std::uint64_t, 1> { 0b0010 };
    using Bits = urd::repr::BitsetSpan<const std::uint64_t>;
    EXPECT_TRUE(urd::EqualTo<Bits> {}(Bits(lhs_blocks.data(), 4), Bits(rhs_blocks.data(), 4)));
    EXPECT_FALSE(urd::EqualTo<Bits> {}(Bits(lhs_blocks.data(), 4), Bits(other_blocks.data(), 4)));
}

// equal_range and less_range: by element EqualTo and Less, over any input range (adaptors, move-only views, proxy
// references), with sizes and prefixes; the transparent and derived predicates take an identity for a record.
TEST(Predicates, RangeComparisonsTakeAnyInputRange)
{
    const auto values = urd::Store<urd::sequence<int>> { 1, 2, 3, 4 };
    auto even_squares = values | std::views::filter([](int value) { return value % 2 == 0; }) | std::views::transform([](int value) { return value * value; });
    EXPECT_TRUE(urd::equal_range(even_squares, std::array { 4, 16 }));
    EXPECT_FALSE(urd::equal_range(even_squares, std::array { 4, 9 }));
    EXPECT_TRUE(urd::less_range(even_squares, std::array { 4, 17 }));
    EXPECT_FALSE(urd::less_range(even_squares, std::array { 4, 16 }));
    EXPECT_FALSE(urd::equal_range(std::array { 1, 2 }, std::array { 1, 2, 3 }));
    EXPECT_TRUE(urd::less_range(std::array { 1, 2 }, std::array { 1, 2, 3 }));
    EXPECT_FALSE(urd::less_range(std::array { 1, 2, 3 }, std::array { 1, 2 }));

    const auto first_two = std::views::take(urd::Store<urd::sequence<int>> { 1, 2, 3 }, 2);  // an owning_view: moves only
    EXPECT_TRUE(urd::equal_range(first_two, std::array { 1, 2 }));
    EXPECT_TRUE(urd::less_range(first_two, std::array { 1, 3 }));

    auto proxies = std::vector<bool> { false, true };
    EXPECT_TRUE(urd::equal_range(proxies, std::array { false, true }));
    EXPECT_TRUE(urd::equal_range(std::array { false, true }, proxies));
    EXPECT_TRUE(urd::less_range(proxies, std::array { true }));
    EXPECT_FALSE(urd::less_range(std::array { true }, proxies));
    EXPECT_TRUE(urd::less_range(std::array { false }, proxies));
    struct Pinned
    {
        int value;
        explicit Pinned(int value_) : value(value_) {}
        Pinned(const Pinned&) = delete;
        Pinned& operator=(const Pinned&) = delete;
        Pinned(Pinned&&) = delete;
        Pinned& operator=(Pinned&&) = delete;
        ~Pinned() = default;
        bool operator==(const Pinned&) const = default;
    };
    EXPECT_TRUE(urd::equal_range(std::array { Pinned(1), Pinned(2) }, std::array { Pinned(1), Pinned(2) }));

    const auto lhs = Named { "at", 2 };
    const auto name = std::string("at");
    const auto arity = 3;
    const auto rhs = std::tie(name, arity);
    EXPECT_FALSE(urd::Less<Named> {}(rhs, lhs));
    EXPECT_TRUE(urd::LessEqual<Named> {}(lhs, rhs));
    EXPECT_TRUE(urd::Greater<Named> {}(rhs, lhs));
    EXPECT_TRUE(urd::GreaterEqual<Named> {}(rhs, lhs));
    EXPECT_EQ(urd::ThreeWayCompare<Named> {}(lhs, rhs), std::strong_ordering::less);
    EXPECT_EQ(urd::ThreeWayCompare<> {}(lhs, rhs), std::strong_ordering::less);
}

// A throwing comparison, dereference, size() or data() reaches the caller, and noexcept says so.
TEST(Predicates, FailuresOfUserOperationsPropagate)
{
    const auto value = ThrowingLess {};
    static_assert(!noexcept(urd::Less<ThrowingLess> {}(value, value)) && !noexcept(urd::Less<> {}(value, value)));
    static_assert(noexcept(urd::Less<double> {}(1, 2)) && noexcept(urd::Hash<unsigned> {}(1U)));
    EXPECT_THROW(urd::Less<ThrowingLess> {}(value, value), std::runtime_error);
    EXPECT_THROW(urd::Less<> {}(value, value), std::runtime_error);
    EXPECT_THROW((urd::Less<std::pair<int, ThrowingLess>> {}({ 0, value }, { 0, value })), std::runtime_error);
    EXPECT_THROW(urd::Less<std::tuple<ThrowingLess>> {}({ value }, { value }), std::runtime_error);
    EXPECT_THROW(urd::ThreeWayCompare<ThrowingLess> {}(value, value), std::runtime_error);
    const auto stored = urd::Store<urd::sequence<ThrowingLess>> { value };
    EXPECT_THROW(urd::Less<urd::Store<urd::sequence<ThrowingLess>>> {}(stored, stored), std::runtime_error);
    EXPECT_THROW(urd::Less<std::reference_wrapper<const ThrowingLess>> {}(std::cref(value), std::cref(value)), std::runtime_error);

    const auto values = std::array { 1U, 2U, 3U };
    auto decoded = values | std::views::transform([](unsigned element) -> unsigned { return element == 2 ? throw std::runtime_error("dereference") : element; });
    static_assert(!noexcept(urd::less_range(decoded, values)));
    EXPECT_THROW(urd::less_range(decoded, values), std::runtime_error);
    EXPECT_THROW(urd::equal_range(decoded, values), std::runtime_error);
    EXPECT_THROW(urd::equal_range(values, decoded), std::runtime_error);
    EXPECT_THROW(urd::hash_range(decoded), std::runtime_error);
    EXPECT_THROW(urd::hash_range(values | std::views::filter([](unsigned) -> bool { throw std::runtime_error("begin"); })), std::runtime_error);
    EXPECT_THROW(urd::hash_range(values | std::views::transform([](unsigned) -> double { throw std::runtime_error("value"); })), std::runtime_error);
    static_assert(std::ranges::contiguous_range<ThrowingDataRange>);
    for (const bool throw_on_size : { false, true })
    {
        const auto range = ThrowingDataRange { values, throw_on_size };
        static_assert(!noexcept(urd::hash_range(range)));
        EXPECT_THROW(urd::hash_range(range), std::runtime_error);
    }
    static_assert(noexcept(urd::hash_range(std::span<const unsigned>(values))));
}
