// Sets and maps under four workloads, for every container a set or map request can resolve to. The numbers
// back the preference orders in include/urd/registry.hpp; docs/BENCHMARKS.md records them.

#include <array>
#include <benchmark/benchmark.h>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
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

/// A 32-byte key, as a small tuple of ids.
struct Key32
{
    std::array<std::uint64_t, 4> parts;
    friend bool operator==(const Key32&, const Key32&) = default;
    friend auto operator<=>(const Key32&, const Key32&) = default;
};

}  // namespace

template<>
struct std::hash<Key32>
{
    std::size_t operator()(const Key32& key) const noexcept
    {
        // Boost's hash_combine.
        constexpr std::size_t golden_ratio = 0x9e3779b97f4a7c15ULL;
        constexpr unsigned left = 6;
        std::size_t seed = 0;
        for (const auto part : key.parts)
            seed ^= std::hash<std::uint64_t> {}(part) + golden_ratio + (seed << left) + (seed >> 2U);
        return seed;
    }
};

namespace
{

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

template<typename T>
using HashSet = std::unordered_set<T>;
template<typename T>
using FlatHashSet = boost::unordered_flat_set<T, std::hash<T>>;
template<typename T>
using TreeSet = std::set<T>;
template<typename T>
using FlatSet = boost::container::flat_set<T>;
template<typename T>
using HashMap = std::unordered_map<T, int>;
template<typename T>
using FlatHashMap = boost::unordered_flat_map<T, int, std::hash<T>>;
template<typename T>
using TreeMap = std::map<T, int>;
template<typename T>
using FlatMap = boost::container::flat_map<T, int>;

// Inserting a random order into a sorted vector is O(n²): its insert and refill benchmarks stop at 10⁴.
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
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
}
