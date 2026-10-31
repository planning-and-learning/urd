#pragma once

// urd::verify for pools: interning pools of rows (handles, unique) and pools of slots (handles).

#include "common.hpp"
#include "urd/interface.hpp"
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

/// urd::verify for a pool: inserts of random rows, of rows written from (or, for sets, added to and removed from)
/// earlier ones, (short, over few values, so that equal rows recur) and clears.
/// After each, every handle given so far is checked: equal rows have equal handles, others different ones, and each
/// handle gives its row back, by get and by read. Rows are compared as the row type keeps them (a set's in order).
template<typename P>
std::optional<std::string> run_pool(P& pool, unsigned seed, int steps)
{
    using Row = typename P::value_type;
    using E = std::ranges::range_value_t<Row>;
    using Traits = container_traits<P>;
    const auto canonical = [](const std::vector<E>& elements)
    {
        const auto row = Row(elements.begin(), elements.end());
        return std::vector<E>(row.begin(), row.end());
    };
    auto rows = std::vector<std::vector<E>>();
    auto handles = std::vector<typename P::handle_type>();
    const auto find = [&pool](const std::vector<E>& elements) { return pool.find(elements); };
    // An insert stored something exactly where find gave none just before it.
    auto flag_wrong = false;
    const auto record = [&](std::pair<typename P::handle_type, bool> inserted, std::vector<E> row, bool absent)
    {
        flag_wrong = flag_wrong || inserted.second != absent;
        handles.push_back(inserted.first);
        rows.push_back(std::move(row));
    };

    constexpr bool sequence_rows = !by_value<traits_t<Row>>;
    auto random = std::mt19937(seed);
    // Sequence rows over few values, so that equal rows recur; set rows over a range of several blocks.
    auto pick = std::uniform_int_distribution<int>(0, sequence_rows ? 3 : 99);
    auto length = std::uniform_int_distribution<int>(0, sequence_rows ? 6 : 12);
    auto memory_before = std::size_t { 0 };
    for (int step = 0; step < steps; ++step)
    {
        auto operation = std::string("insert");
        bool cleared = false;
        if constexpr (has<Traits, shrinkable>)
        {
            if (step % 97 == 96)
            {
                operation = "clear";
                pool.clear();
                rows.clear();
                handles.clear();
                cleared = true;
            }
        }
        // Now and then a row erased, where the pool erases: the last row moves into its handle. The model drops every
        // entry of the erased row and gives the moved row's entries the erased handle.
        if constexpr (requires { pool.erase(handles.front()); })
        {
            if (!cleared && !rows.empty() && step % 7 == 6)
            {
                operation = "erase";
                const auto victim = handles[static_cast<std::size_t>(random() % handles.size())];
                const auto erased_row = rows[static_cast<std::size_t>(std::ranges::find(handles, victim) - handles.begin())];
                const auto last = typename P::handle_type(static_cast<std::uint32_t>(pool.size() - 1));
                const auto moved = pool.erase(victim);
                if (moved != (victim == last ? typename P::handle_type() : last))
                    return "step " + std::to_string(step) + " (erase): erase gives another handle than the last row's, or none where it moved";
                for (std::size_t each = rows.size(); each-- > 0;)
                    if (handles[each] == victim)
                    {
                        rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(each));
                        handles.erase(handles.begin() + static_cast<std::ptrdiff_t>(each));
                    }
                for (auto& handle : handles)
                    if (!moved.is_none() && handle == moved)
                        handle = victim;
                if (!find(erased_row).is_none())
                    return "step " + std::to_string(step) + " (erase): the erased row is still found";
                cleared = true;  // done with this step's change
            }
        }
        const auto base = rows.empty() ? rows.size() : static_cast<std::size_t>(random() % rows.size());
        if constexpr (sequence_rows)
        {
            // Now and then a row written from an earlier one: some of its positions, in order, get new values.
            if (!cleared && base < rows.size() && !rows[base].empty() && step % 3 == 0)
            {
                operation = "insert by writes";
                auto writes = std::vector<typename P::write_type>();
                auto row = rows[base];
                for (std::size_t position = 0; position < row.size(); ++position)
                    if (random() % 3 == 0)
                    {
                        writes.push_back({ position, E(pick(random)) });
                        row[position] = writes.back().value;
                    }
                const bool absent = find(row).is_none();
                record(pool.insert(handles[base], std::span<const typename P::write_type>(writes)), row, absent);
                cleared = true;  // done with this step's insert
            }
        }
        else
        {
            // Now and then a set derived from an earlier one: some elements added, then some removed.
            if (!cleared && base < rows.size() && step % 3 == 0)
            {
                operation = "insert by added and removed";
                auto added = std::vector<E>();
                auto removed = std::vector<E>();
                for (int count = length(random) / 3; count > 0; --count)
                    added.push_back(E(pick(random)));
                for (int count = length(random) / 3; count > 0; --count)
                    removed.push_back(rows[base].empty() || random() % 2 == 0 ? E(pick(random)) : rows[base][random() % rows[base].size()]);
                auto elements = rows[base];
                elements.insert(elements.end(), added.begin(), added.end());
                std::erase_if(elements, [&removed](const E& each) { return std::ranges::find(removed, each) != removed.end(); });
                const bool absent = find(elements).is_none();
                record(pool.insert(handles[base], std::span<const E>(added), std::span<const E>(removed)), canonical(elements), absent);
                cleared = true;  // done with this step's insert
            }
        }
        if (!cleared)
        {
            auto elements = std::vector<E>();
            for (int count = length(random); count > 0; --count)
                elements.push_back(E(pick(random)));
            const bool absent = find(elements).is_none();
            record(pool.insert(elements), canonical(elements), absent);
        }
        const auto label = "step " + std::to_string(step) + " (" + operation + "): ";
        if (flag_wrong)
            return label + "insert says it stored something where find held the row, or the reverse";
        // Inserting stores more or nothing; only clear lowers it.
        const auto memory = pool.memory_usage();
        if (operation != "clear" && operation != "erase" && memory < memory_before)
            return label + "memory_usage fell without a clear or an erase";
        memory_before = memory;
        for (std::size_t each = 0; each < rows.size(); ++each)
        {
            const auto got = pool.get(handles[each]);
            if (!std::ranges::equal(got, rows[each]))
                return label + "get gives back another row";
            auto out = std::vector<E>(pool.row_size(handles[each]));
            pool.read(handles[each], out);
            if (out != rows[each])
                return label + "read gives back another row";
            if (!rows.empty() && (rows[each] == rows.back()) != (handles[each] == handles.back()))
                return label + "equal rows have different handles, or different rows equal ones";
            if constexpr (!sequence_rows)
                for (int probe = 0; probe < 4; ++probe)
                {
                    const auto element = E(pick(random));
                    if (pool.contains(handles[each], element) != (std::ranges::find(rows[each], element) != rows[each].end()))
                        return label + "contains answers otherwise than the row";
                }
            if (find(rows[each]) != handles[each])
                return label + "find misses a stored row, or gives another handle";
        }
        // find stores nothing; without structural_sharing it finds only rows inserted, and the handles are
        // 0..size()-1, one per distinct row.
        auto probe = std::vector<E>();
        for (int count = length(random); count > 0; --count)
            probe.push_back(E(pick(random)));
        const auto found = find(probe);
        if (pool.memory_usage() != memory)
            return label + "find stored something";
        if constexpr (!has<Traits, structural_sharing>)
        {
            const auto inserted = std::ranges::find(rows, canonical(probe));
            if (found.is_none() != (inserted == rows.end()) || (!found.is_none() && found != handles[static_cast<std::size_t>(inserted - rows.begin())]))
                return label + "find answers otherwise than the rows inserted";
            auto distinct = rows;
            std::ranges::sort(distinct);
            const auto count = static_cast<std::size_t>(std::ranges::distance(distinct.begin(), std::ranges::unique(distinct).begin()));
            if (pool.size() != count)
                return label + "size is not the number of distinct rows";
            if (std::ranges::any_of(handles, [&pool](const auto handle) { return handle.value >= pool.size(); }))
                return label + "a handle is not below size()";
        }
    }
    return std::nullopt;
}

/// urd::verify for a pool of slots: inserts, emplaces, acquires, erases, writes through handles and clears. After each,
/// every live handle reads what was put there, live elements have not moved, and an insert after an erase reuses a
/// freed slot.
template<typename P>
std::optional<std::string> run_slots(P& pool, unsigned seed, int steps)
{
    using E = typename P::value_type;
    using Traits = container_traits<P>;
    using Handle = typename P::handle_type;
    auto live = std::vector<std::pair<Handle, E>>();
    auto addresses = std::vector<const E*>();
    auto freed = std::vector<Handle>();

    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, 31);
    for (int step = 0; step < steps; ++step)
    {
        const int drawn = pick(random);
        auto operation = std::string();
        const auto added = [&](Handle handle, const E& value) -> std::optional<std::string>
        {
            if (!freed.empty() && std::ranges::find(freed, handle) == freed.end())
                return std::string("a slot was freed, but the insert took a new one");
            std::erase(freed, handle);
            live.emplace_back(handle, value);
            addresses.push_back(&std::as_const(pool)[handle]);
            return std::nullopt;
        };
        std::optional<std::string> wrong;
        if constexpr (has<Traits, shrinkable>)
        {
            if (drawn == 31)
            {
                operation = "clear";
                pool.clear();
                live.clear();
                addresses.clear();
                freed.clear();
            }
            else if (drawn % 3 == 0 && !live.empty())
            {
                const auto victim = static_cast<std::size_t>(drawn) % live.size();
                operation = "erase";
                pool.erase(live[victim].first);
                freed.push_back(live[victim].first);
                live.erase(live.begin() + static_cast<std::ptrdiff_t>(victim));
                addresses.erase(addresses.begin() + static_cast<std::ptrdiff_t>(victim));
            }
        }
        if (operation.empty())
        {
            if (drawn % 4 == 1 && !live.empty())
            {
                operation = "write";
                const auto target = static_cast<std::size_t>(drawn) % live.size();
                pool[live[target].first] = E(drawn);
                live[target].second = E(drawn);
            }
            else if (drawn % 4 == 2)
            {
                operation = "emplace";
                wrong = added(pool.emplace(drawn), E(drawn));
            }
            else if constexpr (std::default_initializable<E>)
            {
                if (drawn % 4 == 3)
                {
                    // An acquired slot holds whatever it held; the step writes it, as a user would.
                    operation = "acquire";
                    const auto handle = pool.acquire();
                    pool[handle] = E(drawn);
                    wrong = added(handle, E(drawn));
                }
            }
            if (operation.empty())
            {
                operation = "insert";
                wrong = added(pool.insert(E(drawn)), E(drawn));
            }
        }
        const auto label = "step " + std::to_string(step) + " (" + operation + "): ";
        if (wrong)
            return label + *wrong;
        if (static_cast<std::size_t>(pool.size()) != live.size())
            return label + "size() differs from the number of live elements";
        for (std::size_t each = 0; each < live.size(); ++each)
        {
            if (!(std::as_const(pool)[live[each].first] == live[each].second))
                return label + "a handle reads another value";
            if (&std::as_const(pool)[live[each].first] != addresses[each])
                return label + "stable_refs: a live element moved";
            for (std::size_t other = each + 1; other < live.size(); ++other)
                if (live[each].first == live[other].first)
                    return label + "two live elements share a handle";
        }
    }
    return std::nullopt;
}

}  // namespace urd::detail
