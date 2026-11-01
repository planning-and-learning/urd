#pragma once

// urd::verify for queues: pushes, pops and clears checked against a model of the elements in the order pushed.

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

    // Where references are stable, an element has one address whenever it is the next to leave: the one seen
    // when it first was, kept while others queued behind it (a fifo) or hid it and left again (a stack). A heap's
    // equal elements cannot be told apart, so its next element is not tracked. Aligned with the model.
    auto placed = std::vector<const E*>();
    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, 31);
    for (int step = 0; step < steps; ++step)
    {
        const int drawn = pick(random);
        auto operation = std::string();
        // Push more often than pop, so the queue grows and shrinks; now and then clear it.
        if (container_shrinks<Q> && drawn == 31)
        {
            operation = "clear";
            queue.clear();
            model.clear();
            placed.clear();
        }
        else if (model.empty() || drawn % 5 < 3)
        {
            operation = "push " + std::to_string(drawn);
            if (drawn % 2 == 0)
                queue.push(E(drawn));
            else
                queue.emplace(drawn);
            model.push_back(E(drawn));
            placed.push_back(nullptr);
        }
        else
        {
            operation = "pop";
            if (!(front() == *next()))
                return "step " + std::to_string(step) + " (pop): the element leaving is not the one the order says";
            queue.pop();
            placed.erase(placed.begin() + (next() - model.begin()));
            model.erase(next());
        }
        if (static_cast<std::size_t>(queue.size()) != model.size() || queue.empty() != model.empty())
            return "step " + std::to_string(step) + " (" + operation + "): size() differs from the number of elements";
        if (!model.empty() && !(front() == *next()))
            return "step " + std::to_string(step) + " (" + operation + "): the next element to leave is not the one the order says";
        if constexpr (has<Traits, stable_refs> && !has<Traits, by_priority>)
        {
            if (!model.empty())
            {
                auto& place = placed[static_cast<std::size_t>(next() - model.begin())];
                if (place == nullptr)
                    place = std::addressof(front());
                else if (place != std::addressof(front()))
                    return "step " + std::to_string(step) + " (" + operation + "): stable_refs: the next element to leave moved";
            }
        }
    }
    return std::nullopt;
}

}  // namespace urd::detail
