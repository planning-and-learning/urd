// Prehashed keys: hashed Stores find, count and (interners) insert by a key that carries its hash, the standard's way
// to pass a precomputed hash (heterogeneous lookup); an element is hashed once, an insert included. Each family header
// a Store here needs is included, and no other.

#include "urd/testing/allocations.hpp"
#include "urd/elements/prehashed.hpp"
#include "urd/memory.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/records/arena.hpp"
#include "urd/repr/records/records.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/repr/wrappers/layered_pool.hpp"
#include "urd/elements/flat.hpp"
#include "urd/store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <tuple>

namespace
{

/// A key whose hash counts its calls.
struct Key
{
    int value = 0;

    friend bool operator==(const Key&, const Key&) = default;
    static constexpr auto cista_members() noexcept { return urd::Members<&Key::value> {}; }
};
int hashes = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): the counter the tests read
/// The calls per key (its value's low bits): a table that grows hashes the records it holds again.
std::array<int, 4> hashes_of {};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace

template<>
struct urd::Hash<Key>
{
    urd::hash_t operator()(const Key& key) const noexcept
    {
        ++hashes;
        ++hashes_of[static_cast<std::size_t>(key.value) & 3U];
        return static_cast<urd::hash_t>(key.value) * 0x9e3779b97f4a7c15ULL;
    }
};

namespace
{

using urd::pool;
using urd::Store;

// prehash hashes once; lookups by its result do not hash again.
template<typename Pool>
void finds_and_inserts_hashing_once()
{
    auto keys = Pool(urd::layer { 0 });
    hashes_of = {};
    const auto [first, inserted] = keys.insert(Key { 1 });
    EXPECT_TRUE(inserted);
    EXPECT_EQ(hashes_of[1], 1) << "a new record is hashed once: the lookup's hash goes into the table";

    const auto two_key = Key { 2 };
    const auto two = urd::prehash(two_key);
    EXPECT_EQ(hashes_of[2], 1);
    EXPECT_FALSE(keys.contains(two));
    EXPECT_TRUE(keys.find(two).is_none());
    const auto [second, new_two] = keys.insert(two);
    EXPECT_TRUE(new_two);
    EXPECT_TRUE(keys.contains(two));
    EXPECT_EQ(keys.find(two), second);
    EXPECT_EQ(hashes_of[2], 1) << "a prehashed key is not hashed again";

    const auto one_key = Key { 1 };
    const auto one = urd::prehash(one_key);
    const auto [again, new_one] = keys.insert(one, [](auto) { return Key { 1 }; });
    EXPECT_FALSE(new_one);
    EXPECT_EQ(again, first);
    const auto three_key = Key { 3 };
    const auto three = urd::prehash(three_key);
    const auto [made, new_three] = keys.insert(three, [](auto) { return Key { 3 }; });
    EXPECT_TRUE(new_three);
    EXPECT_EQ(keys[made], Key { 3 });
    EXPECT_EQ(hashes_of[3], 1);
}

TEST(Prehashed, RecordInterner) { finds_and_inserts_hashing_once<Store<pool<Key>, urd::flat>>(); }
TEST(Prehashed, ThreadSafeRecordInterner) { finds_and_inserts_hashing_once<Store<pool<Key>, urd::flat, urd::concurrent>>(); }
TEST(Prehashed, ValueInterner) { finds_and_inserts_hashing_once<Store<pool<Key>>>(); }
TEST(Prehashed, ArenaInterner) { finds_and_inserts_hashing_once<urd::repr::Numbered<urd::repr::ArenaInterner<Key>>>(); }
TEST(Prehashed, ThreadSafeArenaInterner) { finds_and_inserts_hashing_once<urd::repr::Numbered<urd::repr::ArenaInterner<Key, true>>>(); }
TEST(Prehashed, LayeredPool) { finds_and_inserts_hashing_once<Store<pool<Key>, urd::flat, urd::layered>>(); }

// A layered pool hashes a key once for its whole chain of layers, not once per layer.
TEST(Prehashed, LayeredPoolHashesOnceForTheChain)
{
    using Keys = Store<pool<Key>, urd::flat, urd::layered>;
    auto root = Keys(urd::layer { 0 });
    const auto one = root.insert(Key { 1 }).first;
    auto middle = Keys(urd::layer { 1 });
    middle.layer_over(&root);
    auto leaf = Keys(urd::layer { 2 });
    leaf.layer_over(&middle);
    hashes = 0;
    EXPECT_EQ(leaf.find(Key { 1 }), one);
    EXPECT_FALSE(leaf.insert(Key { 1 }).second);
    EXPECT_TRUE(leaf.insert(Key { 2 }).second);
    EXPECT_EQ(hashes, 3) << "one hash per lookup or insert";
    const auto two_key = Key { 2 };
    const auto two = urd::prehash(two_key);
    EXPECT_FALSE(leaf.find(two).is_none());
    EXPECT_TRUE(middle.find(two).is_none());
    EXPECT_EQ(hashes, 4) << "a prehashed key is not hashed again in any layer";
}

/// A record that cannot be copied: an arena interner serializes it from a reference.
struct Unique
{
    urd::flat_string name;

    Unique() = default;
    explicit Unique(std::string_view text) : name(text) {}
    Unique(const Unique&) = delete;
    Unique& operator=(const Unique&) = delete;
    Unique(Unique&&) = default;
    Unique& operator=(Unique&&) = default;
    ~Unique() = default;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Unique::name> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Unique::name> {}; }
};

// A factory may give a reference to the record: the arena interner serializes it, and needs no copy of its own.
TEST(Prehashed, FactoryGivesAReference)
{
    auto names = Store<pool<Unique>, urd::flat>(urd::layer { 0 });
    const auto on = Unique("on");
    const auto key = urd::prehash(on);
    const auto [index, inserted] = names.insert(key, [&on](auto) -> const Unique& { return on; });
    EXPECT_TRUE(inserted);
    EXPECT_EQ(names.find(key), index);
    EXPECT_EQ(std::string_view(names[index].name), "on");
}

/// A part a record derives from, and a record whose Members name its base's member too (&Derived::name points into Named).
struct Named
{
    urd::flat_string name;
};
struct Derived : Named
{
    std::uint32_t arity = 0;

    Derived() = default;
    Derived(std::string_view text, std::uint32_t arity_) : Named { urd::flat_string(text) }, arity(arity_) {}

    static constexpr auto cista_members() noexcept { return urd::Members<&Derived::name, &Derived::arity> {}; }
    static constexpr auto identifying_members() noexcept { return urd::Members<&Derived::name, &Derived::arity> {}; }
};

// The arena interners store such a record as cista serializes it, alone and where threads share them.
TEST(Prehashed, ArenaInternsRecordsNamingBaseMembers)
{
    static_assert(urd::Serializable<Derived>);
    const auto check = []<typename Records>()
    {
        auto records = Records(urd::layer { 0 });
        const auto [on, inserted] = records.insert(Derived("on", 2));
        EXPECT_TRUE(inserted);
        EXPECT_FALSE(records.insert(Derived("on", 2)).second);
        EXPECT_TRUE(records.find(Derived("on", 3)).is_none());
        EXPECT_EQ(std::string_view(records[on].name), "on");
        EXPECT_EQ(records[on].arity, 2U);
        EXPECT_GT(urd::memory_usage(records), 0U);
    };
    check.template operator()<Store<pool<Derived>, urd::flat>>();
    check.template operator()<Store<pool<Derived>, urd::flat, urd::stable_refs, urd::concurrent>>();
}

// A single-threaded arena pool owns nothing until its first record (a repository holds one per record type).
TEST(Prehashed, EmptyPoolsOwnNoMemory)
{
    auto names = Store<pool<Unique>, urd::flat>(urd::layer { 0 });
    EXPECT_EQ(urd::memory_usage(names), 0U);
    EXPECT_TRUE(names.find(Unique("on")).is_none());
    EXPECT_EQ(names.size(), 0U);
    names.clear();
    EXPECT_EQ(urd::memory_usage(names), 0U);
    names.insert(Unique("on"));
    EXPECT_GT(urd::memory_usage(names), 0U);

    // So does a single-threaded indexed set (a positional set, a value or record interner).
    auto symbols = Store<urd::set<int>, urd::positional, urd::monotone>(urd::layer { 0 });
    auto values = Store<pool<Key>>(urd::layer { 0 });
    EXPECT_EQ(urd::memory_usage(symbols), 0U);
    EXPECT_EQ(urd::memory_usage(values), 0U);
    EXPECT_FALSE(symbols.contains(3));
    EXPECT_EQ(symbols.begin(), symbols.end());
    symbols.insert(3);
    EXPECT_TRUE(symbols.contains(3));
    EXPECT_GT(urd::memory_usage(symbols), 0U);
}

// A positional set (an indexed set) inserts by a prehashed key too.
TEST(Prehashed, PositionalSet)
{
    auto keys = Store<urd::set<Key>, urd::positional, urd::monotone>(urd::layer { 0 });
    hashes = 0;
    const auto seven = Key { 7 };
    const auto key = urd::prehash(seven);
    EXPECT_TRUE(keys.insert(key).second);
    EXPECT_FALSE(keys.insert(key).second);
    EXPECT_TRUE(keys.contains(key));
    EXPECT_EQ(keys.count(key), 1U);
    EXPECT_EQ(hashes, 1);
}

// A hash set and a hash map find by a prehashed key, as by their heterogeneous lookup.
TEST(Prehashed, HashSetAndMap)
{
    auto keys = Store<urd::set<Key>> { Key { 4 } };
    hashes = 0;
    const auto four_key = Key { 4 };
    const auto four = urd::prehash(four_key);
    const auto five_key = Key { 5 };
    const auto five = urd::prehash(five_key);
    EXPECT_TRUE(keys.contains(four));
    EXPECT_FALSE(keys.contains(five));
    EXPECT_EQ(*keys.find(four), Key { 4 });
    EXPECT_EQ(hashes, 2);

    auto arity = Store<urd::map<std::string, int>> { { "on", 2 }, { "clear", 1 } };
    const auto name = std::string("on");
    const auto on = urd::prehash(name);
    EXPECT_EQ(arity.find(on)->second, 2);
    EXPECT_EQ(arity.count(on), 1U);
}

}  // namespace
