#pragma once

// One container's lifecycle on one Store, each operation it offers timed in turn: insert (or build), probe, iterate,
// erase, pop, clear; for a pool, its rows. Through the Store over the container (urd::test::store_over<S, R>), so every
// container is measured through the same members. Included by benchmarks/experiment.cpp, once per container.

#include "grid.hpp"
#include "cells.hpp"

#include <algorithm>
#include <array>
#include <benchmark/benchmark.h>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::bench
{

using urd::test::op;
using urd::test::op_count;
using urd::test::op_v;

template<typename T>
inline constexpr bool is_pair = false;
template<typename A, typename B>
inline constexpr bool is_pair<std::pair<A, B>> = true;

template<typename T>
T make(std::size_t at);

/// A Store of three elements, distinct for each `at`: the base-128 digits of `at`, one in each third of [0, 384), so
/// a Store of dense keys stays small however many there are (at < 2^21).
template<typename T>
T make_store(std::size_t at)
{
    if constexpr (requires { typename T::handle_type; })
        return urd::test::make_empty<T>(at);  // a pool as a value starts empty: it is filled by handle, not from a range
    else
    {
        using E = std::remove_cv_t<typename T::value_type>;
        constexpr std::size_t bits = 7, digits = std::size_t { 1 } << bits;
        auto elements = std::vector<E>();
        for (std::size_t each = 0; each < 3; ++each)
            elements.push_back(make<E>((each * digits) + ((at >> (bits * each)) & (digits - 1))));
        if constexpr (std::constructible_from<T, decltype(elements.begin()), decltype(elements.end())>)
            return T(elements.begin(), elements.end());
        else
        {
            auto result = urd::test::make_empty<T>(at);
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
    else if constexpr (urd::is_index_type<T>)
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

/// The heap bytes asked for and still held, counted by benchmark_memory (benchmarks/support/experiments_main.cpp.in); none
/// where it does not count them.
std::optional<std::size_t> live_bytes();

/// The memory a Store of a cell holds, once filled as its lifecycle fills it: counted (the heap it took, past its own
/// object), as it tells (urd::memory_usage), over how many elements, of what size, and whether they own nothing more.
struct held_memory
{
    std::size_t counted = 0;
    std::size_t told = 0;
    std::size_t elements = 0;
    std::size_t element_size = 0;
    bool plain = false;
};

/// Whether a T owns no memory beyond itself: what memory per element is judged on (benchmarks/tools/record.py).
template<typename T>
inline constexpr bool owns_nothing = std::is_trivially_copyable_v<T>;
template<typename A, typename B>
inline constexpr bool owns_nothing<std::pair<A, B>> = owns_nothing<std::remove_cv_t<A>> && owns_nothing<B>;

/// The time of each operation over a benchmark's lifecycles.
struct timing
{
    std::array<double, op_count> nanoseconds {};
    std::array<double, op_count> operations {};
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
    using Store = urd::test::store_over<S, R>;
    using K = typename urd::test::key_of<S>::type;
    using V = typename urd::test::value_of<S>::type;
    using E = typename S::lookup_type;  // what the kind's data finds by: a key, else a value
    /// What a Store is built from: entries for a map, else its elements.
    /// Whether its data maps keys to values: built from entries, added by key and value.
    static constexpr bool maps = requires { typename S::mapped_type; };
    using Element = typename element_of<maps, K, V, E>::type;

    static void add(Store& store, std::size_t at)
    {
        if constexpr (maps)
            store.emplace(make<K>(at), make<V>(at));
        else if constexpr (requires { store.push(make<V>(at)); })
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
        if constexpr (std::constructible_from<Store, urd::layer>)
        {
            into = std::make_unique<Store>(urd::layer { 0 });
            return 0;
        }
        else if constexpr (std::default_initializable<Store>)
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
                if constexpr (maps)
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
        if constexpr (offers[static_cast<std::size_t>(op_v<urd::operation::insert>)] || offers[static_cast<std::size_t>(op_v<urd::operation::push>)])
        {
            if (has(op_v<urd::operation::insert>) || has(op_v<urd::operation::push>))
            {
                into.start();
                for (std::size_t each = start(store, order); each < order.size(); ++each)
                    add(*store, order[each]);
                into.stop(has(op_v<urd::operation::push>) ? op_v<urd::operation::push> : op_v<urd::operation::insert>, size);
            }
        }
        if (!store)
            build(store, order);
        auto& held = *store;
        if constexpr (requires(const Store& kept) { kept.contains(probe(size, 0)); })
            if (has(op_v<urd::operation::lookup>))
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                    use(std::as_const(held).contains(probe(size, each)));
                into.stop(op_v<urd::operation::lookup>, probes);
            }
        if constexpr (requires(const Store& kept) { kept.lower_bound(probe(size, 0)); })
            if (has(op_v<urd::operation::ordered_lookup>))
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                    use(std::as_const(held).lower_bound(probe(size, each)));
                into.stop(op_v<urd::operation::ordered_lookup>, probes);
            }
        if (has(op_v<urd::operation::access>))
        {
            if constexpr (requires(const Store& kept) { kept[std::size_t {}]; })
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                    use(std::as_const(held)[(each * stride) % size]);
                into.stop(op_v<urd::operation::access>, probes);
            }
            else if constexpr (urd::has<Store, urd::flat> && requires(const Store& kept) { kept[typename Store::handle_type(0)]; })
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                    use(std::as_const(held)[typename Store::handle_type((each * stride) % size)]);
                into.stop(op_v<urd::operation::access>, probes);
            }
        }
        if constexpr (requires(const Store& kept) { kept.begin() != kept.end(); })
            if (has(op_v<urd::operation::iterate>))
            {
                into.start();
                for (auto each = std::as_const(held).begin(); each != std::as_const(held).end(); ++each)
                    use(*each);
                into.stop(op_v<urd::operation::iterate>, size);
            }
        if constexpr (requires(const Store& kept) { kept.top(); } || requires(const Store& kept) { kept.front(); })
            if (has(op_v<urd::operation::top>))
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                {
                    if constexpr (requires { std::as_const(held).top(); })
                        use(std::as_const(held).top());
                    else
                        use(std::as_const(held).front());
                }
                into.stop(op_v<urd::operation::top>, probes);
            }
        if constexpr (offers[static_cast<std::size_t>(op_v<urd::operation::erase>)])
            if (has(op_v<urd::operation::erase>))
            {
                const std::size_t erased = std::min<std::size_t>(size / 2, probes);
                if constexpr (requires { held.erase(make<E>(0)); })
                {
                    into.start();
                    for (std::size_t each = 0; each < erased; ++each)
                        held.erase(make<E>(order[each]));
                    into.stop(op_v<urd::operation::erase>, erased);
                }
                else if constexpr (requires { held.erase(held.begin()); })
                {
                    into.start();
                    for (std::size_t each = 0; each < erased; ++each)
                        held.erase(std::next(held.begin(), static_cast<std::ptrdiff_t>(held.size() / 2)));
                    into.stop(op_v<urd::operation::erase>, erased);
                }
            }
        if constexpr (requires { held.pop(); })
            if (has(op_v<urd::operation::pop>))
            {
                const auto popped = held.size();
                into.start();
                while (!held.empty())
                    held.pop();
                into.stop(op_v<urd::operation::pop>, popped);
            }
        if constexpr (requires { held.clear(); })
            if (has(op_v<urd::operation::clear>))
            {
                // Per element of the lifecycle's size, though erase or pop may have emptied it: a clear in O(U)
                // or O(capacity) walks what the size made, not what is left.
                into.start();
                held.clear();
                into.stop(op_v<urd::operation::clear>, size);
            }
        store.reset();
    }

    /// The memory of the Store a cell's lifecycle fills: inserted one by one where the cell times inserting, else
    /// built.
    static held_memory memory(const urd::test::cell& at)
    {
        const auto order = shuffled(at.size);
        [[maybe_unused]] const auto has = [&](op what) { return at.operations[static_cast<std::size_t>(what)]; };
        const auto before = live_bytes().value_or(0);
        auto store = std::unique_ptr<Store>();
        if constexpr (offers[static_cast<std::size_t>(op_v<urd::operation::insert>)] || offers[static_cast<std::size_t>(op_v<urd::operation::push>)])
            if (has(op_v<urd::operation::insert>) || has(op_v<urd::operation::push>))
                for (std::size_t each = start(store, order); each < order.size(); ++each)
                    add(*store, order[each]);
        if (!store)
            build(store, order);
        return { live_bytes().value_or(0) - before - sizeof(Store), urd::memory_usage(*store), at.size, sizeof(Element), owns_nothing<Element> };
    }
};

/// A pool's row operations, with rows (or records) of `length` elements.
template<typename R, typename S>
struct row_workload
{
    using Store = urd::test::store_over<S, R>;
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

    /// The pool, built with the bit width its values (below `bound`) need where it is built with one (urd::bit): built
    /// without, it keeps every element in a whole word and measures nothing of packing.
    static std::unique_ptr<Store> built(std::size_t bound)
    {
        if constexpr (std::constructible_from<Store, urd::bit_width>)
            return std::make_unique<Store>(urd::bit_width::for_bound(bound));
        else if constexpr (std::constructible_from<Store, urd::layer, urd::bit_width>)
            return std::make_unique<Store>(urd::layer { 0 }, urd::bit_width::for_bound(bound));
        else
            return std::make_unique<Store>();
    }

    /// Inserting the rows, finding them, reading them by handle, each timed in turn.
    static void lifecycle(const urd::test::cell& at, timing& into)
    {
        const auto has = [&](op what) { return at.operations[static_cast<std::size_t>(what)]; };
        auto all = std::vector<decltype(row(0, 0))>();
        for (std::size_t each = 0; each < rows; ++each)
            all.push_back(row(each, at.size));
        auto store = built(rows * at.size);
        auto handles = std::vector<decltype(insert(*store, all.front()))>();
        handles.reserve(rows);
        into.start();
        for (const auto& each : all)
            handles.push_back(insert(*store, each));
        into.stop(op_v<urd::operation::insert_row>, rows);
        const auto& held = *store;
        if constexpr (requires { held.find(all.front()); })
            if (has(op_v<urd::operation::find_row>))
            {
                into.start();
                for (const auto& each : all)
                    use(held.find(each));
                into.stop(op_v<urd::operation::find_row>, rows);
            }
        if (has(op_v<urd::operation::access>))
        {
            into.start();
            for (const auto& each : handles)
            {
                if constexpr (requires { held.get(each); })
                    use(held.get(each));
                else
                    use(held[each]);
            }
            into.stop(op_v<urd::operation::access>, rows);
        }
    }

    /// The memory of the Store a cell's lifecycle fills: its rows, inserted one by one.
    static held_memory memory(const urd::test::cell& at)
    {
        using Row = typename S::element_type;
        using Element = decltype([]
        {
            if constexpr (urd::is_store<Row>)
                return std::remove_cv_t<typename Row::value_type> {};
            else
                return char {};
        }());
        auto all = std::vector<decltype(row(0, 0))>();
        for (std::size_t each = 0; each < rows; ++each)
            all.push_back(row(each, at.size));
        const auto before = live_bytes().value_or(0);
        auto store = built(rows * at.size);
        for (const auto& each : all)
            use(insert(*store, each));
        return { live_bytes().value_or(0) - before - sizeof(Store), urd::memory_usage(*store), rows * at.size, sizeof(Element), owns_nothing<Element> };
    }
};

/// A graph's lifecycle: `size` vertices with three edges each, to vertices a stride on (in other parts where it is
/// partitioned); added one by one where it grows, else built at once; then every vertex's neighbours read, adjacency
/// probed, edges read by number, edges removed, and the graph cleared, each where it offers it.
template<typename R, typename S>
struct graph_workload
{
    using Store = urd::test::store_over<S, R>;
    using Vertex = typename S::vertex_type;
    using Value = typename S::edge_value_type;
    using Input = typename S::edge_input_type;
    static constexpr auto offers = urd::test::offered<R, S>();
    static constexpr std::size_t parts = 4;
    static constexpr std::size_t degree = 3;

    static Vertex vertex(std::size_t at) { return urd::index_of<Vertex>::from_index(at); }
    /// The target of the k-th edge of u: in the part after u's (cycling) where partitioned, else anywhere.
    static std::size_t target(std::size_t size, std::size_t u, std::size_t k)
    {
        if constexpr (urd::has<S, urd::partitioned>)
        {
            const auto part = size / parts;
            return ((((u / part) + 1 + (k % (parts - 1))) % parts) * part) + (((u * stride) + k) % part);
        }
        else
            return ((u * stride) + k + 1) % size;
    }
    static Input input(std::size_t u, std::size_t v)
    {
        if constexpr (std::is_void_v<Value>)
            return Input(vertex(u), vertex(v));
        else
            return Input(vertex(u), vertex(v), make<Value>(u));
    }

    /// The graph of a cell's size: built at once where it does not grow, else added to vertex by vertex, edge by edge
    /// (timed as insert into `into`, where given).
    static std::unique_ptr<Store> filled(std::size_t size, timing* into)
    {
        const auto part_sizes = std::vector<std::size_t>(urd::has<S, urd::partitioned> ? parts : 0, size / parts);
        if constexpr (offers[static_cast<std::size_t>(op_v<urd::operation::insert>)])
        {
            auto store = std::unique_ptr<Store>();
            if constexpr (urd::has<S, urd::constant_adjacency>)
                store = std::make_unique<Store>(urd::graph_data<Input> { size, {}, part_sizes });  // rows of bits: its vertices fixed
            else
            {
                store = std::make_unique<Store>();
                for (std::size_t u = 0; u < size; ++u)
                    store->add_vertex();
            }
            if (into != nullptr)
                into->start();
            for (std::size_t u = 0; u < size; ++u)
                for (std::size_t k = 0; k < degree; ++k)
                {
                    if constexpr (std::is_void_v<Value>)
                        use(store->add_edge(vertex(u), vertex(target(size, u, k))));
                    else
                        use(store->add_edge(vertex(u), vertex(target(size, u, k)), make<Value>(u)));
                }
            if (into != nullptr)
                into->stop(op_v<urd::operation::insert>, size * degree);
            return store;
        }
        else
        {
            auto edges = std::vector<Input>();
            for (std::size_t u = 0; u < size; ++u)
                for (std::size_t k = 0; k < degree; ++k)
                    edges.push_back(input(u, target(size, u, k)));
            return std::make_unique<Store>(urd::graph_data<Input> { size, edges, part_sizes });
        }
    }

    static void lifecycle(const urd::test::cell& at, timing& into)
    {
        const auto size = at.size;
        const auto has = [&](op what) { return at.operations[static_cast<std::size_t>(what)]; };
        auto store = filled(size, &into);
        auto& held = *store;
        if (has(op_v<urd::operation::iterate>))
        {
            std::size_t read = 0;
            into.start();
            for (std::size_t u = 0; u < size; ++u)
                for (const auto w : std::as_const(held).neighbours(vertex(u)))
                {
                    use(w);
                    ++read;
                }
            into.stop(op_v<urd::operation::iterate>, read);
        }
        if (has(op_v<urd::operation::adjacency>))
        {
            into.start();
            for (std::size_t each = 0; each < probes; ++each)
            {
                const auto u = (each * stride) % size;
                use(std::as_const(held).adjacent(vertex(u), vertex(target(size, u, each % (degree + 1)))));
            }
            into.stop(op_v<urd::operation::adjacency>, probes);
        }
        if constexpr (requires { std::as_const(held).source(std::size_t {}); })
        {
            const auto edges = std::as_const(held).num_edges();
            if (has(op_v<urd::operation::access>))
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                {
                    if constexpr (std::is_void_v<Value>)
                        use(std::as_const(held).target((each * stride) % edges));
                    else
                        use(std::as_const(held)[(each * stride) % edges]);
                }
                into.stop(op_v<urd::operation::access>, probes);
            }
            if (has(op_v<urd::operation::endpoints>))
            {
                into.start();
                for (std::size_t each = 0; each < probes; ++each)
                    use(std::as_const(held).source((each * stride) % edges));
                into.stop(op_v<urd::operation::endpoints>, probes);
            }
        }
        if constexpr (requires { held.remove_edge(std::size_t {}); })
            if (has(op_v<urd::operation::erase>))
            {
                const auto removed = std::min(probes, size * degree);
                into.start();
                for (std::size_t e = 0; e < removed; ++e)
                    held.remove_edge(e);
                into.stop(op_v<urd::operation::erase>, removed);
            }
        if constexpr (requires { held.clear(); })
            if (has(op_v<urd::operation::clear>))
            {
                into.start();
                held.clear();
                into.stop(op_v<urd::operation::clear>, size);
            }
        store.reset();
    }

    /// The memory of the graph a cell's lifecycle fills, per vertex.
    static held_memory memory(const urd::test::cell& at)
    {
        const auto before = live_bytes().value_or(0);
        const auto store = filled(at.size, nullptr);
        return { live_bytes().value_or(0) - before - sizeof(Store), urd::memory_usage(*store), at.size, sizeof(Vertex), owns_nothing<Vertex> };
    }
};

/// The workload of a cell: a pool's rows, a graph's vertices and edges, or elements.
template<typename R, typename S>
using workload_of = std::conditional_t<urd::test::by_handle<S>,
                                       row_workload<R, S>,
                                       std::conditional_t<std::same_as<typename S::kind, urd::graph_kind>, graph_workload<R, S>, workload<R, S>>>;

/// Registers the cells of container R's experiment for the request spec S, one per size. Where the heap is counted
/// (benchmark_memory), each reports, once, the memory of the Store it fills (held_memory), as counters named memory_*;
/// else each runs the lifecycle (enough times for small sizes to take measurable time) and reports the time per
/// operation of each, as a counter named for it.
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
                                         if (live_bytes())
                                         {
                                             held_memory held {};
                                             for (auto _ : state)
                                             {
                                                 held = workload_of<R, S>::memory(at);
                                             }
                                             state.counters["memory_counted"] = static_cast<double>(held.counted);
                                             state.counters["memory_told"] = static_cast<double>(held.told);
                                             state.counters["memory_elements"] = static_cast<double>(held.elements);
                                             state.counters["memory_element_size"] = static_cast<double>(held.element_size);
                                             state.counters["memory_plain"] = held.plain ? 1 : 0;
                                             return;
                                         }
                                         auto into = timing {};
                                         const std::size_t rounds =
                                             std::clamp<std::size_t>(lifecycle_elements / std::max<std::size_t>(at.size, 1), 1, most_rounds);
                                         for (auto _ : state)
                                             for (std::size_t round = 0; round < rounds; ++round)
                                             {
                                                 workload_of<R, S>::lifecycle(at, into);
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
