// The denials (include/urd/store/denials.hpp): the tables of which facets declare which names match the facets, so a
// facet that gains or loses a name is caught here; and a denied operation is still absent to `requires`, as a missing
// member is. What a denied call prints is checked by the message-checking compile-fail cases (CMakeLists.txt).

#include "urd/store/denials.hpp"

#include "urd/store.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/layers.hpp"
#include "urd/use/nodes.hpp"
#include "urd/use/pools.hpp"
#include "urd/use/queues.hpp"
#include "urd/use/records.hpp"
#include "urd/use/scopes.hpp"
#include "urd/use/sequences.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>

namespace
{

using urd::key;
using urd::Store;
using urd::value;
using urd::detail::facet_id;
namespace facet = urd::detail::facet;

// Whether a facet F declares a name: with a second base that declares it too, the name's lookup is ambiguous.
// NOLINTBEGIN(cppcoreguidelines-macro-usage): one detector per name, forty times over
#define URD_TEST_DECLARES(name)          \
    struct names_##name                  \
    {                                    \
        void name();                     \
    };                                   \
    template<typename F>                 \
    struct both_##name : F, names_##name \
    {                                    \
    };                                   \
    template<typename F>                 \
    concept declares_##name = !requires(both_##name<F>& both) { both.name(); };

URD_TEST_DECLARES(insert)
URD_TEST_DECLARES(emplace)
URD_TEST_DECLARES(push_back)
URD_TEST_DECLARES(emplace_back)
URD_TEST_DECLARES(pop_back)
URD_TEST_DECLARES(erase)
URD_TEST_DECLARES(clear)
URD_TEST_DECLARES(find)
URD_TEST_DECLARES(count)
URD_TEST_DECLARES(contains)
URD_TEST_DECLARES(lower_bound)
URD_TEST_DECLARES(upper_bound)
URD_TEST_DECLARES(equal_range)
URD_TEST_DECLARES(begin)
URD_TEST_DECLARES(end)
URD_TEST_DECLARES(cbegin)
URD_TEST_DECLARES(cend)
URD_TEST_DECLARES(rbegin)
URD_TEST_DECLARES(rend)
URD_TEST_DECLARES(at)
URD_TEST_DECLARES(front)
URD_TEST_DECLARES(back)
URD_TEST_DECLARES(position)
URD_TEST_DECLARES(data)
URD_TEST_DECLARES(reserve)
URD_TEST_DECLARES(size)
URD_TEST_DECLARES(empty)
URD_TEST_DECLARES(try_emplace)
URD_TEST_DECLARES(insert_or_assign)
URD_TEST_DECLARES(read)
URD_TEST_DECLARES(update)
URD_TEST_DECLARES(merge)
URD_TEST_DECLARES(values)
URD_TEST_DECLARES(keys)
URD_TEST_DECLARES(push)
URD_TEST_DECLARES(pop)
URD_TEST_DECLARES(top)
URD_TEST_DECLARES(push_scope)
URD_TEST_DECLARES(pop_scope)
URD_TEST_DECLARES(depth)
URD_TEST_DECLARES(bound_in_scope)
URD_TEST_DECLARES(layer_over)
URD_TEST_DECLARES(added)
URD_TEST_DECLARES(removed)
URD_TEST_DECLARES(clear_changes)
URD_TEST_DECLARES(was_added)
URD_TEST_DECLARES(was_removed)
URD_TEST_DECLARES(get)
URD_TEST_DECLARES(view)
URD_TEST_DECLARES(row_size)
URD_TEST_DECLARES(memory_usage)
URD_TEST_DECLARES(acquire)
URD_TEST_DECLARES(same)
URD_TEST_DECLARES(class_size)
URD_TEST_DECLARES(classes)
URD_TEST_DECLARES(reset)
URD_TEST_DECLARES(includes)
URD_TEST_DECLARES(intersects)
URD_TEST_DECLARES(unite)
#undef URD_TEST_DECLARES

struct names_subscript
{
    void operator[](int);
};
template<typename F>
struct both_subscript : F, names_subscript
{
};
template<typename F>
concept declares_subscript = !requires(both_subscript<F>& both) { both[0]; };

// One Store per facet, with that facet on: the facet instantiated for it is what the detectors read.
using Set = Store<key<int>>;
using Sorted = Store<key<int>, urd::sorted>;
using Positional = Store<key<int>, urd::positional>;
using Sequence = Store<value<int>, urd::contiguous>;
using Map = Store<key<int>, value<int>>;
using Multi = Store<key<int>, value<int>, urd::multi>;
using Queue = Store<value<int>, urd::fifo>;
using Scoped = Store<key<int>, value<int>, urd::scoped>;
using Layered = Store<key<int>, urd::layered>;
using Logged = Store<key<int>, urd::change_log>;
using Pool = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique>;
using Slots = Store<value<int>, urd::handles>;
using Partition = Store<key<int>, urd::partition>;
using Ordered = Store<key<int>, value<int>, urd::insertion_ordered>;
using Graph = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>;
using Shaped = Store<value<Store<value<int>>>, urd::fixed_shape>;
struct Point
{
    int x = 0;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Point::x> {}; }
};
using Records = Store<value<Point>, urd::handles, urd::unique, urd::flat>;

template<facet_id Id, typename Facet>
struct entry
{
    static constexpr facet_id id = Id;
    using facet = Facet;
};
/// The Derived type the facets are instantiated for here: not the Store itself, whose own facets already define the
/// friends (operator==, erase_if) a second instantiation would define again.
template<typename S>
struct probe;
template<typename S>
using R_of = typename S::representation;
template<typename S>
using S_of = urd::traits_t<S>;
// The arguments are a name and a type, which parentheses would not leave as such.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,bugprone-macro-parentheses)
#define URD_TEST_FACET(id, Store) entry<facet_id::id, facet::id<probe<Store>, R_of<Store>, S_of<Store>>>
using facets = urd::detail::type_list<URD_TEST_FACET(sizing, Set),
                                      URD_TEST_FACET(iteration, Set),
                                      URD_TEST_FACET(equality, Set),
                                      URD_TEST_FACET(ordering, Sorted),
                                      URD_TEST_FACET(lookup, Set),
                                      URD_TEST_FACET(ordered_lookup, Sorted),
                                      URD_TEST_FACET(reverse_iteration, Sorted),
                                      URD_TEST_FACET(positions, Positional),
                                      URD_TEST_FACET(contiguous_data, Sequence),
                                      URD_TEST_FACET(capacity, Set),
                                      URD_TEST_FACET(growth, Set),
                                      URD_TEST_FACET(removal, Set),
                                      URD_TEST_FACET(values_by_key, Map),
                                      URD_TEST_FACET(multi_values, Multi),
                                      URD_TEST_FACET(set_algebra, Set),
                                      URD_TEST_FACET(queue_access, Queue),
                                      URD_TEST_FACET(scopes, Scoped),
                                      URD_TEST_FACET(layers, Layered),
                                      URD_TEST_FACET(changes, Logged),
                                      URD_TEST_FACET(pool, Pool),
                                      URD_TEST_FACET(records, Records),
                                      URD_TEST_FACET(slots, Slots),
                                      URD_TEST_FACET(grouping, Partition),
                                      URD_TEST_FACET(entries, Ordered),
                                      URD_TEST_FACET(inverse, Graph),
                                      URD_TEST_FACET(shape, Shaped),
                                      URD_TEST_FACET(sorted_ends, Sorted)>;
#undef URD_TEST_FACET

template<std::size_t N>
constexpr bool listed(facet_id id, const std::array<facet_id, N>& ids)
{
    for (const auto each : ids)
        if (each == id)
            return true;
    return false;
}

/// Each name of a group is declared by exactly the facets its table lists.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define URD_TEST_GROUP(group, ...)                                                                                         \
    template<typename List>                                                                                                \
    struct check_##group;                                                                                                  \
    template<typename... E>                                                                                                \
    struct check_##group<urd::detail::type_list<E...>>                                                                     \
    {                                                                                                                      \
        template<typename F>                                                                                               \
        static constexpr bool declared = __VA_ARGS__;                                                                      \
        static constexpr bool ok = ((declared<typename E::facet> == listed(E::id, urd::detail::declares_##group)) && ...); \
    };                                                                                                                     \
    static_assert(check_##group<facets>::ok, "store/denials.hpp: declares_" #group " does not match the facets");

URD_TEST_GROUP(insert, declares_insert<F>)
URD_TEST_GROUP(emplace, declares_emplace<F>)
URD_TEST_GROUP(appending, declares_push_back<F> || declares_emplace_back<F>)
URD_TEST_GROUP(pop_back, declares_pop_back<F>)
URD_TEST_GROUP(erase, declares_erase<F>)
URD_TEST_GROUP(clear, declares_clear<F>)
URD_TEST_GROUP(lookup, declares_find<F> || declares_count<F> || declares_contains<F>)
URD_TEST_GROUP(ordered_lookup, declares_lower_bound<F> || declares_upper_bound<F> || declares_equal_range<F>)
URD_TEST_GROUP(iteration, declares_begin<F> || declares_end<F> || declares_cbegin<F> || declares_cend<F>)
URD_TEST_GROUP(reverse_iteration, declares_rbegin<F> || declares_rend<F>)
URD_TEST_GROUP(subscript, declares_subscript<F> || declares_at<F>)
URD_TEST_GROUP(ends, declares_front<F> || declares_back<F>)
URD_TEST_GROUP(position, declares_position<F>)
URD_TEST_GROUP(data, declares_data<F>)
URD_TEST_GROUP(reserve, declares_reserve<F>)
URD_TEST_GROUP(sizing, declares_size<F> || declares_empty<F>)
URD_TEST_GROUP(values_by_key, declares_try_emplace<F> || declares_insert_or_assign<F> || declares_read<F>)
URD_TEST_GROUP(update, declares_update<F>)
URD_TEST_GROUP(merge, declares_merge<F>)
URD_TEST_GROUP(values, declares_values<F>)
URD_TEST_GROUP(keys, declares_keys<F>)
URD_TEST_GROUP(queue, declares_push<F> || declares_pop<F> || declares_top<F>)
URD_TEST_GROUP(scopes, declares_push_scope<F> || declares_pop_scope<F> || declares_depth<F> || declares_bound_in_scope<F>)
URD_TEST_GROUP(layers, declares_layer_over<F>)
URD_TEST_GROUP(changes, declares_added<F> || declares_removed<F> || declares_clear_changes<F> || declares_was_added<F> || declares_was_removed<F>)
URD_TEST_GROUP(view, declares_view<F>)
URD_TEST_GROUP(rows, declares_get<F> || declares_row_size<F> || declares_memory_usage<F>)
URD_TEST_GROUP(acquire, declares_acquire<F>)
URD_TEST_GROUP(grouping, declares_same<F> || declares_class_size<F> || declares_classes<F>)
URD_TEST_GROUP(reset, declares_reset<F>)
URD_TEST_GROUP(set_algebra, declares_includes<F> || declares_intersects<F> || declares_unite<F>)
#undef URD_TEST_GROUP
// NOLINTEND(cppcoreguidelines-macro-usage)

// A denied operation is as absent to `requires` as a missing member: generic code that probes sees nothing.
template<typename S>
concept can_insert = requires(S store) { store.insert(1); };
template<typename S>
concept can_subscript = requires(S store) { store[0]; };
template<typename S>
concept can_pop_back = requires(S store) { store.pop_back(); };
template<typename S>
concept can_iterate = requires(S store) { store.begin(); };
template<typename S>
concept can_search = requires(S store) { store.lower_bound(1); };
template<typename S>
concept can_push = requires(S store) { store.push(1); };
template<typename S>
concept can_list_values = requires(S store) { store.values(1); };
static_assert(!can_insert<Store<key<int>, urd::frozen>> && !can_subscript<Set> && !can_pop_back<Store<value<int>, urd::monotone>> && !can_iterate<Queue>
              && !can_iterate<Pool> && !can_search<Set> && !can_push<Sequence> && !can_list_values<Map>);
// And what the facets offer is still there.
static_assert(can_insert<Set> && can_search<Sorted> && can_push<Queue> && can_iterate<Set> && can_list_values<Multi>);

// A Store with denials is still no larger than its representation: they are empty bases.
static_assert(sizeof(Store<key<int>, urd::frozen>) == sizeof(Store<key<int>, urd::frozen>::representation));
static_assert(sizeof(Queue) == sizeof(Queue::representation));

}  // namespace

TEST(Denials, TablesMatchTheFacets)
{
    // The static_asserts above are the test; this keeps the binary from running zero tests.
    SUCCEED();
}
