// The cost table (tests/complexity.hpp) covers what the registry can choose: every candidate that serves a request
// of the grid has a cost for every operation the request's Store offers, and every row of the table is used.

#include "complexity.hpp"

#include "grid.hpp"
#include "urd/registry.hpp"
// Every family: facet_switches reads the representations.
#include "urd/use/contiguous.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/layers.hpp"
#include "urd/use/nodes.hpp"
#include "urd/use/pools.hpp"
#include "urd/use/queues.hpp"
#include "urd/use/records.hpp"
#include "urd/use/scopes.hpp"
#include "urd/use/sequences.hpp"
#if defined(URD_HAS_GTL)
#include "urd/use/parallel.hpp"
#endif

#include <cstddef>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <utility>

namespace
{

using urd::test::op;

template<typename R, typename S>
struct serving
{
    static constexpr bool value = urd::detail::serves<R, S>;
};

/// For each serving candidate of S: the operations offered without a cost, as "name op"; and the table rows used.
template<typename S, typename List>
struct audit;
template<typename S, typename... Rs>
struct audit<S, urd::detail::type_list<Rs...>>
{
    template<typename R>
    static void one(std::set<std::string>& missing, std::set<std::pair<std::string, op>>& used)
    {
        if constexpr (serving<R, S>::value)
        {
            const auto offers = urd::test::offered<R, S>();
            for (std::size_t each = 0; each < offers.size(); ++each)
            {
                if (!offers[each])
                    continue;
                const auto what = static_cast<op>(each);
                if (!urd::test::cost_of<R>(what))
                    missing.insert(std::string(urd::test::name<R>::id) + " " + std::string(urd::test::op_names[each]));
                used.emplace(std::string(urd::test::name<R>::id), what);
                if constexpr (requires { typename urd::test::first_part<R>::type; })
                    used.emplace(std::string(urd::test::name<typename urd::test::first_part<R>::type>::id), what);
            }
        }
    }
    static void run(std::set<std::string>& missing, std::set<std::pair<std::string, op>>& used) { (one<Rs>(missing, used), ...); }
};

}  // namespace

TEST(Complexity, EveryServingCandidateHasACostForWhatItOffers)
{
    auto missing = std::set<std::string>();
    auto used = std::set<std::pair<std::string, op>>();
    urd::test::for_each_group(
        [&](const char* /*title*/, auto list)
        {
            urd::test::for_each_request(list,
                                        [&]<typename Q>(std::type_identity<Q>)
                                        {
                                            using S = typename Q::spec;
                                            using List = typename urd::detail::ordered_for<S, typename urd::detail::candidates_for<S>::type>::type;
                                            audit<S, List>::run(missing, used);
                                        });
        });
    for (const auto& each : missing)
        ADD_FAILURE() << each << ": no cost in tests/complexity.hpp";
    for (const auto& each : urd::test::cost_table)
        EXPECT_TRUE(used.contains({ std::string(each.representation), each.operation }))
            << each.representation << " " << urd::test::op_names[static_cast<std::size_t>(each.operation)]
            << ": no request of the grid offers it; drop the row (tests/complexity.hpp)";
}
