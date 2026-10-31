#pragma once

// urd::verify for maps: random inserts, erases and clears checked against a model of the entries (MapModel).

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

/// The order of a map ordered by its values: the one a representation was built with, or a Store's request gave.
template<typename R>
struct value_order
{
    using type = typename container_traits<R>::order_type;
};
template<typename R>
    requires requires { typename R::order_type; }
struct value_order<R>
{
    using type = typename R::order_type;
};
template<typename R>
using value_order_t = typename value_order<R>::type;

/// What a map's contents must be after the same operations: pairs in key order where sorted, each key once
/// where unique, a multimap's equal keys in insertion order.
template<typename Traits, typename K, typename V>
struct MapModel
{
    std::vector<std::pair<K, V>> pairs;

    [[nodiscard]] auto find(const K& key) const { return std::ranges::find(pairs, key, &std::pair<K, V>::first); }
    [[nodiscard]] std::size_t count(const K& key) const { return static_cast<std::size_t>(std::ranges::count(pairs, key, &std::pair<K, V>::first)); }
    bool insert(const K& key, const V& value)
    {
        if (has<Traits, unique> && find(key) != pairs.end())
            return false;
        if (has<Traits, sorted>)
            pairs.insert(std::ranges::upper_bound(pairs, key, {}, &std::pair<K, V>::first), { key, value });
        else
            pairs.emplace_back(key, value);
        return true;
    }
    /// insert_or_assign: whether it inserted.
    bool assign(const K& key, const V& value)
    {
        const auto found = std::ranges::find(pairs, key, &std::pair<K, V>::first);
        if (found == pairs.end())
            return insert(key, value);
        found->second = value;
        return false;
    }
    std::size_t erase(const K& key)
    {
        return std::erase_if(pairs, [&](const auto& pair) { return pair.first == key; });
    }
    void erase_one(const std::pair<K, V>& pair) { pairs.erase(std::ranges::find(pairs, pair)); }
};

/// The first property a map `range` breaks, as violation() for other containers; `probe` is a key.
template<typename R, typename Traits, typename K, typename V>
std::optional<std::string>
map_violation(const R& range, const MapModel<Traits, K, V>& model, const std::vector<const V*>& before, std::size_t size_before, const K& probe)
{
    auto contents = std::vector<std::pair<K, V>>();
    for (const auto& [key, value] : range)
        contents.emplace_back(key, value);
    if (std::ranges::size(range) != contents.size())
        return "size() differs from the number of elements";
    if constexpr (orders_by_value<R>)
    {
        if (!std::ranges::is_sorted(range, value_order_t<R> {}))
            return "sorted: the entries are out of the order the request gave";
    }
    // Where keys repeat, the standard leaves the order among them to the insert (a hint places an element near
    // it), so the pairs are compared as a multiset; key order is checked below.
    auto expected = model.pairs;
    // Compared in order where the map promises one: sorted with unique keys, or insertion order.
    if (!has<Traits, insertion_ordered> && (!has<Traits, sorted> || !has<Traits, unique>) )
    {
        std::ranges::sort(contents);
        std::ranges::sort(expected);
    }
    if (contents != expected)
        return "the contents differ from what the same operations give";

    auto keys = std::vector<K>();
    for (const auto& element : range)
        keys.push_back(element.first);
    if (has<Traits, sorted> && !std::ranges::is_sorted(keys))
        return "sorted: the keys are out of order";
    std::ranges::sort(keys);
    if (has<Traits, unique> && std::ranges::adjacent_find(keys) != keys.end())
        return "unique: a key occurs twice";
    if (has<Traits, nonempty> && std::ranges::empty(range))
        return "nonempty: no element left";

    // A map's stable reference is to a value: an insert moves no value, an erase or clear ends only the references
    // to the values it removes, and at(key) gives the value iteration refers to.
    if constexpr (has<Traits, stable_refs>)
    {
        const auto after = value_addresses(range);
        const auto kept = std::set<const V*>(after.begin(), after.end());
        const auto lost = static_cast<std::size_t>(std::ranges::count_if(before, [&](const V* address) { return !kept.contains(address); }));
        if (lost != (size_before > contents.size() ? size_before - contents.size() : 0))
            return "stable_refs: a value that was not removed moved";
        if constexpr (has<Traits, unique> && requires(const R& constant, const K& key) {
                          { constant.at(key) } -> std::same_as<const V&>;
                      })
            for (const auto& entry : range)
                if (std::addressof(range.at(entry.first)) != std::addressof(entry.second))
                    return "stable_refs: at(key) and iteration refer to different values";
    }
    if constexpr (has<Traits, contiguous>)
    {
        const auto at = addresses(range);
        for (std::size_t pos = 1; pos < at.size(); ++pos)
            if (at[pos] != std::next(at[0], static_cast<std::ptrdiff_t>(pos)))
                return "contiguous: the elements are not next to each other";
    }
    if constexpr (has<Traits, canonical>)
    {
        std::ranges::reverse(contents);
        if (!(R(contents.begin(), contents.end()) == range))
            return "canonical: equal contents built in another order differ";
    }

    const auto found = range.find(probe);
    const auto present = model.find(probe) != model.pairs.end();
    if ((found != std::ranges::end(range)) != present || (present && !(found->first == probe)))
        return "find: wrong for a key looked up";
    if (has<Traits, unique> && present && !(found->second == model.find(probe)->second))
        return "find: the wrong value for a key";
    if (static_cast<std::size_t>(range.count(probe)) != model.count(probe))
        return "count: wrong for a key looked up";
    if (range.contains(probe) != present)
        return "contains: wrong for a key looked up";
    if constexpr (has<Traits, unique>)
        if (present && !(range.at(probe) == model.find(probe)->second))
            return "at: the wrong value for a key";
    if constexpr (has<Traits, sorted>)
    {
        const auto lower = static_cast<std::size_t>(std::ranges::count_if(model.pairs, [&](const auto& pair) { return pair.first < probe; }));
        const auto upper = lower + model.count(probe);
        const auto [first, last] = range.equal_range(probe);
        if (position_of(range, range.lower_bound(probe)) != lower || position_of(range, range.upper_bound(probe)) != upper || position_of(range, first) != lower
            || position_of(range, last) != upper)
            return "lower_bound, upper_bound, equal_range: wrong positions";
    }
    return std::nullopt;
}

/// urd::verify for a conforming map R: inserts of every kind the standard's maps have, erases by key, position
/// and range, and clears, each checked against the model.
template<typename R>
std::optional<std::string> run_map(R& range, unsigned seed, int steps)
{
    using K = std::remove_const_t<typename R::key_type>;
    using V = typename R::mapped_type;
    using Traits = container_traits<R>;
    constexpr bool unique_keys = has<Traits, unique>;
    constexpr int inserts = has<Traits, growable> ? (unique_keys ? 6 : 4) : 0;
    constexpr int removals = has<Traits, shrinkable> ? 3 : 0;

    // A map ordered by its values keeps no key order; its own order is checked by map_violation.
    using ModelTraits = std::conditional_t<orders_by_value<R>, with_t<Traits, any_order>, Traits>;
    auto model = MapModel<ModelTraits, K, V>();
    for (const auto& [key, value] : range)
        model.pairs.emplace_back(key, value);
    if (auto found = map_violation(range, model, {}, 0, K(0)))
        return "at the start: " + *found;
    if constexpr (inserts + removals == 0)
        return std::nullopt;
    else
    {
        auto random = std::mt19937(seed);
        auto pick = std::uniform_int_distribution<int>(0, 31);
        auto pick_value = std::uniform_int_distribution<int>(0, 99);
        auto choose = std::uniform_int_distribution<int>(0, inserts + removals - 1);
        const auto position = [&](std::size_t bound) { return std::uniform_int_distribution<std::size_t>(0, bound)(random); };

        for (int step = 0; step < steps; ++step)
        {
            const auto before = value_addresses(range);
            const auto size_before = std::ranges::size(range);
            const int drawn = pick(random);
            const auto key = K(drawn);
            const auto value = V(pick_value(random));
            auto operation = std::string();
            auto wrong = std::string();
            int op = choose(random);
            const auto name = [&](const char* what) { return std::string(what) + " " + std::to_string(drawn); };
            const auto reports = [&](const auto& result, bool inserted)
            {
                if constexpr (requires { result.second; })
                    return result.second == inserted && result.first->first == key;
                else
                    return inserted && result->first == key && result->second == value;
            };

            if (has<Traits, shrinkable> && step % 100 == 99)
            {
                if constexpr (has<Traits, shrinkable>)
                {
                    operation = "clear";
                    range.clear();
                    model.pairs.clear();
                }
            }
            else if (op < inserts)
            {
                if constexpr (inserts > 0)
                {
                    if (op == 0)
                    {
                        operation = name("insert");
                        const auto inserted = model.insert(key, value);
                        if (!reports(range.insert(std::pair<const K, V>(key, value)), inserted))
                            wrong = "insert reports the wrong outcome";
                    }
                    else if (op == 1)
                    {
                        operation = name("emplace");
                        const auto inserted = model.insert(key, value);
                        if (!reports(range.emplace(key, value), inserted))
                            wrong = "emplace reports the wrong outcome";
                    }
                    else if (op == 2)
                    {
                        operation = name("insert a range from");
                        const auto batch = std::vector<std::pair<K, V>> { { key, value }, { K(pick(random)), value }, { key, V(value + 1) } };
                        range.insert(batch.begin(), batch.end());
                        for (const auto& [each_key, each_value] : batch)
                            model.insert(each_key, each_value);
                    }
                    else if (op == 3)
                    {
                        operation = name("insert with a hint");
                        model.insert(key, value);
                        if (!(range.insert(const_at(range, position(std::ranges::size(range))), std::pair<const K, V>(key, value))->first == key))
                            wrong = "insert with a hint returns the wrong element";
                    }
                    else if constexpr (unique_keys)
                    {
                        if (op == 4)
                        {
                            operation = name("try_emplace");
                            const auto old = model.find(key);
                            const auto expected = old == model.pairs.end() ? value : old->second;
                            const auto inserted = model.insert(key, value);
                            const auto result = range.try_emplace(key, value);
                            if (result.second != inserted || !(result.first->second == expected))
                                wrong = "try_emplace reports the wrong outcome or overwrote a value";
                        }
                        else
                        {
                            operation = name("insert_or_assign");
                            const auto inserted = model.assign(key, value);
                            const auto result = range.insert_or_assign(key, value);
                            if (result.second != inserted || !(result.first->second == value))
                                wrong = "insert_or_assign reports the wrong outcome or kept the old value";
                        }
                    }
                }
            }
            else if constexpr (removals > 0)
            {
                op -= inserts;
                const auto size = std::ranges::size(range);
                if (op == 0)
                {
                    operation = name("erase");
                    if (static_cast<std::size_t>(range.erase(key)) != model.erase(key))
                        wrong = "erase reports the wrong count";
                }
                else if (size > 0)
                {
                    const auto first = position(size - 1);
                    const auto last = op == 1 ? first + 1 : first + position(size - first);
                    operation = "erase positions " + std::to_string(first) + " to " + std::to_string(last);
                    auto removed = std::vector<std::pair<K, V>>();
                    for (auto it = const_at(range, first); it != const_at(range, last); ++it)
                        removed.emplace_back(it->first, it->second);
                    if (op == 1)
                        range.erase(const_at(range, first));
                    else
                        range.erase(const_at(range, first), const_at(range, last));
                    for (const auto& pair : removed)
                        model.erase_one(pair);
                }
            }
            if (operation.empty())
                continue;

            const auto label = "step " + std::to_string(step) + " (" + operation + "): ";
            if (!wrong.empty())
                return label + wrong;
            if (auto found = map_violation(range, model, before, size_before, K(pick(random))))
                return label + *found;
        }
        return std::nullopt;
    }
}

}  // namespace urd::detail
