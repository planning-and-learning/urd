// The registry audit: for every request of the grid (tests/grid.hpp), every registered candidate that serves it, and
// whether the one chosen (the first serving one) answers more categories the request left open than another serving
// one does. Such a choice is a surprise unless a benchmark backs it: each one is listed below with its reason.

#include "urd/registry.hpp"

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
    static std::vector<candidate> list() { return { candidate { urd::test::name<Rs>::id, serving<Rs, S>::value, uninvited<S, Rs>() }... }; }
};

/// The surprises the registry's order makes on purpose: the request, and the benchmark or reason behind the order.
const std::string_view tree_for_changing_order =
    "a sorted set or map that changes is a tree (nodes, O(log n) inserts); the flat ones serve only with frozen or small: docs/BENCHMARKS.md";
const std::map<std::string, std::string> allowed {
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
