// The registry audit: for every request of the grid (tests/grid.hpp), every registered candidate that serves it, and
// whether the one chosen (the first serving one) answers more categories the request left open than another serving
// one does. Such a choice is a surprise unless a benchmark backs it: each one is listed below with its reason.

#include "urd/registry.hpp"

#include "comparisons.hpp"
#include "grid.hpp"

#include <array>
#include <gtest/gtest.h>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using urd::test::Id;

/// The guarantees that cost something to give: storage by index, nodes, locks, generations, a log, a parent, shared
/// parts. A container that gives one nobody asked for is a surprise, unlike one that also happens to keep its memory,
/// lie contiguous, or iterate in order (a dense container's order is its index order, at no cost).
constexpr std::array costly { urd::tag::dense,       urd::tag::stable_refs,  urd::tag::concurrent, urd::tag::constant_clear,     urd::tag::scoped,
                              urd::tag::layered,     urd::tag::change_log,   urd::tag::handles,    urd::tag::structural_sharing, urd::tag::partition,
                              urd::tag::fixed_shape, urd::tag::bidirectional };

/// How many costly guarantees R gives that the request S did not ask for.
template<typename S, typename R>
constexpr int uninvited()
{
    int count = 0;
    for (const auto each : costly)
        if (!S::urd_properties.has(each) && urd::container_traits<R>::urd_properties.has(each))
            ++count;
    return count;
}

struct candidate
{
    std::string_view id;
    bool serves;
    int uninvited;
    /// Whether the representation stores by index: a demand on the keys, not a guarantee alone.
    bool demands_dense;
};

/// Whether R serves S, read in a class template: a function template with R deduced makes GCC instantiate R itself
/// while checking the concept, which the registry's selection never does.
template<typename R, typename S>
struct serving
{
    static constexpr bool value = urd::detail::serves<R, S>;
};

template<typename S, typename List>
struct candidates;
template<typename S, typename... Rs>
struct candidates<S, urd::detail::type_list<Rs...>>
{
    static std::vector<candidate> list()
    {
        return {
            candidate { urd::test::name<Rs>::id, serving<Rs, S>::value, uninvited<S, Rs>(), urd::container_traits<Rs>::urd_properties.has(urd::tag::dense) }...
        };
    }
};

/// The surprises the registry's order makes on purpose: the request, and the benchmark or reason behind the order.
const std::string_view tree_for_changing_order =
    "a sorted set or map that changes is a tree (nodes, O(log n) inserts); the flat ones serve only with frozen or small: docs/BENCHMARKS.md";
const std::string_view deque_for_fifo =
    "a deque is level with a ring over one vector on a small frontier and faster on a large one and at growing (3.9 against 5.4 ns per "
    "step with a million queued, 2.2 against 6.7 per element grown), and never moves an element: docs/BENCHMARKS.md; the ring serves "
    "retains, and small";
const std::map<std::string, std::string> allowed {
    { "Store<value<Id>, fifo>", std::string(deque_for_fifo) },
    { "Store<value<int>, fifo>", std::string(deque_for_fifo) },
    { "Store<key<Id>, sorted, monotone>", std::string(tree_for_changing_order) },
    { "Store<key<Id>, sorted>", std::string(tree_for_changing_order) },
    { "Store<key<Id>, value<Id>, multi, sorted>", std::string(tree_for_changing_order) },
    { "Store<key<Id>, value<Id>, sorted>", std::string(tree_for_changing_order) },
    { "Store<key<Id>, value<Id>, sorted, monotone>", std::string(tree_for_changing_order) },
    { "Store<key<int>, sorted, monotone>", std::string(tree_for_changing_order) },
    { "Store<key<int>, sorted, order_by<P>>", std::string(tree_for_changing_order) },
    { "Store<key<int>, sorted>", std::string(tree_for_changing_order) },
    { "Store<key<int>, value<int>, multi, sorted>", std::string(tree_for_changing_order) },
    { "Store<key<int>, value<int>, sorted>", std::string(tree_for_changing_order) },
    { "Store<key<int>, value<int>, sorted, monotone>", std::string(tree_for_changing_order) },
    { "Store<key<int>, value<int>, sorted, order_by<P>>", std::string(tree_for_changing_order) },
    { "Store<key<std::string>, sorted, monotone>", std::string(tree_for_changing_order) },
    { "Store<key<std::string>, sorted>", std::string(tree_for_changing_order) },
    { "Store<key<std::string>, value<double>, multi, sorted>", std::string(tree_for_changing_order) },
    { "Store<key<std::string>, value<double>, sorted>", std::string(tree_for_changing_order) },
    { "Store<key<std::string>, value<double>, sorted, monotone>", std::string(tree_for_changing_order) },
    { "Store<key<Store<key<int>, sorted, unique, shrinkable>>, sorted>", std::string(tree_for_changing_order) },
    { "Store<key<index<Id>>, sorted>", std::string(tree_for_changing_order) },
};

}  // namespace

TEST(Registry, TheChosenCandidateAnswersNoMoreThanAnotherServingOne)
{
    auto violations = std::map<std::string, std::string>();
    int requests = 0;
    urd::test::for_each_group(
        [&](const char* /*title*/, auto list)
        {
            urd::test::for_each_request(list,
                                        [&]<typename R>(std::type_identity<R>)
                                        {
                                            using S = typename R::spec;
                                            using List = typename urd::detail::ordered_for<S, typename urd::detail::candidates_for<S>::type>::type;
                                            const auto all = candidates<S, List>::list();
                                            ++requests;
                                            const candidate* chosen = nullptr;
                                            const candidate* best = nullptr;
                                            for (const auto& each : all)
                                            {
                                                if (!each.serves)
                                                    continue;
                                                if (chosen == nullptr)
                                                    chosen = &each;
                                                if (best == nullptr || each.uninvited < best->uninvited)
                                                    best = &each;
                                            }
                                            if (chosen != nullptr && best->uninvited < chosen->uninvited)
                                                violations[R::describe()] = std::string(chosen->id) + " answers " + std::to_string(chosen->uninvited)
                                                                            + " unasked categories, " + std::string(best->id) + " only "
                                                                            + std::to_string(best->uninvited);
                                        });
        });
    EXPECT_GT(requests, 150);
    for (const auto& [request, what] : violations)
        EXPECT_TRUE(allowed.contains(request)) << request << ": " << what;
    for (const auto& [request, reason] : allowed)
        EXPECT_TRUE(violations.contains(request)) << request << " no longer surprises; drop it from the allowed list (" << reason << ")";
}

// The guarantee that is also a demand: a container that stores by index needs the keys it was promised. Selection
// enforces it (serves_by_traits, include/urd/registry.hpp); this checks it stays enforced for every request of the
// grid and every candidate the registry would accept, so that a change to selection or to a container's traits that
// lets a dense container take a request with open keys is caught here before it reaches a user. Spelled out rather
// than through demands_met, so that it is no tautology against the function it guards.
TEST(Registry, NoServingCandidateDemandsWhatTheRequestDidNotPromise)
{
    int requests = 0;
    urd::test::for_each_group(
        [&](const char* /*title*/, auto list)
        {
            urd::test::for_each_request(list,
                                        [&]<typename R>(std::type_identity<R>)
                                        {
                                            using S = typename R::spec;
                                            using List = typename urd::detail::ordered_for<S, typename urd::detail::candidates_for<S>::type>::type;
                                            ++requests;
                                            for (const auto& each : candidates<S, List>::list())
                                                EXPECT_TRUE(!each.serves || !each.demands_dense || S::urd_properties.has(urd::tag::dense))
                                                    << R::describe() << ": " << each.id << " stores by index, but the request did not promise dense keys";
                                        });
        });
    EXPECT_GT(requests, 150);
}

// The claims the registry makes, derived from the costs (benchmarks/comparisons.hpp): none in the wrong order, and every
// trade-off with its reason. A trade-off is measured too (benchmarks/record.py); the reason says why the chosen one
// is the default all the same.
namespace
{

const std::map<std::string, std::string, std::less<>> trade_offs {
    { "flat_set > recycling_tree_set", "a sorted vector inserts and erases in O(n) where a tree takes O(log n), in under a third of the memory; it is chosen only for small or frozen requests, where it is also faster to search and iterate" },
    { "flat_set > indexed_set", "a frozen contiguous set: binary search in O(log n) against a hash lookup, for a sorted array without a table of positions" },
    { "dense_set > sparse_set", "a bitset clears and iterates in O(U/64) where a sparse set takes O(1) and O(n), in U bits against two words per index; constant_clear asks for the sparse set" },
    { "dense_set > ordered_sparse_set", "as dense_set > sparse_set; insertion_ordered asks for the ordered one" },
    { "tracked_dense_set > sparse_set", "proportional_clear: the bitset clears the words it touched, about O(n/64), in U bits against two words per index" },
    { "tracked_dense_set > ordered_sparse_set", "as tracked_dense_set > sparse_set" },
    { "dense_map > generation_map", "a slot per index clears in O(U) where generations clear in O(1), at a generation per slot and a check on every lookup; constant_clear asks for generations" },
    { "dense_map > stable_generation_map", "as dense_map > generation_map" },
    { "dense_map > thread_safe_generation_map", "as dense_map > generation_map" },
    { "dense_map > layered_generation_map", "as dense_map > generation_map" },
    { "stable_dense_map > stable_generation_map", "as dense_map > generation_map" },
    { "stable_dense_map > thread_safe_generation_map", "as dense_map > generation_map" },
    { "layered > layered_generation_map", "as dense_map > generation_map, layered" },
    { "dense_map > ordered_map", "a slot per index against keys in insertion order with values beside them: O(1) erase and less memory, for an O(U) clear and iteration; insertion_ordered asks for the ordered map" },
    { "posting_map > unordered_multimap", "a vector of values per key: erasing a key is O(its values), and the values of a key are one span; nodes erase in O(1) each" },
    { "posting_map > multimap", "a vector of values per key: O(1) insert and lookup against O(log n), erasing a key O(its values)" },
    { "posting_map > flat_multimap", "a vector of values per key: O(1) insert and lookup against O(n) and O(log n), erasing a key O(its values)" },
    { "vector > row_pool", "a vector of rows erases in O(n) where a row pool keeps the row for reuse in O(1), at a kept row per erased one; retains asks for the pool" },
    { "jagged > vector", "frozen rows in one array per level, a row appended in O(its length), against a vector of row Stores each owning its storage" },
    { "row_interner > tree_vector_set", "each row in full, read in O(1), against shared nodes read in O(r); structural_sharing asks for the tree" },
    { "row_interner > tree_bitset_set", "as row_interner > tree_vector_set, for bit rows" },
    { "thread_safe_row_interner > tree_vector_set", "as row_interner > tree_vector_set" },
};

}  // namespace

TEST(Registry, EveryClaimIsSettledMeasuredOrReasoned)
{
    auto used = std::set<std::string>();
    for (const auto& [id, found] : urd::test::all_comparisons())
    {
        EXPECT_NE(found.result, urd::test::outcome::wrong_order)
            << id << ": " << found.other << " grows slower and takes no more memory than " << found.chosen
            << " (tests/complexity.hpp); reorder the registry, or correct the costs (" << found.requests.front() << ")";
        if (found.result != urd::test::outcome::trade_off)
            continue;
        const auto pair = found.chosen + " > " + found.other;
        used.insert(pair);
        EXPECT_TRUE(trade_offs.contains(pair)) << pair << ": a trade-off (" << id << ") with no reason in trade_offs (tests/registry.cpp)";
    }
    for (const auto& [pair, reason] : trade_offs)
        EXPECT_TRUE(used.contains(pair)) << pair << " is no trade-off any more; drop its reason (" << reason << ")";
}
