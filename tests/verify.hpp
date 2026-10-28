#pragma once

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

namespace urd
{

namespace detail
{

/// Whether the contents of `range` are as Traits says: sorted, unique, nonempty. The other properties are
/// about the type, not the contents; the checks below cover those. O(n log n) at most.
template<typename Traits, std::ranges::forward_range R>
[[nodiscard]] bool holds(const R& range)
{
    if (has<Traits, nonempty> && std::ranges::empty(range))
        return false;
    if (has<Traits, sorted> && !std::ranges::is_sorted(range))
        return false;
    if constexpr (has<Traits, unique>)
    {
        if constexpr (has<Traits, sorted>)
            return std::ranges::adjacent_find(range) == std::ranges::end(range);
        auto copy = std::vector<std::ranges::range_value_t<R>>(std::ranges::begin(range), std::ranges::end(range));
        std::ranges::sort(copy);
        return std::ranges::adjacent_find(copy) == copy.end();
    }
    return true;
}

template<typename R>
using element_t = std::ranges::range_value_t<R>;

template<typename R>
std::vector<const element_t<R>*> addresses(const R& range)
{
    auto result = std::vector<const element_t<R>*>();
    for (const auto& element : range)
        result.push_back(std::addressof(element));
    return result;
}

template<typename R>
std::ranges::iterator_t<const R> const_at(const R& range, std::size_t pos)
{ return std::ranges::next(std::ranges::begin(range), static_cast<std::ranges::range_difference_t<R>>(pos)); }

template<typename R>
std::size_t position_of(const R& range, const std::ranges::iterator_t<const R>& it)
{ return static_cast<std::size_t>(std::ranges::distance(std::ranges::begin(range), it)); }

/// What a container's contents must be after the same operations: placed by value as Traits says (in sorted
/// order, each element once), or where they were put.
template<typename Traits, typename E>
struct Model
{
    std::vector<E> elements;

    bool insert(const E& element)
    {
        if (has<Traits, unique> && std::ranges::find(elements, element) != elements.end())
            return false;
        if (has<Traits, sorted>)
            elements.insert(std::ranges::upper_bound(elements, element), element);
        else
            elements.push_back(element);
        return true;
    }

    void erase_one(const E& element) { elements.erase(std::ranges::find(elements, element)); }
};

/// Whether an insert's result says what the model's insert did: for a unique container, whether it inserted;
/// for any, an iterator to an element equal to `element`.
template<typename Result, typename E>
bool reports(const Result& result, bool inserted, const E& element)
{
    if constexpr (requires { result.second; })
        return result.second == inserted && *result.first == element;
    else
        return inserted && *result == element;
}

/// The first property `range` breaks after a step that left `size_before` elements at `before`, if any.
/// `probe` is looked up and compared with the model.
template<typename R, typename Traits = container_traits<R>>
std::optional<std::string> violation(const R& range,
                                     const Model<Traits, element_t<R>>& model,
                                     const std::vector<const element_t<R>*>& before,
                                     std::size_t size_before,
                                     const element_t<R>& probe)
{
    using E = element_t<R>;
    auto contents = std::vector<E>(std::ranges::begin(range), std::ranges::end(range));
    if (std::ranges::size(range) != contents.size())
        return "size() differs from the number of elements";

    auto expected = model.elements;
    if (by_value<Traits> && !has<Traits, sorted> && !has<Traits, insertion_ordered>)
    {
        std::ranges::sort(contents);
        std::ranges::sort(expected);
    }
    if (contents != expected)
        return "the contents differ from what the same operations give";

    if (has<Traits, sorted> && !holds<properties_of<sorted>>(range))
        return "sorted: the elements are out of order";
    if (has<Traits, unique> && !holds<properties_of<unique>>(range))
        return "unique: an element occurs twice";
    if (has<Traits, nonempty> && std::ranges::empty(range))
        return "nonempty: no element left";

    // An insert moves no element; an erase or clear ends only the references to what it removed.
    if constexpr (has<Traits, stable_refs>)
    {
        const auto after = addresses(range);
        const auto kept = std::set<const E*>(after.begin(), after.end());
        const auto lost = static_cast<std::size_t>(std::ranges::count_if(before, [&](const E* address) { return !kept.contains(address); }));
        if (lost != (size_before > contents.size() ? size_before - contents.size() : 0))
            return "stable_refs: an element that was not removed moved";
    }

    if constexpr (has<Traits, contiguous>)
    {
        const auto at = addresses(range);
        for (std::size_t pos = 1; pos < at.size(); ++pos)
            if (at[pos] != std::next(at[0], static_cast<std::ptrdiff_t>(pos)))
                return "contiguous: the elements are not next to each other";
    }

    // Equal contents, built in another order, give an equal container.
    if constexpr (has<Traits, canonical>)
    {
        std::ranges::reverse(contents);
        if (!(R(contents.begin(), contents.end()) == range))
            return "canonical: equal contents built in another order differ";
    }

    if constexpr (by_value<Traits>)
    {
        const auto present = std::ranges::find(model.elements, probe) != model.elements.end();
        const auto found = range.find(probe);
        if ((found != std::ranges::end(range)) != present || (present && !(*found == probe)))
            return "find: wrong for an element looked up";
        if (static_cast<std::size_t>(range.count(probe)) != static_cast<std::size_t>(std::ranges::count(model.elements, probe)))
            return "count: wrong for an element looked up";
        if (range.contains(probe) != present)
            return "contains: wrong for an element looked up";
    }
    if constexpr (has<Traits, sorted>)
    {
        const auto lower = static_cast<std::size_t>(std::ranges::lower_bound(model.elements, probe) - model.elements.begin());
        const auto upper = static_cast<std::size_t>(std::ranges::upper_bound(model.elements, probe) - model.elements.begin());
        const auto [first, last] = range.equal_range(probe);
        if (position_of(range, range.lower_bound(probe)) != lower || position_of(range, range.upper_bound(probe)) != upper || position_of(range, first) != lower
            || position_of(range, last) != upper)
            return "lower_bound, upper_bound, equal_range: wrong positions";
    }
    return std::nullopt;
}

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
map_violation(const R& range, const MapModel<Traits, K, V>& model, const std::vector<const element_t<R>*>& before, std::size_t size_before, const K& probe)
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

    if constexpr (has<Traits, stable_refs>)
    {
        const auto after = addresses(range);
        const auto kept = std::set<const element_t<R>*>(after.begin(), after.end());
        const auto lost = static_cast<std::size_t>(std::ranges::count_if(before, [&](const auto* address) { return !kept.contains(address); }));
        if (lost != (size_before > contents.size() ? size_before - contents.size() : 0))
            return "stable_refs: an element that was not removed moved";
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
            const auto before = addresses(range);
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

/// urd::verify for a queue: pushes, pops and clears, after each of which the next element to leave and the size are compared
/// with a model, the elements in the order pushed.
template<typename Q>
std::optional<std::string> run_queue(Q& queue, unsigned seed, int steps)
{
    using E = typename Q::value_type;
    using Traits = container_traits<Q>;
    auto model = std::vector<E>();
    // Which of the model's elements leaves next: the first, the last, or the least (the first of equal ones).
    const auto next = [&model]
    {
        if constexpr (has<Traits, fifo>)
            return model.begin();
        else if constexpr (has<Traits, lifo>)
            return std::prev(model.end());
        else
            return std::ranges::min_element(model, Less<E> {});
    };
    const auto front = [&queue]() -> const E&
    {
        if constexpr (has<Traits, fifo>)
            return queue.front();
        else
            return queue.top();
    };

    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, 31);
    for (int step = 0; step < steps; ++step)
    {
        const int drawn = pick(random);
        auto operation = std::string();
        // Push more often than pop, so the queue grows and shrinks; now and then clear it.
        if (has<Traits, shrinkable> && drawn == 31)
        {
            operation = "clear";
            queue.clear();
            model.clear();
        }
        else if (model.empty() || drawn % 5 < 3)
        {
            operation = "push " + std::to_string(drawn);
            if (drawn % 2 == 0)
                queue.push(E(drawn));
            else
                queue.emplace(drawn);
            model.push_back(E(drawn));
        }
        else
        {
            operation = "pop";
            if (!(front() == *next()))
                return "step " + std::to_string(step) + " (pop): the element leaving is not the one the order says";
            queue.pop();
            model.erase(next());
        }
        if (static_cast<std::size_t>(queue.size()) != model.size() || queue.empty() != model.empty())
            return "step " + std::to_string(step) + " (" + operation + "): size() differs from the number of elements";
        if (!model.empty() && !(front() == *next()))
            return "step " + std::to_string(step) + " (" + operation + "): the next element to leave is not the one the order says";
    }
    return std::nullopt;
}

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
                handles.push_back(pool.insert(handles[base], std::span<const typename P::write_type>(writes)));
                rows.push_back(row);
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
                handles.push_back(pool.insert(handles[base], std::span<const E>(added), std::span<const E>(removed)));
                rows.push_back(canonical(elements));
                cleared = true;  // done with this step's insert
            }
        }
        if (!cleared)
        {
            auto elements = std::vector<E>();
            for (int count = length(random); count > 0; --count)
                elements.push_back(E(pick(random)));
            handles.push_back(pool.insert(elements));
            rows.push_back(canonical(elements));
        }
        const auto label = "step " + std::to_string(step) + " (" + operation + "): ";
        // Inserting stores more or nothing; only clear lowers it.
        const auto memory = pool.memory_usage();
        if (operation != "clear" && memory < memory_before)
            return label + "memory_usage fell without a clear";
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

/// urd::verify for a conforming R.
template<typename R>
std::optional<std::string> run(R& range, unsigned seed, int steps)
{
    using E = detail::element_t<R>;
    using Traits = container_traits<R>;
    constexpr bool keyed = by_value<Traits>;
    constexpr int universe = 32;  // small, so inserts meet duplicates and erases find elements

    // The operations R offers: inserts (by value, or at the end), removals and, for a sequence, inserts before
    // a position, and assignments through iterators where R's iterators allow them.
    constexpr int inserts = has<Traits, growable> ? (keyed ? 5 : 3) : 0;
    constexpr int removals = has<Traits, shrinkable> ? (keyed ? 3 : 5) : 0;
    constexpr int writes = !keyed && std::indirectly_writable<std::ranges::iterator_t<R>, const E&> ? 1 : 0;

    auto model = detail::Model<Traits, E> { std::vector<E>(std::ranges::begin(range), std::ranges::end(range)) };
    if (auto found = detail::violation(range, model, {}, 0, E(0)))
        return "at the start: " + *found;
    if constexpr (inserts + removals + writes == 0)
        return std::nullopt;  // nothing changes it, and the start was checked
    else
    {
        auto random = std::mt19937(seed);
        auto pick = std::uniform_int_distribution<int>(0, universe - 1);
        auto choose = std::uniform_int_distribution<int>(0, inserts + removals + writes - 1);
        const auto position = [&](std::size_t bound) { return std::uniform_int_distribution<std::size_t>(0, bound)(random); };

        for (int step = 0; step < steps; ++step)
        {
            const auto before = detail::addresses(range);
            const auto size_before = std::ranges::size(range);
            const int drawn = pick(random);
            const auto element = E(drawn);
            auto operation = std::string();
            auto wrong = std::string();
            int op = choose(random);

            if (has<Traits, shrinkable> && step % 100 == 99)
            {
                if constexpr (has<Traits, shrinkable>)
                {
                    operation = "clear";
                    range.clear();
                    model.elements.clear();
                }
            }
            else if (op < inserts)
            {
                if constexpr (inserts > 0 && keyed)
                {
                    if (op == 0)
                    {
                        operation = "insert " + std::to_string(drawn);
                        const auto inserted = model.insert(element);
                        if (!detail::reports(range.insert(element), inserted, element))
                            wrong = "insert reports the wrong outcome";
                    }
                    else if (op == 1)
                    {
                        operation = "insert moved " + std::to_string(drawn);
                        const auto inserted = model.insert(element);
                        if (!detail::reports(range.insert(E(element)), inserted, element))
                            wrong = "insert reports the wrong outcome";
                    }
                    else if (op == 2)
                    {
                        operation = "emplace " + std::to_string(drawn);
                        const auto inserted = model.insert(element);
                        if (!detail::reports(range.emplace(drawn), inserted, element))
                            wrong = "emplace reports the wrong outcome";
                    }
                    else if (op == 3)
                    {
                        const auto batch = std::vector<E> { element, E(pick(random)), element };
                        operation = "insert a range from " + std::to_string(drawn);
                        range.insert(batch.begin(), batch.end());
                        for (const auto& each : batch)
                            model.insert(each);
                    }
                    else
                    {
                        operation = "insert with a hint " + std::to_string(drawn);
                        const auto hint = detail::const_at(range, position(std::ranges::size(range)));
                        model.insert(element);
                        if (!(*range.insert(hint, element) == element))
                            wrong = "insert with a hint returns the wrong element";
                    }
                }
                else if constexpr (inserts > 0)
                {
                    if (op == 0)
                    {
                        operation = "push_back " + std::to_string(drawn);
                        range.push_back(element);
                    }
                    else if (op == 1)
                    {
                        operation = "push_back moved " + std::to_string(drawn);
                        range.push_back(E(element));
                    }
                    else
                    {
                        operation = "emplace_back " + std::to_string(drawn);
                        const auto& made = range.emplace_back(drawn);
                        if (!(made == element) || std::addressof(made) != std::addressof(*detail::const_at(range, size_before)))
                            wrong = "emplace_back returns the wrong element";
                    }
                    model.elements.push_back(element);
                }
            }
            else if (op - inserts < removals)
            {
                if constexpr (removals > 0)
                {
                    op -= inserts;
                    const auto size = std::ranges::size(range);
                    // Op 0 removes by value (kept by value) or the last element (a sequence); ops 3 and 4, which
                    // only a sequence has, insert before a position.
                    if (op == 0)
                    {
                        if constexpr (keyed)
                        {
                            operation = "erase " + std::to_string(drawn);
                            if (static_cast<std::size_t>(range.erase(element)) != static_cast<std::size_t>(std::erase(model.elements, element)))
                                wrong = "erase reports the wrong count";
                        }
                        else if (size > 0)
                        {
                            operation = "pop_back";
                            range.pop_back();
                            model.elements.pop_back();
                        }
                    }
                    else if (op >= 3)
                    {
                        if constexpr (!keyed)
                        {
                            const auto pos = position(size);
                            operation = (op == 3 ? "insert " : "emplace ") + std::to_string(drawn) + " at " + std::to_string(pos);
                            const auto at = detail::const_at(range, pos);
                            const auto made = op == 3 ? range.insert(at, element) : range.emplace(at, drawn);
                            model.elements.insert(model.elements.begin() + static_cast<std::ptrdiff_t>(pos), element);
                            if (detail::position_of(range, made) != pos || !(*made == element))
                                wrong = operation.substr(0, operation.find(' ')) + " returns the wrong position";
                        }
                    }
                    else if (size > 0)
                    {
                        // Erase one position (op 1) or a range of them (op 2).
                        const auto first = position(size - 1);
                        const auto last = op == 1 ? first + 1 : first + position(size - first);
                        operation = "erase positions " + std::to_string(first) + " to " + std::to_string(last);
                        const auto removed = std::vector<E>(detail::const_at(range, first), detail::const_at(range, last));
                        // The iterator after the erased positions; erase(position) may return a proxy that converts.
                        std::ranges::iterator_t<R> next;
                        if (op == 1)
                            next = range.erase(detail::const_at(range, first));
                        else
                            next = range.erase(detail::const_at(range, first), detail::const_at(range, last));
                        if constexpr (keyed)
                            for (const auto& each : removed)
                                model.erase_one(each);
                        else
                        {
                            model.elements.erase(model.elements.begin() + static_cast<std::ptrdiff_t>(first),
                                                 model.elements.begin() + static_cast<std::ptrdiff_t>(last));
                            if (detail::position_of(range, std::ranges::iterator_t<const R>(next)) != first)
                                wrong = "erase returns the wrong position";
                        }
                    }
                }
            }
            else if (std::ranges::size(range) > 0)
            {
                if constexpr (writes > 0)
                {
                    const auto pos = position(std::ranges::size(range) - 1);
                    operation = "assign " + std::to_string(drawn) + " at " + std::to_string(pos);
                    *std::ranges::next(std::ranges::begin(range), static_cast<std::ranges::range_difference_t<R>>(pos)) = element;
                    model.elements[pos] = element;
                }
            }
            if (operation.empty())
                continue;  // a removal from an empty container

            const auto label = "step " + std::to_string(step) + " (" + operation + "): ";
            if (!wrong.empty())
                return label + wrong;
            if (auto found = detail::violation(range, model, before, size_before, E(pick(random))))
                return label + *found;
        }
        return std::nullopt;
    }
}

}  // namespace detail

/// Checks that `range` offers the operations container_traits<R> requires (interface.hpp; a compile-time
/// error otherwise) and keeps every declared property through them. It runs `steps` random operations on small
/// integers, each operation R's properties require, from inserts to erases of positions and ranges, and after
/// each one compares the contents and the lookups with a reference model and checks each declared property,
/// with the exceptions their definitions make. concurrent is not checked here: it needs several threads,
/// under ThreadSanitizer.
///
/// Returns the first violation, naming the step and what broke, or nothing.
template<typename R>
    requires detail::queue_traits<container_traits<R>> || has<container_traits<R>, handles> || std::constructible_from<detail::element_t<R>, int>
             || detail::map_container<R>
[[nodiscard]] std::optional<std::string> verify(R range, unsigned seed, int steps = 200)
{
    static_assert(conforming<R>, "urd::verify: R lacks an operation its declared properties require (include/urd/interface.hpp lists them)");
    if constexpr (!conforming<R>)
        return std::nullopt;
    else if constexpr (detail::queue_traits<container_traits<R>>)
        return detail::run_queue(range, seed, steps);
    else if constexpr (has<container_traits<R>, handles, unique>)
        return detail::run_pool(range, seed, steps);
    else if constexpr (has<container_traits<R>, handles>)
        return detail::run_slots(range, seed, steps);
    else if constexpr (detail::map_container<R>)
        return detail::run_map(range, seed, steps);
    else
        return detail::run(range, seed, steps);
}

}  // namespace urd
