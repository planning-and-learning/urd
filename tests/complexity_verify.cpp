// The cost table (tests/complexity.hpp) checked by counting the work done: every entry verified by counting is run,
// through the Store over the representation, at two sizes 64 times apart, on an element that counts its copies, moves
// and the calls to its identity (every hash, equality and order urd makes). The ratio of the work per operation
// gives the class: O(1) stays level, O(log n) grows by the ratio of the logarithms, O(n) by the ratio of the sizes.
// Entries whose work no element operation shows (index and bit containers, clearing, iterating) are checked from
// measured times instead (benchmarks/record.py).
//
// Release only: the linear cases do millions of operations.

// Every container, before anything that names a Store: a Store's container is resolved where the Store is
// first named, and only containers whose headers precede that are found defined.
#include "urd/repr/addressable_heap.hpp"
#include "urd/repr/arena.hpp"
#include "urd/repr/bidirectional_csr.hpp"
#include "urd/repr/bucket_queue.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/csr_multimap.hpp"
#include "urd/repr/dense_multimap.hpp"
#include "urd/repr/dense_set.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/entry_ordered.hpp"
#include "urd/repr/flat_map.hpp"
#include "urd/repr/flat_set.hpp"
#include "urd/repr/index_map.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/jagged.hpp"
#include "urd/repr/layer.hpp"
#include "urd/repr/layered_pool.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/parallel_hash.hpp"
#include "urd/repr/pools.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/queue.hpp"
#include "urd/repr/records.hpp"
#include "urd/repr/recycling.hpp"
#include "urd/repr/ring_fifo.hpp"
#include "urd/repr/row_pool.hpp"
#include "urd/repr/scoped.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/slots.hpp"
#include "urd/repr/sparse_set.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/repr/tree_bitset.hpp"
#include "urd/repr/tree_vector.hpp"
#include "urd/repr/unordered_flat_map.hpp"
#include "urd/repr/unordered_flat_set.hpp"
// The tables and the grid.
#include "complexity.hpp"
#include "grid.hpp"
#include "urd/registry.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

using urd::frozen;
using urd::key;
using urd::value;
using urd::test::growth;
using urd::test::op;

/// An element that counts the work done on it: copies, moves, and calls to its identity.
struct Probe
{
    std::uint64_t id = 0;
    static inline std::uint64_t work = 0;

    Probe() = default;
    explicit Probe(std::uint64_t value) : id(value) {}
    Probe(const Probe& other) : id(other.id) { ++work; }
    Probe(Probe&& other) noexcept : id(other.id) { ++work; }
    Probe& operator=(const Probe& other)
    {
        id = other.id;
        ++work;
        return *this;
    }
    Probe& operator=(Probe&& other) noexcept
    {
        id = other.id;
        ++work;
        return *this;
    }
    ~Probe() = default;

    [[nodiscard]] std::tuple<std::uint64_t> identifying_members() const
    {
        ++work;
        return { id };
    }
};

inline constexpr auto probe_value = [](const Probe& element) { return element.id; };
inline constexpr auto entry_value = [](const Probe& /*key*/, const int& value) { return value; };

/// `count` distinct probes in random order (an odd multiplier is a bijection), and as many absent ones.
std::vector<Probe> probes(std::size_t count, bool absent = false)
{
    auto result = std::vector<Probe>();
    result.reserve(count);
    for (std::uint64_t each = 0; each < count; ++each)
        result.emplace_back((2 * each + (absent ? 1 : 0)) * 0x9E3779B97F4A7C15ULL);
    std::ranges::shuffle(result, std::mt19937(7));
    return result;
}

using Row = std::vector<Probe>;
std::vector<Row> rows(std::size_t count, std::size_t length)
{
    auto result = std::vector<Row>(count);
    for (std::size_t each = 0; each < count; ++each)
        for (std::size_t at = 0; at < length; ++at)
            result[each].emplace_back((each * length + at) * 0x9E3779B97F4A7C15ULL);
    return result;
}

template<typename S, typename Store>
void add(Store& store, const Probe& element)
{
    if constexpr (S::is_map)
        store.emplace(element, 0);
    else if constexpr (S::is_queue)
        store.push(element);
    else if constexpr (requires { store.push_back(element); })
        store.push_back(element);
    else
        store.insert(element);
}

/// The work per operation of `what` on a Store of R for S at size n; none where this test cannot run it.
template<typename R, typename S>
std::optional<double> work(op what, std::size_t size)
{
    using Store = urd::detail::Store<R, S>;
    constexpr std::size_t probed = 256;
    // Few enough that the Store keeps about its size while they run.
    constexpr std::size_t erased = 16;
    // A priority queue's worst push is one below everything held: descending order makes every push one.
    auto elements = probes(size);
    if constexpr (urd::has<S, urd::by_priority>)
        if (what == op::push)
            std::ranges::sort(elements, [](const Probe& lhs, const Probe& rhs) { return lhs.id > rhs.id; });
    const auto build = [&]
    {
        if constexpr (urd::has<S, frozen>)
            return Store(elements.begin(), elements.end());
        else
        {
            auto store = Store();
            for (const auto& each : elements)
                add<S>(store, each);
            return store;
        }
    };
    const auto per = [](std::uint64_t total, std::size_t count) { return static_cast<double>(total) / static_cast<double>(count); };
    switch (what)
    {
        case op::insert:
        case op::push:
            if constexpr (urd::has<S, frozen>)
                return std::nullopt;
            else
            {
                auto store = Store();
                Probe::work = 0;
                for (const auto& each : elements)
                    add<S>(store, each);
                return per(Probe::work, size);
            }
        case op::lookup:
            if constexpr (requires(const Store& held, const Probe& element) { held.contains(element); })
            {
                const auto store = build();
                const auto absent = probes(probed, true);
                Probe::work = 0;
                for (std::size_t each = 0; each < probed; ++each)
                    static_cast<void>(store.contains(each % 2 == 0 ? elements[each % size] : absent[each]));
                return per(Probe::work, probed);
            }
            return std::nullopt;
        case op::ordered_lookup:
            if constexpr (requires(const Store& held, const Probe& element) { held.lower_bound(element); })
            {
                const auto store = build();
                Probe::work = 0;
                for (std::size_t each = 0; each < probed; ++each)
                    static_cast<void>(store.lower_bound(elements[each % size]));
                return per(Probe::work, probed);
            }
            return std::nullopt;
        case op::erase:
            if constexpr (S::is_queue || urd::has<S, frozen>)
                return std::nullopt;
            else
            {
                auto store = build();
                Probe::work = 0;
                if constexpr (requires(Store& held, const Probe& element) { held.erase(element); })
                    for (std::size_t each = 0; each < erased; ++each)
                        store.erase(elements[each]);
                else if constexpr (requires(Store& held) { held.erase(held.begin()); })
                    for (std::size_t each = 0; each < erased; ++each)
                        store.erase(std::next(store.begin(), static_cast<std::ptrdiff_t>(store.size() / 2)));
                else
                    return std::nullopt;
                return per(Probe::work, erased);
            }
        case op::pop:
            if constexpr (requires(Store& held) { held.pop(); })
            {
                auto store = build();
                Probe::work = 0;
                while (!store.empty())
                    store.pop();
                return per(Probe::work, size);
            }
            return std::nullopt;
        case op::scope:
            if constexpr (requires(Store& held) { held.pop_scope(); })
            {
                auto store = Store();
                store.push_scope();
                for (const auto& each : elements)
                    add<S>(store, each);
                Probe::work = 0;
                store.pop_scope();
                return static_cast<double>(Probe::work);
            }
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

/// The work per row of `what` on a pool of R for S, with rows of `length` elements.
template<typename R, typename S>
std::optional<double> row_work(op what, std::size_t length)
{
    using Store = urd::detail::Store<R, S>;
    constexpr std::size_t count = 32;
    const auto all = rows(count, length);
    auto store = Store();
    if (what == op::insert_row)
    {
        Probe::work = 0;
        for (const auto& each : all)
            static_cast<void>(store.insert(each));
        return static_cast<double>(Probe::work) / count;
    }
    if (what == op::find_row)
    {
        for (const auto& each : all)
            store.insert(each);
        Probe::work = 0;
        for (const auto& each : all)
            static_cast<void>(store.find(each));
        return static_cast<double>(Probe::work) / count;
    }
    return std::nullopt;
}

/// The class a ratio of work across a 64-fold growth shows: O(1) about 1, O(log) about 1.75, O(n) about 64.
growth classify(double small, double large)
{
    if (large < 0.5 && small < 0.5)
        return growth::one;
    const double ratio = large / std::max(small, 0.5);
    if (ratio < 1.3)
        return growth::one;
    if (ratio < 8)
        return growth::log;
    if (ratio < 88)
        return growth::linear;
    return growth::n_log_n;
}

struct verified
{
    growth measured = growth::one;
    growth declared = growth::one;
    std::string request;
};

/// Runs every counted operation of every candidate serving the request Q, once per (representation, operation).
template<typename Q, typename List>
struct verify_all;
template<typename Q, typename... Rs>
struct verify_all<Q, urd::detail::type_list<Rs...>>
{
    using S = typename Q::spec;

    template<typename R>
    static void one(std::map<std::pair<std::string, op>, verified>& into)
    {
        {
            const auto offers = urd::test::offered<R, S>();
            for (std::size_t each = 0; each < offers.size(); ++each)
            {
                const auto what = static_cast<op>(each);
                const auto how = urd::test::cost_of<R>(what);
                const auto key = std::pair(std::string(urd::test::name<R>::id), what);
                if (!offers[each] || !how || how->timed || urd::test::passed_on<R>(what) || into.contains(key))
                    continue;
                std::optional<double> small;
                std::optional<double> large;
                if (what == op::insert_row || what == op::find_row)
                {
                    if constexpr (S::is_pool)
                    {
                        small = row_work<R, S>(what, 16);
                        large = row_work<R, S>(what, 1024);
                    }
                }
                else if constexpr (!S::is_pool)
                {
                    small = work<R, S>(what, 256);
                    large = work<R, S>(what, 16384);
                }
                // One quantity grows: a row's length for the row operations, the changes for a scope, else the elements
                // (with their key range and capacity). A cost in another quantity holds it fixed, so it must not grow.
                const auto varied = what == op::insert_row || what == op::find_row ? urd::scale::row : what == op::scope ? urd::scale::changes : urd::scale::n;
                if (small && large)
                    into[key] = { classify(*small, *large), urd::scale_of(how->of) == varied ? how->grows : growth::one, Q::describe() };
            }
        }
    }
    static void run(std::map<std::pair<std::string, op>, verified>& into) { (one<Rs>(into), ...); }
};

/// The counted costs the candidates serving S declare themselves (not passed on to a part), by name.
template<typename S, typename List>
struct counted;
template<typename S, typename... Rs>
struct counted<S, urd::detail::type_list<Rs...>>
{
    template<typename R>
    static void one(std::set<std::pair<std::string, op>>& into)
    {
        for (std::size_t each = 0; each < static_cast<std::size_t>(op::count); ++each)
        {
            const auto what = static_cast<op>(each);
            const auto how = urd::test::cost_of<R>(what);
            if (how && !how->timed && !urd::test::passed_on<R>(what))
                into.emplace(std::string(urd::test::name<R>::id), what);
        }
    }
    static void run(std::set<std::pair<std::string, op>>& into) { (one<Rs>(into), ...); }
};

template<typename... Ps>
using probe_request = urd::test::request<Ps...>;

/// The requests over Probe whose candidates cover every counted entry of the table.
using probe_requests = urd::test::requests<
    // Sets.
    probe_request<key<Probe>>,
    probe_request<key<Probe>, urd::stable_refs>,
    probe_request<key<Probe>, urd::sorted>,
    probe_request<key<Probe>, urd::sorted, urd::small>,
    probe_request<key<Probe>, urd::sorted, frozen>,
    probe_request<key<Probe>, urd::concurrent>,
    probe_request<key<Probe>, urd::stable_refs, urd::concurrent>,
    probe_request<key<Probe>, urd::positional>,
    probe_request<key<Probe>, urd::positional, urd::monotone, urd::stable_refs>,
    probe_request<key<Probe>, urd::positional, urd::monotone, urd::concurrent>,
    // Maps.
    probe_request<key<Probe>, value<int>>,
    probe_request<key<Probe>, value<int>, urd::stable_refs>,
    probe_request<key<Probe>, value<int>, urd::sorted>,
    probe_request<key<Probe>, value<int>, urd::sorted, urd::small>,
    probe_request<key<Probe>, value<int>, urd::concurrent>,
    probe_request<key<Probe>, value<int>, urd::stable_refs, urd::concurrent>,
    probe_request<key<Probe>, value<int>, urd::multi>,
    probe_request<key<Probe>, value<int>, urd::multi, urd::sorted>,
    probe_request<key<Probe>, value<int>, urd::multi, urd::sorted, urd::small>,
    probe_request<key<Probe>, value<int>, urd::multi, urd::stable_refs>,
    probe_request<key<Probe>, value<int>, urd::scoped>,
    probe_request<key<Probe>, value<int>, urd::scoped, urd::sorted>,
    probe_request<key<Probe>, value<int>, urd::sorted, urd::small, urd::order_by<entry_value>>,
    // Sequences.
    probe_request<value<Probe>>,
    probe_request<value<Probe>, urd::sorted>,
    probe_request<value<Probe>, urd::monotone, urd::stable_refs>,
    probe_request<value<Probe>, urd::monotone, urd::stable_refs, urd::concurrent>,
    // Queues.
    probe_request<value<Probe>, urd::fifo>,
    probe_request<value<Probe>, urd::fifo, urd::retains>,
    probe_request<value<Probe>, urd::lifo>,
    probe_request<value<Probe>, urd::by_priority>,
    probe_request<value<Probe>, urd::by_priority, urd::handles>,
    probe_request<value<Probe>, urd::by_priority, urd::order_by<probe_value>>,
    // Pools of rows.
    probe_request<value<urd::Store<value<Probe>, frozen>>, urd::handles, urd::unique>,
    probe_request<value<urd::Store<value<Probe>, frozen>>, urd::handles, urd::unique, urd::structural_sharing>,
    probe_request<value<urd::Store<value<Probe>, frozen>>, urd::handles, urd::unique, urd::concurrent>>;

template<typename... Qs>
void verify_requests(urd::test::requests<Qs...> /*list*/, std::map<std::pair<std::string, op>, verified>& into)
{ (verify_all<Qs, urd::test::serving_containers<typename Qs::spec>>::run(into), ...); }

}  // namespace

TEST(Complexity, CountedCostsGrowAsDeclared)
{
#if !defined(NDEBUG)
    GTEST_SKIP() << "Release only: the linear cases do millions of operations";
#endif
    auto results = std::map<std::pair<std::string, op>, verified>();
    verify_requests(probe_requests {}, results);
    for (const auto& [key, result] : results)
        EXPECT_EQ(static_cast<int>(result.measured), static_cast<int>(result.declared))
            << key.first << " " << urd::test::op_names[static_cast<std::size_t>(key.second)] << " (" << result.request << "): measured "
            << static_cast<int>(result.measured) << ", declared " << static_cast<int>(result.declared) << " (0 constant, 1 logarithmic, 2 linear, 3 n log n)";
    // Every counted cost a candidate of the grid declares is verified (gtl's tables are no candidates without it).
    auto declared = std::set<std::pair<std::string, op>>();
    urd::test::for_each_group(
        [&](const char* /*title*/, auto list)
        {
            urd::test::for_each_request(list,
                                        [&]<typename Q>(std::type_identity<Q>)
                                        {
                                            using S = typename Q::spec;
                                            using List = urd::test::serving_containers<S>;
                                            counted<S, List>::run(declared);
                                        });
        });
    for (const auto& [representation, what] : declared)
        EXPECT_TRUE(results.contains({ representation, what }))
            << representation << " " << urd::test::op_names[static_cast<std::size_t>(what)]
            << ": counted, but no request over Probe reaches it (probe_requests, tests/complexity_verify.cpp), or mark it timed";
}
