// What a Store costs to compile beyond its container (tests/cost/overhead.py): URD_OVERHEAD_TYPES distinct element types,
// each held in a set, a sequence and a map, by Store or, with URD_OVERHEAD_DIRECT, by the container each Store
// chooses, named directly. The Store build checks that these are the containers it chose, so the two builds hold
// the same containers and differ only by urd.

#include "urd/store.hpp"

#include <compare>
#include <concepts>
#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace
{

/// The element types: one unless the test asks for more.
#if defined(URD_OVERHEAD_TYPES)
constexpr int types = URD_OVERHEAD_TYPES;
#else
constexpr int types = 1;
#endif

template<int I>
struct Element
{
    int value;
    friend bool operator==(const Element&, const Element&) = default;
    friend auto operator<=>(const Element&, const Element&) = default;
};

template<int I>
using DirectSet = std::set<Element<I>, urd::Less<Element<I>>>;
template<int I>
using DirectSequence = std::vector<Element<I>>;
template<int I>
using DirectMap = std::map<Element<I>, int, urd::Less<Element<I>>>;

#if defined(URD_OVERHEAD_DIRECT)
template<int I>
using Set = DirectSet<I>;
template<int I>
using Sequence = DirectSequence<I>;
template<int I>
using Map = DirectMap<I>;
#else
template<int I>
using Set = urd::Store<urd::set<Element<I>>>;
template<int I>
using Sequence = urd::Store<urd::sequence<Element<I>>>;
template<int I>
using Map = urd::Store<urd::map<Element<I>, int>>;
#endif

/// Holds, clears and sizes one container of each kind, as code that uses a Store does.
template<int I>
std::size_t hold()
{
#if !defined(URD_OVERHEAD_DIRECT)
    static_assert(std::same_as<typename Set<I>::representation, DirectSet<I>>);
    static_assert(std::same_as<typename Sequence<I>::representation, DirectSequence<I>>);
    static_assert(std::same_as<typename Map<I>::representation, DirectMap<I>>);
#endif
    auto set = Set<I>();
    auto sequence = Sequence<I>();
    auto map = Map<I>();
    set.clear();
    sequence.clear();
    map.clear();
    return set.size() + sequence.size() + map.size();
}

template<int... I>
std::size_t hold_all(std::integer_sequence<int, I...> /*types*/)
{ return (hold<I>() + ...); }

}  // namespace

int main() { return static_cast<int>(hold_all(std::make_integer_sequence<int, types> {})); }
