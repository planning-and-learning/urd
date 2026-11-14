// Why a container does not serve a request (urd/registry/scan.hpp, shortfall_of): what an agent is told. Checked at
// compile time: the shortfall is none exactly where the container serves, and names what it lacks and what the
// request must still state or promise.

#include "urd/store.hpp"

#include <gtest/gtest.h>
#include <string_view>

namespace
{

template<typename Kind, typename... Tags>
using spec = typename urd::detail::to_spec<Kind, Tags...>::type;

/// The shortfall of the container `id` of S's kind for S.
template<typename S>
constexpr urd::detail::shortfall shortfall_for(std::string_view id)
{
    for (const auto& entry : urd::detail::table_of<typename S::kind>)
        if (entry.id == id)
            return urd::detail::shortfall_of(entry, urd::detail::shape_of<S>);
    throw "shortfall_for: no such container in the kind's table";
}

/// Whether every container of S's kind has no shortfall exactly where it serves S.
template<typename S>
constexpr bool agrees()
{
    for (const auto& entry : urd::detail::table_of<typename S::kind>)
        if (urd::detail::serves(entry, urd::detail::shape_of<S>) != urd::detail::shortfall_of(entry, urd::detail::shape_of<S>).none())
            return false;
    return true;
}

using sorted_set = spec<urd::set<int>, urd::sorted>;
using bag = spec<urd::multiset<int>>;

static_assert(agrees<sorted_set>());
static_assert(agrees<bag>());
static_assert(agrees<spec<urd::set<urd::Store<urd::set<int>>>>>());
static_assert(agrees<spec<urd::map<int, int>, urd::frozen>>());
static_assert(agrees<spec<urd::queue<int>, urd::by_priority>>());

// A hash table lacks the order a sorted set asks for, and nothing else.
constexpr auto hashed = shortfall_for<sorted_set>("unordered_flat_set");
static_assert(hashed.lacks.count == 1 && hashed.lacks.has(urd::order::sorted) && hashed.needs.count == 0 && hashed.held.count == 0);

// A dense set demands dense keys: the request must state them.
constexpr auto bits = shortfall_for<sorted_set>("dense_set");
static_assert(bits.lacks.count == 0 && bits.needs.has(urd::keys::dense));

// A set keeps each key once; a multiset that leaves duplicates open must state unique for it (an exact category).
constexpr auto once = shortfall_for<bag>("flat_set");
static_assert(once.lacks.count == 0 && once.needs.count == 1 && once.needs.has(urd::uniqueness::unique));

}  // namespace

TEST(Shortfall, IsCheckedAtCompileTime) { SUCCEED(); }
