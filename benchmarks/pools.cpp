// Pools of states, as a search stores them: each state its parent with a few positions changed. The tree pools
// (structural_sharing) against the flat row interner: time per state inserted and read, and bytes per state. The
// numbers back docs/BENCHMARKS.md.

#include "urd/use/pools.hpp"

#include "urd/store.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/sequences.hpp"

#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

namespace
{

constexpr std::size_t states = 20'000;
constexpr int changes = 3;
constexpr unsigned seed = 7;          // the same states in every run
constexpr std::uint32_t values = 16;  // values a position takes, so that subtrees recur
constexpr unsigned true_share = 10;   // one fact in this many is true

using Values = urd::Store<urd::value<std::uint32_t>, urd::frozen>;
using Facts = urd::Store<urd::key<std::uint32_t>, urd::dense, urd::frozen>;
using FlatValues = urd::Store<urd::value<Values>, urd::handles, urd::unique>;
using TreeValues = urd::Store<urd::value<Values>, urd::handles, urd::unique, urd::structural_sharing>;
using FlatFacts = urd::Store<urd::value<Facts>, urd::handles, urd::unique>;
using TreeFacts = urd::Store<urd::value<Facts>, urd::handles, urd::unique, urd::structural_sharing>;

/// States of `width` values: a random first one, then each a random earlier one with `changes` positions rewritten.
template<typename Pool>
void value_states(benchmark::State& state)
{
    const auto width = static_cast<std::size_t>(state.range(0));
    for (auto _ : state)
    {
        auto random = std::mt19937(seed);
        auto pool = Pool();
        auto first = std::vector<std::uint32_t>(width);
        for (auto& value : first)
            value = random() % values;
        auto handles = std::vector<typename Pool::handle_type> { pool.insert(first).first };
        handles.reserve(states);
        for (std::size_t each = 1; each < states; ++each)
        {
            auto writes = std::vector<urd::Write<std::uint32_t>>();
            for (int change = 0; change < changes; ++change)
                writes.push_back(
                    { (width / changes) * static_cast<std::size_t>(change) + random() % (width / changes), static_cast<std::uint32_t>(random() % values) });
            handles.push_back(pool.insert(handles[random() % handles.size()], std::span<const urd::Write<std::uint32_t>>(writes)).first);
        }
        auto row = std::vector<std::uint32_t>(width);
        for (const auto handle : handles)
            pool.read(handle, row);
        benchmark::DoNotOptimize(row);
        state.counters["bytes/state"] = static_cast<double>(pool.memory_usage()) / static_cast<double>(states);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(states));
}

/// States of `width` facts, about a tenth of them true: each a random earlier one with `changes` facts added and as
/// many removed.
template<typename Pool>
void fact_states(benchmark::State& state)
{
    const auto width = static_cast<std::uint32_t>(state.range(0));
    for (auto _ : state)
    {
        auto random = std::mt19937(seed);
        auto pool = Pool();
        auto first = std::vector<std::uint32_t>();
        for (std::uint32_t fact = 0; fact < width; ++fact)
            if (random() % true_share == 0)
                first.push_back(fact);
        auto handles = std::vector<typename Pool::handle_type> { pool.insert(first).first };
        handles.reserve(states);
        for (std::size_t each = 1; each < states; ++each)
        {
            auto added = std::vector<std::uint32_t>();
            auto removed = std::vector<std::uint32_t>();
            for (int change = 0; change < changes; ++change)
            {
                added.push_back(static_cast<std::uint32_t>(random() % width));
                removed.push_back(static_cast<std::uint32_t>(random() % width));
            }
            handles.push_back(
                pool.insert(handles[random() % handles.size()], std::span<const std::uint32_t>(added), std::span<const std::uint32_t>(removed)).first);
        }
        std::size_t total = 0;
        for (const auto handle : handles)
            total += pool.contains(handle, width / 2) ? 1U : 0U;
        benchmark::DoNotOptimize(total);
        state.counters["bytes/state"] = static_cast<double>(pool.memory_usage()) / static_cast<double>(states);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(states));
}

}  // namespace

BENCHMARK_TEMPLATE(value_states, FlatValues)->Arg(100)->Arg(1000)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(value_states, TreeValues)->Arg(100)->Arg(1000)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(fact_states, FlatFacts)->Arg(1000)->Arg(10000)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(fact_states, TreeFacts)->Arg(1000)->Arg(10000)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
