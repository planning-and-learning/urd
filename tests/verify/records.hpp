#pragma once

// urd::verify for pools of records stored flat, and for layered ones.

#include "common.hpp"
#include "urd/semantics/less.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::detail
{

/// urd::verify for a pool of records stored flat: inserts of records drawn from a small universe (so that equal ones
/// recur), every fourth through a factory given the reserved index, and clears. After each, the pool holds exactly
/// the distinct records given, in first-insertion order under indices 0..n-1: each comes back by its index, is found
/// at it, and is contained; an absent one is found at none; a re-insert gives the index it has and no insertion;
/// the factory runs once where the record was new and with the index it got; memory never falls but on clear.
template<typename P>
std::optional<std::string> run_records(P& pool, unsigned seed, int steps)
{
    using T = typename P::value_type;
    using Index = typename P::handle_type;
    using Traits = container_traits<P>;
    auto model = std::vector<T>();
    // Where references are stable, each record's address when it went in; aligned with the model.
    auto placed = std::vector<const T*>();
    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, 23);
    auto memory_before = std::size_t { 0 };
    const auto failed = [&](int step, const std::string& what) { return std::optional<std::string>("step " + std::to_string(step) + ": " + what); };
    for (int step = 0; step < steps; ++step)
    {
        bool cleared = false;
        if constexpr (container_shrinks<P>)
        {
            if (step % 97 == 96)
            {
                pool.clear();
                model.clear();
                placed.clear();
                memory_before = 0;
                cleared = true;
            }
        }
        if (!cleared)
        {
            const auto record = T(pick(random));
            auto expected = std::optional<std::size_t>();
            for (std::size_t each = 0; each < model.size(); ++each)
                if (EqualTo<T> {}(model[each], record))
                    expected = each;
            auto result = std::pair<Index, bool>();
            int calls = 0;
            auto given = Index();
            if (step % 4 == 3)
                result = pool.insert(record,
                                     [&](Index at)
                                     {
                                         ++calls;
                                         given = at;
                                         return T(record);
                                     });
            else
                result = pool.insert(record);
            if (result.second == expected.has_value())
                return failed(step,
                              "insert reported " + std::string(result.second ? "new" : "held") + " for a record the model " + (expected ? "holds" : "lacks"));
            if (result.first.value != (expected ? *expected : model.size()))
                return failed(step,
                              "insert gave index " + std::to_string(result.first.value) + ", expected " + std::to_string(expected ? *expected : model.size()));
            if (step % 4 == 3 && (calls != (result.second ? 1 : 0) || (result.second && given != result.first)))
                return failed(step, "the factory ran " + std::to_string(calls) + " times, given index " + std::to_string(given.value));
            if (result.second)
            {
                model.push_back(record);
                placed.push_back(std::addressof(pool[result.first]));
            }
        }
        if (pool.size() != model.size())
            return failed(step, "size " + std::to_string(pool.size()) + ", expected " + std::to_string(model.size()));
        for (std::size_t each = 0; each < model.size(); ++each)
        {
            const auto at = Index(static_cast<std::uint32_t>(each));
            if (!EqualTo<T> {}(pool[at], model[each]))
                return failed(step, "the record at " + std::to_string(each) + " is not the one inserted");
            if (pool.find(model[each]) != at || !pool.contains(model[each]))
                return failed(step, "the record at " + std::to_string(each) + " is not found at its index");
            if (has<Traits, stable_refs> && std::addressof(pool[at]) != placed[each])
                return failed(step, "stable_refs: the record at " + std::to_string(each) + " moved");
        }
        if (!pool.find(T(1000)).is_none() || pool.contains(T(1000)))
            return failed(step, "a record never inserted is found");
        if (pool.memory_usage() < memory_before)
            return failed(step, "memory_usage fell without a clear");
        memory_before = pool.memory_usage();
    }
    return std::nullopt;
}

/// urd::verify for a layered pool of records: a root holding eight records, a middle layer over it holding four
/// more, and `leaf` over that, which takes the random inserts of run_records. The chain is the model: the leaf
/// answers every record of every layer at the index it got there, finds a record held anywhere without adding it,
/// numbers new ones after the chain, numbers its layer 2, starts its own indices at the chain's size, and after a
/// clear holds the chain and nothing more.
template<typename P>
std::optional<std::string> run_layered_records(P& fresh, unsigned seed, int steps)
{
    using T = typename P::value_type;
    using Index = typename P::handle_type;
    auto root = P();
    auto middle = P();
    // The leaf is a local, declared after its parents, so that it never outlives them.
    auto leaf = std::move(fresh);
    auto chain = std::vector<T>();
    for (int drawn = 0; drawn < 8; ++drawn)
        if (root.insert(T(drawn)).second)
            chain.push_back(T(drawn));
    middle.layer_over(&root);
    for (int drawn = 8; drawn < 12; ++drawn)
        if (middle.insert(T(drawn)).second)
            chain.push_back(T(drawn));
    leaf.layer_over(&middle);
    const auto base = chain.size();
    // Where references are stable, each record's address through the leaf when it went in; aligned with the chain.
    auto placed = std::vector<const T*>();
    for (std::size_t each = 0; each < base; ++each)
        placed.push_back(std::addressof(leaf[Index(static_cast<std::uint32_t>(each))]));
    const auto failed = [&](int step, const std::string& what) { return std::optional<std::string>("step " + std::to_string(step) + ": " + what); };
    if (root.layer() != 0 || middle.layer() != 1 || leaf.layer() != 2)
        return failed(-1, "the layers are not numbered 0, 1, 2 along the chain");
    if (leaf.base() != base || middle.base() != root.size() || root.base() != 0)
        return failed(-1, "a layer's indices do not start at its parent's size");
    if (leaf.size() != base || leaf.parent_grew())
        return failed(-1, "a fresh layer is not its chain");
    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, 23);
    using Traits = container_traits<P>;
    for (int step = 0; step < steps; ++step)
    {
        bool cleared = false;
        if constexpr (container_shrinks<P>)
        {
            if (step % 97 == 96)
            {
                leaf.clear();
                chain.resize(base);
                placed.resize(base);
                cleared = true;
            }
        }
        if (!cleared)
        {
            const auto record = T(pick(random));
            auto expected = std::optional<std::size_t>();
            for (std::size_t each = 0; each < chain.size(); ++each)
                if (EqualTo<T> {}(chain[each], record))
                    expected = each;
            auto result = std::pair<Index, bool>();
            auto given = Index();
            if (step % 4 == 3)
                result = leaf.insert(record,
                                     [&](Index at)
                                     {
                                         given = at;
                                         return T(record);
                                     });
            else
                result = leaf.insert(record);
            if (result.second == expected.has_value())
                return failed(step,
                              "insert reported " + std::string(result.second ? "new" : "held") + " for a record the chain " + (expected ? "holds" : "lacks"));
            if (result.first.value != (expected ? *expected : chain.size()))
                return failed(step,
                              "insert gave index " + std::to_string(result.first.value) + ", expected " + std::to_string(expected ? *expected : chain.size()));
            if (step % 4 == 3 && result.second && given != result.first)
                return failed(step, "the factory was given another index than the record got");
            if (result.second)
            {
                chain.push_back(record);
                placed.push_back(std::addressof(leaf[result.first]));
            }
        }
        if (leaf.size() != chain.size() || leaf.base() != base)
            return failed(step, "size " + std::to_string(leaf.size()) + ", expected " + std::to_string(chain.size()));
        for (std::size_t each = 0; each < chain.size(); ++each)
        {
            const auto at = Index(static_cast<std::uint32_t>(each));
            if (!EqualTo<T> {}(leaf[at], chain[each]))
                return failed(step, "the record at " + std::to_string(each) + " is not the chain's");
            if (leaf.find(chain[each]) != at || !leaf.contains(chain[each]))
                return failed(step, "the record at " + std::to_string(each) + " is not found at its index through the chain");
            if (has<Traits, stable_refs> && std::addressof(leaf[at]) != placed[each])
                return failed(step, "stable_refs: the record at " + std::to_string(each) + " moved");
        }
        if (root.size() != 8 || middle.size() != base || leaf.parent_grew())
            return failed(step, "a parent changed");
    }
    return std::nullopt;
}

}  // namespace urd::detail
