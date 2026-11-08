// Scopes, change logs, layers and families of Stores. Each header a Store here needs is included.

#include "urd/testing/allocations.hpp"
#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/pools/pools.hpp"
#include "urd/repr/pools/tree_bitset.hpp"
#include "urd/repr/pools/tree_vector.hpp"
#include "urd/repr/sequences/jagged.hpp"
#include "urd/repr/sequences/row_pool.hpp"
#include "urd/repr/sequences/segmented.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/repr/wrappers/layer.hpp"
#include "urd/repr/wrappers/layered_pool.hpp"
#include "urd/repr/wrappers/scoped.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

using urd::map;
using urd::pool;
using urd::sequence;
using urd::set;
using urd::Store;

// Scopes undo, latest first, every change made in them: random changes and scopes against a stack of copies.
TEST(Scoped, PopUndoesTheScopesChanges)
{
    using Symbols = Store<map<int, int>, urd::scoped>;
    auto random = std::mt19937(3);
    for (int round = 0; round < 50; ++round)
    {
        auto symbols = Symbols();
        auto saved = std::vector<std::map<int, int>>();
        const auto contents = [&symbols] { return std::map<int, int>(symbols.begin(), symbols.end()); };
        for (int step = 0; step < 300; ++step)
        {
            const int key = static_cast<int>(random() % 16);
            const int value = static_cast<int>(random() % 100);
            switch (random() % 7)
            {
                case 0:
                    symbols.push_scope();
                    saved.push_back(contents());
                    break;
                case 1:
                    if (!saved.empty())
                    {
                        symbols.pop_scope();
                        ASSERT_EQ(contents(), saved.back()) << "step " << step;
                        saved.pop_back();
                    }
                    break;
                case 2:
                    symbols.erase(key);
                    break;
                case 3:
                    symbols.insert_or_assign(key, value);
                    break;
                case 4:
                    symbols[key] = value;
                    break;
                case 5:
                    symbols.try_emplace(key, value);
                    break;
                default:
                    if (symbols.contains(key))
                        symbols.at(key) += 1;
                    break;
            }
            ASSERT_EQ(symbols.depth(), saved.size());
        }
    }
}

// A binding is in the innermost scope where that scope made it (loki's redeclaration check).
TEST(Scoped, BoundInScopeIsTheInnermostScopesOwn)
{
    auto symbols = Store<map<std::string, int>, urd::scoped>();
    symbols.insert_or_assign("x", 1);
    EXPECT_TRUE(symbols.bound_in_scope("x"));  // no scope open: bound at all
    symbols.push_scope();
    EXPECT_FALSE(symbols.bound_in_scope("x"));  // the outer binding is not this scope's
    symbols.insert_or_assign("x", 2);
    symbols.insert_or_assign("y", 3);
    EXPECT_TRUE(symbols.bound_in_scope("x") && symbols.bound_in_scope("y"));
    symbols.push_scope();
    EXPECT_FALSE(symbols.bound_in_scope("y"));
    symbols.erase("y");
    EXPECT_FALSE(symbols.bound_in_scope("y"));  // changed here, but not bound
    symbols.pop_scope();
    EXPECT_TRUE(symbols.bound_in_scope("y"));
    symbols.pop_scope();
    EXPECT_EQ(symbols.at("x"), 1);
    EXPECT_TRUE(symbols.bound_in_scope("x") && !symbols.contains("y"));
}

// Equal bindings compare equal, whatever order the tables iterate in: one table grew large and shrank back, the
// other never grew, so their slots differ.
TEST(Scoped, EqualBindingsCompareEqual)
{
    using Symbols = Store<map<int, int>, urd::scoped>;
    auto grown = Symbols();
    for (int key = 0; key < 4096; ++key)
        grown.insert_or_assign(key, key);
    for (int key = 4096 - 1; key >= 0; --key)
        if (key % 512 != 0)
            grown.erase(key);
    auto small = Symbols();
    for (int key = 4096 - 512; key >= 0; key -= 512)
        small.insert_or_assign(key, key);
    small.push_scope();  // open scopes do not count: only the bindings in force
    EXPECT_FALSE(std::ranges::equal(grown, small));
    EXPECT_TRUE(grown == small);
    small.insert_or_assign(0, 1);
    EXPECT_FALSE(grown == small);
}

// A layer finds its parent's values, holds only its own, and shadows the parent's with its own; owner(key) is the
// layer that holds a key.
TEST(Layered, AnswersFromItselfThenItsParent)
{
    using Name = urd::index<struct NameTag>;
    using Names = Store<map<Name, int>, urd::dense, urd::layered>;
    const auto on = Name(0);
    const auto clear = Name(1);
    const auto holding = Name(2);
    const auto empty = Name(3);
    auto parent = Names();
    parent.insert_or_assign(on, 0);
    parent.insert_or_assign(clear, 1);
    auto child = Names();
    child.insert_or_assign(holding, 2);
    child.layer_over(&parent);
    EXPECT_TRUE(child.contains(on) && child.contains(holding) && !child.contains(empty));
    EXPECT_EQ(child.size(), 1U);
    EXPECT_EQ(std::as_const(child).at(clear), 1);
    EXPECT_EQ(&child.owner(clear), &parent);
    EXPECT_EQ(&child.owner(holding), &child);
    EXPECT_EQ(child.read(clear, [](int value) { return value; }, -1), 1);
    EXPECT_EQ(child.read(empty, [](int value) { return value; }, -1), -1);
    // try_emplace makes an own value from its arguments where the layer has none, shadowing the parent's.
    EXPECT_TRUE(child.try_emplace(on, 7).second);
    EXPECT_FALSE(child.try_emplace(on, 8).second);
    EXPECT_EQ(std::as_const(child).at(on), 7);
    EXPECT_EQ(&child.owner(on), &child);
    EXPECT_EQ(parent.at(on), 0);
    child.insert_or_assign(on, 5);
    EXPECT_EQ(child.at(on), 5);
    EXPECT_EQ(child.erase(clear), 0U);
    EXPECT_TRUE(child.contains(clear));
    // at() on a layer that may change hands out its own value to change: the parent's is copied up first.
    child.at(clear) = 3;
    EXPECT_EQ(child.size(), 3U);
    EXPECT_EQ(parent.at(clear), 1);
    // update() too, where neither holds the key: held is false, the value a default one.
    EXPECT_FALSE(child.update(empty, [](int& value, bool held) { value = 4; return held; }));
    EXPECT_EQ(std::as_const(child).at(empty), 4);
    child.layer_over(nullptr);
    EXPECT_FALSE(child.contains(Name(9)));
    EXPECT_EQ(child.size(), 4U);
}

namespace
{

template<typename T>
using Interned = Store<set<T>, urd::positional, urd::monotone>;

}  // namespace

// Relations as yggdrasil's relation repositories keep them: per predicate, a pool of the rows (tuples of object
// indices) bound to it, each row once; a phase layers over the previous through both levels: a predicate absent
// from the layer falls through to the parent's pool, a predicate touched in the layer gets a pool of its own over
// the parent's pool, with indices continuing after it.
namespace
{

struct PredicateTag;
struct ObjectTag;
using Object = urd::index<ObjectTag>;
using Row = Store<sequence<Object>, urd::frozen>;
using Rows = Store<pool<Row>, urd::monotone, urd::layered>;
using Relations = Store<map<urd::index<PredicateTag>, Rows>, urd::dense, urd::layered>;
static_assert(std::same_as<Relations::representation, urd::repr::Layered<urd::repr::IndexMap<urd::index<PredicateTag>, Rows, false>>>);

}  // namespace

TEST(Layered, RelationsLayerThroughBothLevels)
{
    const auto on = urd::index<PredicateTag>(0);
    const auto clear = urd::index<PredicateTag>(1);
    const auto holding = urd::index<PredicateTag>(2);
    const auto a = Object(0);
    const auto b = Object(1);
    auto domain = Relations();
    const auto on_ab = domain[on].insert({ a, b }).first;
    const auto on_ba = domain[on].insert({ b, a }).first;
    const auto clear_a = domain[clear].insert({ a }).first;
    EXPECT_EQ(on_ab, urd::index<Row>(0));
    EXPECT_EQ(on_ba, urd::index<Row>(1));
    EXPECT_EQ(clear_a, urd::index<Row>(0));

    auto task = Relations();
    task.layer_over(&domain);
    EXPECT_TRUE(task.contains(on) && task.contains(clear) && !task.contains(holding));
    EXPECT_EQ(task.size(), 0U);
    EXPECT_EQ(std::as_const(task).at(clear).row_size(clear_a), 1U);  // read through: the domain's pool
    EXPECT_EQ(task[on].insert({ a, b }), std::pair(on_ab, false));   // the domain's row, through the task's own pool over it
    EXPECT_EQ(task.size(), 1U);                                      // which now exists, layered
    EXPECT_EQ(task[on].base(), 2U);
    const auto on_aa = task[on].insert({ a, a }).first;
    EXPECT_EQ(on_aa, urd::index<Row>(2));
    const auto expected = Row { a, b };
    EXPECT_EQ(task[on].get(on_ab), expected);
    EXPECT_EQ(task[on].row_size(on_aa), 2U);
    EXPECT_EQ(std::as_const(domain).at(on).row_size(on_ba), 2U);  // the domain's pool, untouched: still two rows at the branch
    EXPECT_FALSE(task[on].parent_grew());
    const auto holding_b = task[holding].insert({ b }).first;  // a predicate new in the task: a root pool of its own
    EXPECT_EQ(holding_b, urd::index<Row>(0));
    EXPECT_EQ(task.size(), 2U);
    // The layer that holds a predicate's rows: the task's own pool, else the domain's.
    EXPECT_EQ(&task.owner(on), &task);
    EXPECT_EQ(&task.owner(clear), &domain);
    // A cleared task holds nothing of its own; a predicate touched again gets its pool layered again, over the domain's.
    task.clear();
    EXPECT_EQ(task.size(), 0U);
    EXPECT_EQ(&task.owner(on), &domain);
    EXPECT_EQ(task[on].base(), 2U);
    EXPECT_EQ(task[on].insert({ a, a }).first, urd::index<Row>(2));
}

// Rows of a width given at construction (urd::bit): try_emplace builds a predicate's own pool from its arguments and
// layers it over the parent's, which may be narrower.
namespace
{

using BitRows = Store<pool<Row>, urd::monotone, urd::bit, urd::layered>;
using BitRelations = Store<map<urd::index<PredicateTag>, BitRows>, urd::dense, urd::layered>;
static_assert(std::same_as<BitRows::representation, urd::repr::LayeredPool<urd::repr::RowInterner<Row, false, urd::width::bit>>>);

}  // namespace

TEST(Layered, RelationsBuildEachLayersPoolFromItsArguments)
{
    const auto on = urd::index<PredicateTag>(0);
    auto domain = BitRelations();
    EXPECT_TRUE(domain.try_emplace(on, urd::bit_width::for_bound(4)).second);
    const auto on_12 = domain[on].insert({ Object(1), Object(2) }).first;
    auto task = BitRelations();
    task.layer_over(&domain);
    EXPECT_TRUE(task.try_emplace(on, urd::bit_width::for_bound(1000)).second);
    EXPECT_FALSE(task.try_emplace(on, urd::bit_width::for_bound(1000)).second);
    EXPECT_EQ(task[on].base(), 1U);
    EXPECT_EQ(task[on].insert({ Object(1), Object(2) }), std::pair(on_12, false));  // found in the domain's pool
    const auto wide = task[on].insert({ Object(999), Object(3) }).first;             // too wide for the domain's
    EXPECT_EQ(wide, urd::index<Row>(1));
    EXPECT_EQ(task[on].get(wide), (Row { Object(999), Object(3) }));
}

}  // namespace
