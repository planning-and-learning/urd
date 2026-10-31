// The registry audit: for every request of the grid (tests/grid.hpp), every registered candidate that serves it, and
// whether the one chosen (the first serving one) answers more categories the request left open than another serving
// one does. Such a choice is a surprise unless a benchmark backs it: each one is listed below with its reason.

#include "urd/registry.hpp"

#include "complexity.hpp"
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

/// How many costly guarantees `offered` gives that the request S did not ask for.
template<typename S>
constexpr int uninvited(const urd::detail::property_set& offered)
{
    int count = 0;
    for (const auto each : urd::test::costly)
        if (!S::urd_properties.has(each) && offered.has(each))
            ++count;
    return count;
}

struct candidate
{
    std::string_view id;
    int uninvited;
    /// Whether the container stores by index: a demand on the keys, not a guarantee alone.
    bool demands_dense;
};

/// The containers serving S, in the order they are preferred, read from their declarations.
template<typename S>
std::vector<candidate> candidates()
{
    auto result = std::vector<candidate>();
    urd::test::for_each_serving<S>(
        [&]<typename Decl>(std::type_identity<Decl>)
        {
            const auto offered = Decl::offers(urd::detail::features_of<S>())->properties;
            result.push_back({ Decl::id, uninvited<S>(offered), offered.has(urd::tag::dense) });
        });
    return result;
}

/// The surprises the registry's order makes on purpose: the request, and the benchmark or reason behind the order.
const std::string_view tree_for_changing_order =
    "a sorted set or map that changes is a tree (nodes, O(log n) inserts); the flat ones serve only with frozen or small: docs/BENCHMARKS.md";
const std::string_view deque_for_fifo =
    "a deque is level with a ring over one vector on a small frontier and faster on a large one and at growing (3.9 against 5.4 ns per "
    "step with a million queued, 2.2 against 6.7 per element grown), and never moves an element: docs/BENCHMARKS.md; the ring serves "
    "retains, and small";
const std::map<std::string, std::string>& allowed()
{
    static const auto reasons = std::map<std::string, std::string> {
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
    return reasons;
}

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
                                            const auto all = candidates<S>();
                                            ++requests;
                                            const candidate* chosen = nullptr;
                                            const candidate* best = nullptr;
                                            for (const auto& each : all)
                                            {
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
        EXPECT_TRUE(allowed().contains(request)) << request << ": " << what;
    for (const auto& [request, reason] : allowed())
        EXPECT_TRUE(violations.contains(request)) << request << " no longer surprises; drop it from the allowed list (" << reason << ")";
}

// The guarantee that is also a demand: a container that stores by index needs the keys it was promised. Selection
// enforces it (demands_of, urd/property_set.hpp); this checks it stays enforced for every request of the
// grid and every candidate the registry would accept, so that a change to selection or to a container's traits that
// lets a dense container take a request with open keys is caught here before it reaches a user. Spelled out rather
// than through demands_of, so that it is no tautology against the function it guards.
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
                                            ++requests;
                                            for (const auto& each : candidates<S>())
                                                EXPECT_TRUE(!each.demands_dense || S::urd_properties.has(urd::tag::dense))
                                                    << R::describe() << ": " << each.id << " stores by index, but the request did not promise dense keys";
                                        });
        });
    EXPECT_GT(requests, 150);
}
