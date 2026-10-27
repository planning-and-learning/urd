#pragma once

#include "urd/repr/ops.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
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
using element_t = std::remove_cvref_t<std::ranges::range_reference_t<const R>>;

/// A container whose ops look elements up: a set, compared without regard to order unless sorted.
template<typename R>
concept looks_up = requires(const R& range, const element_t<R>& element) { repr::ops<R>::contains(range, element); };

template<typename R>
std::vector<const element_t<R>*> addresses(const R& range)
{
    auto result = std::vector<const element_t<R>*>();
    for (const auto& element : range)
        result.push_back(std::addressof(element));
    return result;
}

/// What a container's contents must be after the same operations, kept as Traits says: in sorted order,
/// each element once.
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

    std::size_t erase(const E& element) { return std::erase(elements, element); }
};

/// The first property `range` breaks after a step that left `size_before` elements at `before`, if any.
template<typename R, typename Traits = container_traits<R>>
std::optional<std::string> violation(const R& range, const Model<Traits, element_t<R>>& model,
                                     const std::vector<const element_t<R>*>& before, std::size_t size_before)
{
    using E = element_t<R>;
    auto contents = std::vector<E>(std::ranges::begin(range), std::ranges::end(range));
    if (std::ranges::size(range) != contents.size())
        return "size() differs from the number of elements";

    auto expected = model.elements;
    if (looks_up<R> && !has<Traits, sorted>)
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
    if constexpr (has<Traits, canonical> && requires { repr::ops<R>::make(std::vector<E>()); })
    {
        std::ranges::reverse(contents);
        if (!(repr::ops<R>::make(std::move(contents)) == range))
            return "canonical: equal contents built in another order differ";
    }
    return std::nullopt;
}

}  // namespace detail

/// Checks that `range` keeps every property container_traits<R> declares through every operation it offers.
/// It runs `steps` random operations through repr::ops<R> (inserts, and erases and clears where R is
/// shrinkable) on small integers, and after each one compares the contents with a reference model and checks
/// each declared property, with the exceptions their definitions make. concurrent is not checked here: it
/// needs several threads, under ThreadSanitizer.
///
/// Returns the first violation, naming the step and the property, or nothing.
template<typename R>
    requires std::constructible_from<detail::element_t<R>, int>
[[nodiscard]] std::optional<std::string> verify(R range, unsigned seed, int steps = 2000)
{
    using E = detail::element_t<R>;
    using Traits = container_traits<R>;
    using ops = repr::ops<R>;
    constexpr int universe = 32;  // small, so inserts meet duplicates and erases find elements

    auto model = detail::Model<Traits, E> { std::vector<E>(std::ranges::begin(range), std::ranges::end(range)) };
    if (auto found = detail::violation(range, model, {}, 0))
        return "at the start: " + *found;

    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, universe - 1);
    auto coin = std::bernoulli_distribution(0.6);
    for (int step = 0; step < steps; ++step)
    {
        const auto before = detail::addresses(range);
        const auto size_before = std::ranges::size(range);
        const int drawn = pick(random);
        const auto element = E(drawn);
        auto operation = std::string();
        if constexpr (has<Traits, shrinkable>)
        {
            if (step % 500 == 499)
            {
                operation = "clear";
                ops::clear(range);
                model.elements.clear();
            }
            else if (coin(random))
            {
                operation = "insert " + std::to_string(drawn);
                if (ops::insert(range, element) != model.insert(element))
                    return "step " + std::to_string(step) + " (" + operation + "): insert reports the wrong outcome";
            }
            else
            {
                operation = "erase " + std::to_string(drawn);
                if (ops::erase(range, element) != model.erase(element))
                    return "step " + std::to_string(step) + " (" + operation + "): erase reports the wrong count";
            }
        }
        else if constexpr (has<Traits, growable>)
        {
            operation = "insert " + std::to_string(drawn);
            if (ops::insert(range, element) != model.insert(element))
                return "step " + std::to_string(step) + " (" + operation + "): insert reports the wrong outcome";
        }
        else
            return std::nullopt;  // nothing changes it, and the start was checked

        if (auto found = detail::violation(range, model, before, size_before))
            return "step " + std::to_string(step) + " (" + operation + "): " + *found;
    }
    return std::nullopt;
}

}  // namespace urd
