// Sets and maps under four workloads, for every container a set or map request can resolve to, with urd's
// hash, equality and order as the registry instantiates them. The numbers
// back the preference orders in include/urd/registry.hpp; docs/BENCHMARKS.md records them.

#include "urd/repr/dense.hpp"
#include "urd/repr/grouped.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/queues.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/semantics/less.hpp"

#include <array>
#include <benchmark/benchmark.h>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

/// A 32-byte key, as a small tuple of ids; it names its identity, from which urd hashes and orders it.
struct Key32
{
    std::array<std::uint64_t, 4> parts;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Key32::parts> {}; }
};

template<typename T>
T make_key(std::uint64_t value)
{
    if constexpr (std::same_as<T, Key32>)
        return Key32 { { value, value + 1, value + 2, value + 3 } };
    else
        return static_cast<T>(value);
}

/// `count` distinct keys in random order; `misses` keys absent from them.
template<typename T>
std::vector<T> keys(std::size_t count, bool misses = false)
{
    auto result = std::vector<T>();
    for (std::size_t pos = 0; pos < count; ++pos)
        result.push_back(make_key<T>(2 * pos + (misses ? 1 : 0)));
    constexpr unsigned seed = 42;  // the same keys in every run
    std::shuffle(result.begin(), result.end(), std::mt19937(seed));
    return result;
}

template<typename C>
constexpr bool is_map = requires { typename C::mapped_type; };

template<typename C, typename T>
void add(C& container, const T& key)
{
    if constexpr (is_map<C>)
        container.emplace(key, 1);
    else
        container.insert(key);
}

template<typename C>
using key_t = typename C::key_type;

template<typename C>
void insert(benchmark::State& state)
{
    const auto input = keys<key_t<C>>(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state)
    {
        auto container = C();
        for (const auto& key : input)
            add(container, key);
        benchmark::DoNotOptimize(container);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

template<typename C>
void lookup(benchmark::State& state)
{
    const auto n = static_cast<std::size_t>(state.range(0));
    auto container = C();
    for (const auto& key : keys<key_t<C>>(n))
        add(container, key);
    auto probes = keys<key_t<C>>(n / 2);
    const auto misses = keys<key_t<C>>(n / 2, true);
    probes.insert(probes.end(), misses.begin(), misses.end());
    for (auto _ : state)
    {
        std::size_t found = 0;
        for (const auto& probe : probes)
            found += container.contains(probe) ? 1U : 0U;
        benchmark::DoNotOptimize(found);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(probes.size()));
}

template<typename C>
void clear_refill(benchmark::State& state)
{
    const auto input = keys<key_t<C>>(static_cast<std::size_t>(state.range(0)));
    auto container = C();
    for (const auto& key : input)
        add(container, key);
    for (auto _ : state)
    {
        container.clear();
        for (const auto& key : input)
            add(container, key);
        benchmark::DoNotOptimize(container);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

template<typename C>
void iterate(benchmark::State& state)
{
    auto container = C();
    for (const auto& key : keys<key_t<C>>(static_cast<std::size_t>(state.range(0))))
        add(container, key);
    for (auto _ : state)
    {
        std::size_t touched = 0;
        for (const auto& element : container)
        {
            const auto* address = &element;
            benchmark::DoNotOptimize(address);
            ++touched;
        }
        benchmark::DoNotOptimize(touched);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

/// Postings: n values over n/8 keys, inserted, then each key's values read.
template<typename C>
void postings(benchmark::State& state)
{
    const auto n = static_cast<std::size_t>(state.range(0));
    const auto postings_keys = keys<key_t<C>>(n / 8);
    for (auto _ : state)
    {
        auto container = C();
        for (std::size_t pos = 0; pos < n; ++pos)
            container.emplace(postings_keys[pos % postings_keys.size()], static_cast<int>(pos));
        std::int64_t sum = 0;
        for (const auto& key : postings_keys)
        {
            const auto [first, last] = container.equal_range(key);
            for (auto it = first; it != last; ++it)
                sum += it->second;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

template<typename T>
using PostingMap = urd::repr::PostingMap<T, int>;
template<typename T>
using HashMultiMap = std::unordered_multimap<T, int, urd::Hash<T>, urd::EqualTo<T>>;
template<typename T>
using TreeMultiMap = std::multimap<T, int, urd::Less<T>>;

template<template<typename> class C, typename T>
void add_postings(const std::string& name)
{
    for (const auto size : { 1'000, 100'000 })
        benchmark::RegisterBenchmark("postings<" + name + ">", postings<C<T>>)->Arg(size);
}

template<typename T>
using DenseSet = urd::repr::DenseSet<T>;
template<typename T>
using SparseSet = urd::repr::SparseSet<T>;
template<typename T>
using DenseMap = urd::repr::IndexMap<T, int, false>;
template<typename T>
using GenerationMap = urd::repr::IndexMap<T, int, true>;
template<typename T>
using IndexedSet = urd::repr::IndexedSet<T>;
template<typename T>
using HashSet = std::unordered_set<T, urd::Hash<T>, urd::EqualTo<T>>;
template<typename T>
using FlatHashSet = boost::unordered_flat_set<T, urd::Hash<T>, urd::EqualTo<T>>;
template<typename T>
using TreeSet = std::set<T, urd::Less<T>>;
template<typename T>
using FlatSet = boost::container::flat_set<T, urd::Less<T>>;
template<typename T>
using HashMap = std::unordered_map<T, int, urd::Hash<T>, urd::EqualTo<T>>;
template<typename T>
using FlatHashMap = boost::unordered_flat_map<T, int, urd::Hash<T>, urd::EqualTo<T>>;
template<typename T>
using TreeMap = std::map<T, int, urd::Less<T>>;
template<typename T>
using FlatMap = boost::container::flat_map<T, int, urd::Less<T>>;

// Inserting a random order into a sorted vector is O(n²): its insert and refill benchmarks stop at 10⁴.
/// A fifo frontier: `live` elements queued, then per step two pushes and two pops; time per step.
template<typename Q>
void frontier(benchmark::State& state)
{
    const auto live = static_cast<int>(state.range(0));
    auto queue = Q();
    for (int element = 0; element < live; ++element)
        queue.push(element);
    int step = 0;
    for (auto _ : state)
    {
        queue.push(step);
        queue.push(step + 1);
        benchmark::DoNotOptimize(&queue.front());
        queue.pop();
        benchmark::DoNotOptimize(&queue.front());
        queue.pop();
        ++step;
    }
}
/// A fifo grown from empty to n, then emptied; time per element.
template<typename Q>
void grow(benchmark::State& state)
{
    const auto count = static_cast<int>(state.range(0));
    for (auto _ : state)
    {
        auto queue = Q();
        for (int element = 0; element < count; ++element)
            queue.push(element);
        while (!queue.empty())
        {
            benchmark::DoNotOptimize(&queue.front());
            queue.pop();
        }
    }
    state.SetItemsProcessed(state.iterations() * count);
}
template<typename T>
using Fifo = urd::repr::Fifo<T>;
template<typename T>
using RingFifo = urd::repr::RingFifo<T>;
template<template<typename> class Q>
void add_queues(const std::string& name)
{
    for (const auto live : { 16, 1'000, 100'000, 1'000'000 })
        benchmark::RegisterBenchmark("frontier<" + name + ">", frontier<Q<int>>)->Arg(live);
    for (const auto count : { 1'000, 1'000'000 })
        benchmark::RegisterBenchmark("grow<" + name + ">", grow<Q<int>>)->Arg(count);
}

constexpr std::array<std::int64_t, 3> sizes { 100, 10'000, 100'000 };
constexpr std::size_t sizes_for_quadratic_inserts = 2;

template<template<typename> class C, typename T>
void add_benchmarks(const std::string& name, bool quadratic_inserts)
{
    const auto container = "<" + name + ">";
    for (std::size_t pos = 0; pos < sizes.size(); ++pos)
    {
        const auto size = sizes.at(pos);
        const bool inserts = !quadratic_inserts || pos < sizes_for_quadratic_inserts;
        if (inserts)
            benchmark::RegisterBenchmark("insert" + container, insert<C<T>>)->Arg(size);
        benchmark::RegisterBenchmark("lookup" + container, lookup<C<T>>)->Arg(size);
        if (inserts)
            benchmark::RegisterBenchmark("clear_refill" + container, clear_refill<C<T>>)->Arg(size);
        benchmark::RegisterBenchmark("iterate" + container, iterate<C<T>>)->Arg(size);
    }
}

}  // namespace

int main(int argc, char** argv)
{
    add_benchmarks<HashSet, int>("HashSet<int>", false);
    add_benchmarks<FlatHashSet, int>("FlatHashSet<int>", false);
    add_benchmarks<TreeSet, int>("TreeSet<int>", false);
    add_benchmarks<FlatSet, int>("FlatSet<int>", true);
    add_benchmarks<HashSet, Key32>("HashSet<Key32>", false);
    add_benchmarks<FlatHashSet, Key32>("FlatHashSet<Key32>", false);
    add_benchmarks<TreeSet, Key32>("TreeSet<Key32>", false);
    add_benchmarks<FlatSet, Key32>("FlatSet<Key32>", true);
    add_benchmarks<HashMap, int>("HashMap<int>", false);
    add_benchmarks<FlatHashMap, int>("FlatHashMap<int>", false);
    add_benchmarks<TreeMap, int>("TreeMap<int>", false);
    add_benchmarks<FlatMap, int>("FlatMap<int>", true);
    // Dense keys: indices 0..2n.
    add_benchmarks<DenseSet, int>("DenseSet<int>", false);
    add_benchmarks<SparseSet, int>("SparseSet<int>", false);
    add_benchmarks<DenseMap, int>("DenseMap<int>", false);
    add_benchmarks<GenerationMap, int>("GenerationMap<int>", false);
    // Insertion order: a vector plus a table of positions.
    add_benchmarks<IndexedSet, int>("IndexedSet<int>", false);
    add_benchmarks<IndexedSet, Key32>("IndexedSet<Key32>", false);
    // Multimaps: postings, several values per key.
    add_postings<PostingMap, int>("PostingMap<int>");
    add_postings<HashMultiMap, int>("HashMultiMap<int>");
    add_postings<TreeMultiMap, int>("TreeMultiMap<int>");
    add_postings<PostingMap, Key32>("PostingMap<Key32>");
    add_postings<HashMultiMap, Key32>("HashMultiMap<Key32>");
    add_postings<TreeMultiMap, Key32>("TreeMultiMap<Key32>");
    // Queues: a deque against a ring over one vector.
    add_queues<Fifo>("Fifo<int>");
    add_queues<RingFifo>("RingFifo<int>");
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
}
