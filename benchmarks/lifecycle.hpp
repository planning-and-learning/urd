#pragma once

// One container's lifecycle on one Store, each operation it offers timed in turn: insert (or build), probe, iterate,
// erase, pop, clear; for a pool, its rows. Through the Store over the container (urd::detail::Store<R, S>), so every
// container is measured through the same members. Included by benchmarks/experiment.cpp, once per container.

#include "../tests/grid.hpp"
#include "cells.hpp"

#include <algorithm>
#include <array>
#include <benchmark/benchmark.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::bench
{

using urd::test::op;

template<typename T>
inline constexpr bool is_index = false;
template<typename T>
inline constexpr bool is_index<urd::index<T>> = true;

template<typename T>
inline constexpr bool is_pair = false;
template<typename A, typename B>
inline constexpr bool is_pair<std::pair<A, B>> = true;

template<typename T>
T make(std::size_t at);

/// A Store of three elements, distinct for each `at`.
template<typename T>
T make_store(std::size_t at)
{
    if constexpr (requires { typename T::handle_type; })
        return T();  // a pool as a value starts empty: it is filled by handle, not from a range
    else
    {
        using E = std::remove_cv_t<typename T::value_type>;
        auto elements = std::vector<E>();
        for (std::size_t each = 0; each < 3; ++each)
            elements.push_back(make<E>(3 * at + each));
        if constexpr (std::constructible_from<T, decltype(elements.begin()), decltype(elements.end())>)
            return T(elements.begin(), elements.end());
        else
        {
            auto result = T();
            for (auto& each : elements)
            {
                if constexpr (requires { result.push_back(each); })
                    result.push_back(std::move(each));
                else
                    result.insert(std::move(each));
            }
            return result;
        }
    }
}

/// The `at`-th element of type T: distinct for distinct `at`, and an index `at` for index types (dense keys).
template<typename T>
T make(std::size_t at)
{
    if constexpr (std::is_arithmetic_v<T>)
        return static_cast<T>(at);
    else if constexpr (std::is_same_v<T, std::string>)
        return "an element of some length " + std::to_string(at);
    else if constexpr (std::is_same_v<T, urd::test::Id>)
        return T { static_cast<std::uint32_t>(at) };
    else if constexpr (is_index<T>)
        return T(static_cast<typename T::value_type>(at));
    else if constexpr (std::is_same_v<T, urd::test::Text>)
        return T { urd::flat_string("text " + std::to_string(at)) };
    else if constexpr (is_pair<T>)
        return T(make<std::remove_cv_t<typename T::first_type>>(at), make<typename T::second_type>(at));
    else if constexpr (urd::is_store<T>)
        return make_store<T>(at);
    else if constexpr (requires(T row) { row.push_back(make<typename T::value_type>(0)); })
    {
        auto row = T();
        for (std::size_t each = 0; each < 3; ++each)
            row.push_back(make<typename T::value_type>(3 * at + each));
        return row;
    }
    else
        static_assert(sizeof(T) == 0, "make: an element type the benchmarks cannot build");
}

/// `count` distinct indices in random order, fixed by the seed.
inline std::vector<std::size_t> shuffled(std::size_t count, unsigned seed = 42)
{
    auto result = std::vector<std::size_t>(count);
    for (std::size_t each = 0; each < count; ++each)
        result[each] = each;
    std::ranges::shuffle(result, std::mt19937(seed));
    return result;
}

/// Keeps a result from being optimized away (DoNotOptimize takes no const references).
template<typename T>
void use(const T& value)
{
    const auto* address = &value;
    benchmark::DoNotOptimize(address);
}

/// The time of each operation over a benchmark's lifecycles.
struct timing
{
    std::array<double, static_cast<std::size_t>(op::count)> nanoseconds {};
    std::array<double, static_cast<std::size_t>(op::count)> operations {};
    std::chrono::steady_clock::time_point began;

    void start() { began = std::chrono::steady_clock::now(); }
    void stop(op what, std::size_t count)
    {
        nanoseconds[static_cast<std::size_t>(what)] += std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - began).count();
        operations[static_cast<std::size_t>(what)] += static_cast<double>(count);
    }
};

/// What one iteration of a probing workload looks at.
constexpr std::size_t probes = 1000;
/// A prime: the probes spread over the elements instead of walking them in order.
constexpr std::size_t stride = 7919;
/// How many lifecycles a benchmark runs: enough that a small Store's take measurable time, at most most_rounds.
constexpr std::size_t lifecycle_elements = 100000;
constexpr std::size_t most_rounds = 1000;

/// What a Store of these key, value and element types is built from: entries for a map, else its elements.
template<bool Map, typename K, typename V, typename E>
struct element_of
{
    using type = E;
};

template<typename K, typename V, typename E>
struct element_of<true, K, V, E>
{
    using type = std::pair<K, V>;
};

template<typename R, typename S>
struct workload
{
    using Store = urd::detail::Store<R, S>;
    using K = typename S::key_type;
    using V = typename S::value_type;
    using E = std::conditional_t<S::is_set || S::is_partition || S::is_map, K, V>;  // keyed: by key
    /// What a Store is built from: entries for a map, else its elements.
    using Element = typename element_of<S::is_map, K, V, E>::type;

    static void add(Store& store, std::size_t at)
    {
        if constexpr (S::is_map)
            store.emplace(make<K>(at), make<V>(at));
        else if constexpr (S::is_queue)
            store.push(make<V>(at));
        else if constexpr (requires { store.push_back(make<E>(at)); })
            store.push_back(make<E>(at));
        else if constexpr (urd::is_store<E> && requires { store.push_back(make<std::vector<std::remove_cv_t<typename E::value_type>>>(at)); })
            store.push_back(make<std::vector<std::remove_cv_t<typename E::value_type>>>(at));
        else
            store.insert(make<E>(at));
    }

    /// An empty Store, or (nonempty) one holding the first element of `order`; how many of `order` it holds.
    static std::size_t start(std::unique_ptr<Store>& into, const std::vector<std::size_t>& order)
    {
        if constexpr (std::default_initializable<Store>)
        {
            into = std::make_unique<Store>();
            return 0;
        }
        else
        {
            auto first = std::vector<E> { make<E>(order.front()) };
            into = std::make_unique<Store>(first.begin(), first.end());
            return 1;
        }
    }

    /// The Store of the elements in `order`, where inserting is not measured, built in place (some Stores do not move):
    /// from all at once where it takes a range (a frozen one), else one by one.
    static void build(std::unique_ptr<Store>& into, const std::vector<std::size_t>& order)
    {
        if constexpr (urd::has<S, urd::fixed_shape> && std::constructible_from<Store, const std::vector<std::vector<std::size_t>>&>)
            into = std::make_unique<Store>(std::vector<std::vector<std::size_t>>(order.size(), std::vector<std::size_t>(3, 3)));  // rows of three rows of three
        else if constexpr (urd::has<S, urd::fixed_shape>)
            into = std::make_unique<Store>(urd::row_lengths(std::vector<std::size_t>(order.size(), 3)));
        else if constexpr (std::constructible_from<Store, typename std::vector<Element>::iterator, typename std::vector<Element>::iterator>)
        {
            auto elements = std::vector<Element>();
            for (const auto at : order)
                if constexpr (S::is_map)
                    elements.emplace_back(make<K>(at), make<V>(at));
                else
                    elements.push_back(make<E>(at));
            into = std::make_unique<Store>(elements.begin(), elements.end());
        }
        else
        {
            for (std::size_t each = start(into, order); each < order.size(); ++each)
                add(*into, order[each]);
        }
    }

    /// The probe a lookup at step `each` makes: a held element, or (odd steps) one past them.
    static E probe(std::size_t size, std::size_t each) { return make<E>(each % 2 == 0 ? (each * stride) % size : size + each); }

    static constexpr auto offers = urd::test::offered<R, S>();

    /// One lifecycle on one Store: insert (or build), probe, iterate, erase, pop, clear, each timed in turn into
    /// `totals` (ns) and `counts` (operations).
    static void lifecycle(const urd::test::cell& at, timing& into)
    {
        const auto size = at.size;
        const auto order = shuffled(size);
        const auto has = [&](op what) { return at.operations[static_cast<std::size_t>(what)]; };
        auto store = std::unique_ptr<Store>();
        if constexpr (offers[static_cast<std::size_t>(op::insert)] || offers[static_cast<std::size_t>(op::push)])
        {
            if (has(op::insert) || has(op::push))
            {
                into.start();
                for (std::size_t each = start(store, order); each < order.size(); ++each)
                    add(*store, order[each]);
                into.stop(has(op::push) ? op::push : op::insert, size);
            }
        }
        if (!store)
            build(store, order);
        auto& held = *store;
        if constexpr (requires(const Store& kept) { kept.contains(probe(size, 0)); })
            if (has(op::lookup))
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                    use(std::as_const(held).contains(probe(size, each)));
                into.stop(op::lookup, probes);
            }
        if constexpr (requires(const Store& kept) { kept.lower_bound(probe(size, 0)); })
            if (has(op::ordered_lookup))
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                    use(std::as_const(held).lower_bound(probe(size, each)));
                into.stop(op::ordered_lookup, probes);
            }
        if (has(op::access))
        {
            if constexpr (requires(const Store& kept) { kept[std::size_t {}]; })
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                    use(std::as_const(held)[(each * stride) % size]);
                into.stop(op::access, probes);
            }
            else if constexpr (requires(const Store& kept) { kept[typename Store::index_type(0)]; })
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                    use(std::as_const(held)[typename Store::index_type((each * stride) % size)]);
                into.stop(op::access, probes);
            }
        }
        if constexpr (requires(const Store& kept) { kept.begin() != kept.end(); })
            if (has(op::iterate))
            {
                into.start();
                for (auto each = std::as_const(held).begin(); each != std::as_const(held).end(); ++each)
                    use(*each);
                into.stop(op::iterate, size);
            }
        if constexpr (requires(const Store& kept) { kept.top(); } || requires(const Store& kept) { kept.front(); })
            if (has(op::top))
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                {
                    if constexpr (requires { std::as_const(held).top(); })
                        use(std::as_const(held).top());
                    else
                        use(std::as_const(held).front());
                }
                into.stop(op::top, probes);
            }
        if constexpr (!S::is_queue)
            if (has(op::erase))
            {
                const std::size_t erased = std::min<std::size_t>(size / 2, probes);
                if constexpr (requires { held.erase(make<E>(0)); })
                {
                    into.start();
                    for (std::size_t each = 0; each < erased; ++each)
                        held.erase(make<E>(order[each]));
                    into.stop(op::erase, erased);
                }
                else if constexpr (requires { held.erase(held.begin()); })
                {
                    into.start();
                    for (std::size_t each = 0; each < erased; ++each)
                        held.erase(std::next(held.begin(), static_cast<std::ptrdiff_t>(held.size() / 2)));
                    into.stop(op::erase, erased);
                }
            }
        if constexpr (requires { held.pop(); })
            if (has(op::pop))
            {
                const auto popped = held.size();
                into.start();
                while (!held.empty())
                    held.pop();
                into.stop(op::pop, popped);
            }
        if constexpr (requires { held.clear(); })
            if (has(op::clear))
            {
                const auto cleared = std::max<std::size_t>(held.size(), 1);
                into.start();
                held.clear();
                into.stop(op::clear, cleared);
            }
        store.reset();
    }
};

/// A pool's row operations, with rows (or records) of `length` elements.
template<typename R, typename S>
struct row_workload
{
    using Store = urd::detail::Store<R, S>;
    static constexpr std::size_t rows = 256;

    static auto row(std::size_t at, std::size_t length)
    {
        using Row = typename S::element_type;
        if constexpr (std::is_same_v<Row, urd::test::Text>)
            return Row { urd::flat_string(std::to_string(at) + std::string(length, 'x')) };
        else if constexpr (urd::is_store<Row>)
        {
            using E = std::remove_cv_t<typename Row::value_type>;
            auto result = std::vector<E>();
            for (std::size_t each = 0; each < length; ++each)
                result.push_back(make<E>(at * length + each));
            return result;
        }
        else
            return make<Row>(at);
    }

    template<typename Row>
    static auto insert(Store& store, const Row& row)
    {
        if constexpr (requires { store.insert(row).first; })
            return store.insert(row).first;
        else
            return store.emplace(row);
    }

    /// Inserting the rows, finding them, reading them by handle, each timed in turn.
    static void lifecycle(const urd::test::cell& at, timing& into)
    {
        const auto has = [&](op what) { return at.operations[static_cast<std::size_t>(what)]; };
        auto all = std::vector<decltype(row(0, 0))>();
        for (std::size_t each = 0; each < rows; ++each)
            all.push_back(row(each, at.size));
        auto store = std::make_unique<Store>();
        auto handles = std::vector<decltype(insert(*store, all.front()))>();
        handles.reserve(rows);
        into.start();
        for (const auto& each : all)
            handles.push_back(insert(*store, each));
        into.stop(op::insert_row, rows);
        const auto& held = *store;
        if constexpr (requires { held.find(all.front()); })
            if (has(op::find_row))
            {
                into.start();
                for (const auto& each : all)
                    use(held.find(each));
                into.stop(op::find_row, rows);
            }
        if (has(op::access))
        {
            into.start();
            for (const auto& each : handles)
            {
                if constexpr (requires { held.get(each); })
                    use(held.get(each));
                else
                    use(held[each]);
            }
            into.stop(op::access, rows);
        }
    }
};

/// Registers the cells of container R's experiment for the request spec S, one per size: each runs the lifecycle (enough
/// times for small sizes to take measurable time) and reports the time per operation of each, as a counter named for it.
template<typename R, typename S>
void register_experiment(std::size_t ordinal)
{
    const auto id = urd::test::experiment_of<S>(urd::test::name<R>::id) + (ordinal == 1 ? "" : "#" + std::to_string(ordinal));
    for (const auto size : urd::test::sizes_of<S>())
    {
        const auto at = urd::test::cell { id + "/" + std::to_string(size), urd::test::offered<R, S>(), size };
        benchmark::RegisterBenchmark(at.id,
                                     [at](benchmark::State& state)
                                     {
                                         auto into = timing {};
                                         const std::size_t rounds =
                                             std::clamp<std::size_t>(lifecycle_elements / std::max<std::size_t>(at.size, 1), 1, most_rounds);
                                         for (auto _ : state)
                                             for (std::size_t round = 0; round < rounds; ++round)
                                             {
                                                 if constexpr (S::is_pool)
                                                     row_workload<R, S>::lifecycle(at, into);
                                                 else
                                                     workload<R, S>::lifecycle(at, into);
                                             }
                                         for (std::size_t what = 0; what < into.operations.size(); ++what)
                                             if (into.operations[what] > 0)
                                                 state.counters[std::string(urd::test::op_names[what])] = into.nanoseconds[what] / into.operations[what];
                                     })
            ->Iterations(1)
            ->Unit(benchmark::kMillisecond);
    }
}

}  // namespace urd::bench
