#pragma once

#include "urd/interface.hpp"
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
{
    return std::ranges::next(std::ranges::begin(range), static_cast<std::ranges::range_difference_t<R>>(pos));
}

template<typename R>
std::size_t position_of(const R& range, std::ranges::iterator_t<const R> it)
{
    return static_cast<std::size_t>(std::ranges::distance(std::ranges::begin(range), it));
}

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
std::optional<std::string> violation(const R& range, const Model<Traits, element_t<R>>& model,
                                     const std::vector<const element_t<R>*>& before, std::size_t size_before,
                                     const element_t<R>& probe)
{
    using E = element_t<R>;
    auto contents = std::vector<E>(std::ranges::begin(range), std::ranges::end(range));
    if (std::ranges::size(range) != contents.size())
        return "size() differs from the number of elements";

    auto expected = model.elements;
    if (by_value<Traits> && !has<Traits, sorted>)
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
            if (at[pos] != at[0] + pos)
                return "contiguous: the elements are not next to each other";
    }

    // Equal contents, built in another order, give an equal container.
    if constexpr (has<Traits, canonical>)
    {
        std::ranges::reverse(contents);
        if (!(R(contents.data(), contents.data() + contents.size()) == range))
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
        if (position_of(range, range.lower_bound(probe)) != lower || position_of(range, range.upper_bound(probe)) != upper
            || position_of(range, first) != lower || position_of(range, last) != upper)
            return "lower_bound, upper_bound, equal_range: wrong positions";
    }
    return std::nullopt;
}

/// urd::verify for a conforming R.
template<typename R>
std::optional<std::string> run(R range, unsigned seed, int steps)
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
                        range.insert(batch.data(), batch.data() + batch.size());
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
            else if ((op -= inserts) < removals)
            {
                if constexpr (removals > 0)
                {
                    const auto size = std::ranges::size(range);
                    if (keyed && op == 0)
                    {
                        if constexpr (keyed)
                        {
                            operation = "erase " + std::to_string(drawn);
                            if (static_cast<std::size_t>(range.erase(element)) != static_cast<std::size_t>(std::erase(model.elements, element)))
                                wrong = "erase reports the wrong count";
                        }
                    }
                    else if (!keyed && op == 0 && size > 0)
                    {
                        if constexpr (!keyed)
                        {
                            operation = "pop_back";
                            range.pop_back();
                            model.elements.pop_back();
                        }
                    }
                    else if (!keyed && op >= 3)
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
                        [[maybe_unused]] const auto next = op == 1 ? range.erase(detail::const_at(range, first))
                                                  : range.erase(detail::const_at(range, first), detail::const_at(range, last));
                        if constexpr (keyed)
                            for (const auto& each : removed)
                                model.erase_one(each);
                        else
                        {
                            model.elements.erase(model.elements.begin() + static_cast<std::ptrdiff_t>(first),
                                                 model.elements.begin() + static_cast<std::ptrdiff_t>(last));
                            if (detail::position_of(range, next) != first)
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
    requires std::constructible_from<detail::element_t<R>, int>
[[nodiscard]] std::optional<std::string> verify(R range, unsigned seed, int steps = 200)
{
    static_assert(conforming<R>, "urd::verify: R lacks an operation its declared properties require (include/urd/interface.hpp lists them)");
    if constexpr (conforming<R>)
        return detail::run(std::move(range), seed, steps);
    else
        return std::nullopt;
}

}  // namespace urd
