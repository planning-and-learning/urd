// Every operation of Refined keeps its properties, and does what the plain container would.

#include "check.hpp"
#include "urd/refined.hpp"

#include <algorithm>
#include <cstddef>
#include <random>
#include <set>
#include <vector>

namespace
{

constexpr int RANGE = 20;
constexpr int STEPS = 2000;

/// What the contents should be after the same operations, computed without urd.
std::vector<int> expected(const std::vector<int>& history, urd::props properties)
{
    auto out = std::vector<int>();
    for (const int value : history)
        if (!urd::includes(properties, urd::props::unique) || std::ranges::find(out, value) == out.end())
            out.push_back(value);
    if (urd::includes(properties, urd::props::sorted))
        std::ranges::sort(out);
    return out;
}

/// Random inserts and erases, checking the properties and the contents after each.
template<urd::props P>
void random_operations(unsigned seed)
{
    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, RANGE);
    auto refined = urd::Refined<std::vector<int>, P>();
    auto history = std::vector<int>();  // the inserted values in order, minus erased ones

    for (int step = 0; step < STEPS; ++step)
    {
        const int value = pick(random);
        if (pick(random) % 3 == 0)
        {
            const std::size_t erased = refined.erase(value);
            URD_CHECK(erased == static_cast<std::size_t>(std::ranges::count(expected(history, P), value)));
            std::erase(history, value);
        }
        else
        {
            const bool present = std::ranges::find(history, value) != history.end();
            URD_CHECK(refined.insert(value) == (!urd::includes(P, urd::props::unique) || !present));
            history.push_back(value);
        }
        URD_CHECK(urd::holds<P>(refined));
        // Unsorted contents keep insertion order; with `unique`, of first occurrences.
        URD_CHECK(std::vector<int>(refined.begin(), refined.end()) == expected(history, P));
    }
}

/// make() gives the expected contents for any input.
template<urd::props P>
void make_from_random_input(unsigned seed)
{
    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, RANGE);
    for (int round = 0; round < 200; ++round)
    {
        auto input = std::vector<int>(static_cast<std::size_t>(pick(random)));
        for (int& value : input)
            value = pick(random);
        const auto made = urd::Refined<std::vector<int>, P>::make(input);
        URD_CHECK(urd::holds<P>(made));
        URD_CHECK(std::vector<int>(made.begin(), made.end()) == expected(input, P));
    }
}

}  // namespace

int main()
{
    constexpr auto sorted = urd::props::sorted;
    constexpr auto unique = urd::props::unique;

    for (unsigned seed = 0; seed < 10; ++seed)
    {
        random_operations<urd::props::none>(seed);
        random_operations<sorted>(seed);
        random_operations<unique>(seed);
        random_operations<sorted | unique>(seed);
        make_from_random_input<sorted>(seed);
        make_from_random_input<unique>(seed);
        make_from_random_input<sorted | unique>(seed);
    }

    // Dropping properties keeps the contents.
    const auto both = urd::SortedUniqueVector<int>::make({ 3, 1, 2 });
    const urd::SortedVector<int> sorted_only = both;
    URD_CHECK(std::vector<int>(sorted_only.begin(), sorted_only.end()) == std::vector<int>({ 1, 2, 3 }));

    // A view has the contents and the properties of what it views.
    const auto view = both.view();
    URD_CHECK(view.size() == 3 && view[0] == 1 && view[2] == 3);
}
