// Scopes, change logs, layers and families of Stores. Each header a Store here needs is included.

#include "../allocations.hpp"
#include "urd/compose/family.hpp"
#include "urd/concepts.hpp"
#include "urd/repr/bidirectional_csr.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/csr_multimap.hpp"
#include "urd/repr/dense_multimap.hpp"
#include "urd/repr/dense_set.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/index_map.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/jagged.hpp"
#include "urd/repr/layer.hpp"
#include "urd/repr/layered_pool.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/pools.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/row_pool.hpp"
#include "urd/repr/scoped.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/slots.hpp"
#include "urd/repr/sparse_set.hpp"
#include "urd/repr/tree_bitset.hpp"
#include "urd/repr/tree_vector.hpp"
#include "urd/repr/unordered_flat_map.hpp"
#include "urd/repr/unordered_flat_set.hpp"
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

using urd::key;
using urd::Store;
using urd::value;

// Scopes undo, latest first, every change made in them: random changes and scopes against a stack of copies.
TEST(Scoped, PopUndoesTheScopesChanges)
{
    using Symbols = Store<key<int>, value<int>, urd::scoped>;
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
    auto symbols = Store<key<std::string>, value<int>, urd::scoped>();
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
    using Symbols = Store<key<int>, value<int>, urd::scoped>;
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

// The change log answers whether an element is among its changes, without scanning them.
TEST(ChangeLog, WasAddedAndWasRemoved)
{
    auto facts = Store<key<int>, urd::change_log> { 1, 2 };
    facts.clear_changes();
    facts.insert(3);
    facts.erase(1);
    EXPECT_TRUE(facts.was_added(3) && !facts.was_added(2) && facts.was_removed(1) && !facts.was_removed(3));
}

// A layer finds its parent's elements, holds only its own, and shadows the parent's values with its own.
TEST(Layered, AnswersFromItselfThenItsParent)
{
    using Names = Store<key<std::string>, value<int>, urd::layered>;
    const auto parent = Names { { "on", 0 }, { "clear", 1 } };
    auto child = Names { { "holding", 2 } };
    child.layer_over(&parent);
    EXPECT_TRUE(child.contains("on") && child.contains("holding") && !child.contains("empty"));
    EXPECT_EQ(child.size(), 1U);
    EXPECT_EQ(std::as_const(child).at("clear"), 1);
    EXPECT_FALSE(child.try_emplace("on", 7).second);
    EXPECT_EQ(std::as_const(child).at("on"), 0);
    child.insert_or_assign("on", 5);
    EXPECT_EQ(child.at("on"), 5);
    EXPECT_EQ(parent.at("on"), 0);
    EXPECT_EQ(child.erase("clear"), 0U);
    EXPECT_TRUE(child.contains("clear"));
    // at() on a layer that may change hands out its own value to change: the parent's is copied up first.
    child.at("clear") = 3;
    EXPECT_EQ(child.size(), 3U);
    EXPECT_EQ(parent.at("clear"), 1);

    using Seen = Store<key<int>, urd::layered>;
    const auto before = Seen { 1, 2 };
    auto after = Seen { 3 };
    after.layer_over(&before);
    EXPECT_FALSE(after.insert(2).second);
    EXPECT_TRUE(after.insert(4).second);
    EXPECT_EQ(std::set<int>(after.begin(), after.end()), (std::set<int> { 3, 4 }));
    after.layer_over(nullptr);
    EXPECT_FALSE(after.contains(1));
}

namespace
{

template<typename T>
using Interned = Store<key<T>, urd::positional, urd::monotone>;

}  // namespace

// A family holds one Store per type and reaches each by its type.
TEST(Family, ReachesEachMemberByType)
{
    auto repositories = urd::compose::Family<Interned, int, std::string>();
    repositories.get<int>().insert(7);
    repositories.get<std::string>().insert("on");
    repositories.get<std::string>().insert("clear");
    EXPECT_EQ(repositories.get<int>().size(), 1U);
    EXPECT_EQ(std::as_const(repositories).get<std::string>()[1], "clear");
    std::size_t total = 0;
    repositories.for_each([&total](const auto& member) { total += member.size(); });
    EXPECT_EQ(total, 3U);
}

// A change log holds exactly the difference between the contents and those when it was last cleared.
TEST(ChangeLog, HoldsTheDifferenceSinceItWasCleared)
{
    auto random = std::mt19937(29);
    auto facts = Store<key<int>, urd::change_log>();
    auto reference = std::set<int>();
    for (int step = 0; step < 2000; ++step)
    {
        const int element = static_cast<int>(random() % 20);
        switch (random() % 10)
        {
            case 0:
                facts.clear_changes();
                reference = std::set<int>(facts.begin(), facts.end());
                break;
            case 1:
                if (random() % 8 == 0)
                    facts.clear();
                break;
            case 2:
            case 3:
            case 4:
                facts.erase(element);
                break;
            default:
                facts.insert(element);
                break;
        }
        const auto now = std::set<int>(facts.begin(), facts.end());
        auto added = std::vector<int>();
        auto removed = std::vector<int>();
        std::ranges::set_difference(now, reference, std::back_inserter(added));
        std::ranges::set_difference(reference, now, std::back_inserter(removed));
        ASSERT_EQ(std::set<int>(facts.added().begin(), facts.added().end()), std::set<int>(added.begin(), added.end())) << "step " << step;
        ASSERT_EQ(facts.added().size(), added.size());
        ASSERT_EQ(std::set<int>(facts.removed().begin(), facts.removed().end()), std::set<int>(removed.begin(), removed.end())) << "step " << step;
        ASSERT_EQ(facts.removed().size(), removed.size());
    }
}

// Relations as yggdrasil's relation repositories keep them: per predicate, a pool of the rows (tuples of object
// indices) bound to it, each row once; a phase layers over the previous through both levels: a predicate absent
// from the layer falls through to the parent's pool, a predicate touched in the layer gets a pool of its own over
// the parent's pool, with indices continuing after it.
namespace
{

struct PredicateTag;
struct ObjectTag;
using Object = urd::index<ObjectTag>;
using Row = Store<value<Object>, urd::frozen>;
using Rows = Store<value<Row>, urd::handles, urd::unique, urd::monotone, urd::layered>;
using Relations = Store<key<urd::index<PredicateTag>>, value<Rows>, urd::dense, urd::layered>;
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
    EXPECT_EQ(task[on].layer(), 1U);
    const auto on_aa = task[on].insert({ a, a }).first;
    EXPECT_EQ(on_aa, urd::index<Row>(2));
    const auto expected = Row { a, b };
    EXPECT_EQ(task[on].get(on_ab), expected);
    EXPECT_EQ(task[on].row_size(on_aa), 2U);
    EXPECT_EQ(std::as_const(domain).at(on).row_size(on_ba), 2U);  // the domain's pool, untouched: still two rows at the branch
    EXPECT_FALSE(task[on].parent_grew());
    const auto holding_b = task[holding].insert({ b }).first;  // a predicate new in the task: a root pool of its own
    EXPECT_EQ(holding_b, urd::index<Row>(0));
    EXPECT_EQ(task[holding].layer(), 0U);
    EXPECT_EQ(task.size(), 2U);
}

}  // namespace
