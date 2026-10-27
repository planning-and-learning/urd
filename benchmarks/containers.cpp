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
        std::size_t seed = 0;
        for (const auto part : key.parts)
            seed ^= std::hash<std::uint64_t> {}(part) + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

namespace
{

template<typename T>
T make_key(std::uint64_t value)
{
    if constexpr (std::same_as<T, Key32>)
        return Key32 { { value, value * 3, value * 5, value * 7 } };
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
    std::shuffle(result.begin(), result.end(), std::mt19937(42));
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

// Inserting a random order into a sorted vector is O(n²): its insert benchmarks stop at 10⁴.
void small(benchmark::internal::Benchmark* benchmark) { benchmark->Arg(100)->Arg(10'000); }
void large(benchmark::internal::Benchmark* benchmark) { benchmark->Arg(100)->Arg(10'000)->Arg(100'000); }

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

}  // namespace

#define URD_CONTAINER(C, T, insert_sizes)                        \
    BENCHMARK_TEMPLATE(insert, C<T>)->Apply(insert_sizes);       \
    BENCHMARK_TEMPLATE(lookup, C<T>)->Apply(large);              \
    BENCHMARK_TEMPLATE(clear_refill, C<T>)->Apply(insert_sizes); \
    BENCHMARK_TEMPLATE(iterate, C<T>)->Apply(large)

URD_CONTAINER(HashSet, int, large);
URD_CONTAINER(FlatHashSet, int, large);
URD_CONTAINER(TreeSet, int, large);
URD_CONTAINER(FlatSet, int, small);
URD_CONTAINER(HashSet, Key32, large);
URD_CONTAINER(FlatHashSet, Key32, large);
URD_CONTAINER(TreeSet, Key32, large);
URD_CONTAINER(FlatSet, Key32, small);
URD_CONTAINER(HashMap, int, large);
URD_CONTAINER(FlatHashMap, int, large);
URD_CONTAINER(TreeMap, int, large);
URD_CONTAINER(FlatMap, int, small);

BENCHMARK_MAIN();
