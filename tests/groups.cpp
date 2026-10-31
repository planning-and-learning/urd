// The four groups of tags (kind, semantics, costs, hints; docs/language/index.rst), checked against the code over the
// request grid: each request with a stated tag, against the same request without it (its category back at its root,
// the request rules applied again). A group is written by hand per category (urd/tag_synopsis.hpp); these check the
// three groups the code can tell apart from semantics:
//
// - kind: without the tag, the Store is another kind (a queue, a pool, a partition or none), or offers other operations;
// - hints: without the tag, the Store has the same facets and the same guarantees; only the container may differ;
// - costs: without the tag, the Store has the same facets, and its guarantees differ only in the tag's category.
//
// And every public tag earns its place: stating it changes some resolution or some Store's interface in the grid.
// Selection here is by value (urd/registry/select.hpp), so no container is named and no container header included.

#include "complexity.hpp"
#include "grid.hpp"
#include "names.hpp"
#include "urd/registry.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{

using urd::tag;
using urd::detail::facet_id;

/// What the checks read of a spec: its kind, its facets, the operations it offers, its guarantees, and the container
/// it resolves to (by id; "" for none).
struct seen
{
    bool queue = false;
    bool pool = false;
    bool partition = false;
    std::array<bool, static_cast<std::size_t>(facet_id::count)> facets {};
    std::array<bool, static_cast<std::size_t>(urd::test::op::count)> offers {};
    urd::detail::property_set properties = urd::detail::property_set::roots();
    std::string_view container;
};
template<typename S>
seen seen_of()
{
    auto result = seen {};
    result.queue = S::is_queue;
    result.pool = S::is_pool;
    result.partition = S::is_partition;
    for (std::size_t each = 0; each < result.facets.size(); ++each)
        result.facets[each] = urd::test::spec_switches<S>::on(static_cast<facet_id>(each));
    result.offers = urd::test::offered<S>();
    result.properties = S::urd_properties;
    result.container = urd::detail::choose<S>::id;
    return result;
}

/// One pair: a request with a stated tag, and the same request without it.
struct pair
{
    std::string request;
    tag stated;
    seen with;
    seen without;
};

template<typename... Ps>
struct pairs_of_stated;
template<typename... Ps>
struct pairs_of_stated<urd::detail::type_list<Ps...>>
{
    using checked = urd::detail::to_spec<Ps...>;
    template<std::size_t... I>
    static void add(std::vector<pair>& into, const std::string& request, std::index_sequence<I...> /*stated*/)
    {
        (into.push_back({ request,
                          checked::stated_tags.tags[I],
                          seen_of<typename checked::type>(),
                          seen_of<typename checked::template without<checked::stated_tags.tags[I]>>() }),
         ...);
    }
    static void add(std::vector<pair>& into, const std::string& request) { add(into, request, std::make_index_sequence<checked::stated_tags.count> {}); }
};

/// Every pair of the grid.
std::vector<pair> grid_pairs()
{
    auto result = std::vector<pair>();
    urd::test::for_each_group(
        [&](const char* /*title*/, auto list)
        {
            urd::test::for_each_request(list,  // qualified: lookup by argument would complete every Store named in a request
                                        [&]<typename Q>(std::type_identity<Q>) { pairs_of_stated<typename Q::stated>::add(result, Q::describe()); });
        });
    return result;
}

std::string_view group_of(tag of) { return urd::test::category_vocabulary[urd::detail::index(urd::detail::root_of(of))].group; }

std::string named(const pair& each) { return std::string(urd::test::tag_name(each.stated)) + " in " + each.request; }

/// The guarantees, with the category of `of` back at its root on both sides.
bool equal_but(urd::detail::property_set a, urd::detail::property_set b, tag of)
{
    a.put(urd::detail::root_of(of));
    b.put(urd::detail::root_of(of));
    return a == b;
}

TEST(Groups, KindTagsChangeTheKindOrTheOperations)
{
    for (const auto& each : grid_pairs())
    {
        if (group_of(each.stated) != "kind")
            continue;
        const bool kind_changes = each.with.queue != each.without.queue || each.with.pool != each.without.pool || each.with.partition != each.without.partition;
        // A priority queue with handles is still a priority queue: its handles add update and erase by handle to its
        // queue access, which the operations compared here do not list.
        const bool queue_handles = each.stated == tag::handles && each.with.queue;
        EXPECT_TRUE(kind_changes || each.with.offers != each.without.offers || queue_handles) << named(each);
    }
}

TEST(Groups, HintsChangeNoFacetAndNoGuarantee)
{
    for (const auto& each : grid_pairs())
    {
        if (group_of(each.stated) != "hints")
            continue;
        EXPECT_EQ(each.with.facets, each.without.facets) << named(each);
        EXPECT_TRUE(equal_but(each.with.properties, each.without.properties, each.stated)) << named(each);
    }
}

TEST(Groups, CostsChangeNoFacetAndNoOtherGuarantee)
{
    for (const auto& each : grid_pairs())
    {
        if (group_of(each.stated) != "costs")
            continue;
        EXPECT_EQ(each.with.facets, each.without.facets) << named(each);
        EXPECT_TRUE(equal_but(each.with.properties, each.without.properties, each.stated)) << named(each);
    }
}

TEST(Groups, EveryPublicTagChangesSomeResolutionOrInterface)
{
    auto earned = std::set<tag>();
    for (const auto& each : grid_pairs())
        if (each.with.container != each.without.container || each.with.facets != each.without.facets || each.with.offers != each.without.offers)
            earned.insert(each.stated);
    for (std::size_t each = 0; each < urd::detail::tag_count; ++each)
    {
        const auto of = static_cast<tag>(each);
        if (urd::detail::root_of(of) == of || !urd::detail::statable(of))
            continue;
        EXPECT_TRUE(earned.contains(of)) << urd::test::tag_name(of)
                                         << ": stated in no request of the grid where it changes the container or the interface (tests/grid.hpp)";
    }
}

/// Whether stating `sparse` changes what a request resolves to: the check that decides whether sparse can be internal
/// (claude.taxonomy.md, change 3). Every grid request that leaves the keys open, with sparse added.
template<typename... Ps>
std::string_view resolved_with_sparse(urd::detail::type_list<Ps...> /*stated*/)
{ return urd::detail::choose<typename urd::detail::to_spec<Ps..., urd::sparse>::type>::id; }

TEST(Groups, SparseChangesNoResolution)
{
    urd::test::for_each_group(
        [&](const char* /*title*/, auto list)
        {
            urd::test::for_each_request(list,
                                        [&]<typename Q>(std::type_identity<Q>)
                                        {
                                            using S = typename Q::spec;
                                            // Stores of Stores are left out: checking such a request completes its
                                            // element Stores, whose containers this file does not include.
                                            if constexpr (urd::has<S, urd::dense> || urd::has<S, urd::sparse> || !(S::is_set || S::is_map)
                                                          || urd::is_store<typename S::key_type> || urd::is_store<typename S::value_type>)
                                                return;
                                            else
                                                EXPECT_EQ(urd::detail::choose<S>::id, resolved_with_sparse(typename Q::stated {})) << Q::describe();
                                        });
        });
}

}  // namespace
