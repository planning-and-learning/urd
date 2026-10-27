// Every property a Store promises holds after every operation, not only at the end: each Store below runs
// random inserts and erases (those it offers), and after each step its contents match a reference model
// and every requested property is checked.

#include "check.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <random>
#include <set>
#include <vector>

namespace
{

using urd::key;
using urd::props;
using urd::value;

constexpr int steps = 2000;
constexpr int universe = 32;  // small, so inserts meet duplicates and erases find elements

template<typename S>
constexpr bool has(props wanted)
{
    return urd::includes(S::properties, wanted);
}

template<typename S>
constexpr bool is_set = requires(const S& store, int element) { store.contains(element); };

template<typename S>
constexpr bool can_insert = requires(S& store, int element) { store.insert(element); };

template<typename S>
constexpr bool can_erase = requires(S& store, int element) { store.erase(element); };

/// What a Store's contents must be after the same operations: a set, or a sequence kept as its properties say.
template<typename S>
struct Model
{
    std::vector<int> elements;

    bool insert(int element)
    {
        const bool unique = is_set<S> || has<S>(props::unique);
        if (unique && std::ranges::find(elements, element) != elements.end())
            return false;
        if (has<S>(props::sorted))
            elements.insert(std::ranges::upper_bound(elements, element), element);
        else
            elements.push_back(element);
        return true;
    }

    std::size_t erase(int element) { return std::erase(elements, element); }
};

template<typename S>
std::vector<const int*> addresses(const S& store)
{
    auto result = std::vector<const int*>();
    for (const int& element : store)
        result.push_back(std::addressof(element));
    return result;
}

template<typename S>
void check_step(const S& store, const Model<S>& model, const std::vector<const int*>& before, std::size_t size_before)
{
    auto contents = std::vector<int>(store.begin(), store.end());
    URD_CHECK(store.size() == contents.size());
    URD_CHECK(store.empty() == contents.empty());

    // The contents, in order where order is promised.
    auto expected = model.elements;
    if (is_set<S> && !has<S>(props::sorted))
    {
        std::ranges::sort(contents);
        std::ranges::sort(expected);
    }
    URD_CHECK(contents == expected);

    URD_CHECK(urd::holds<S::properties & (props::sorted | props::unique | props::nonempty)>(store));
    if constexpr (is_set<S>)
        URD_CHECK(urd::holds<props::unique>(store));
    if constexpr (has<S>(props::monotone))
        URD_CHECK(store.size() >= size_before);

    // Each step is one insert or one erase: an insert moves no element, an erase removes only what it erased.
    if constexpr (has<S>(props::stable_refs))
    {
        const auto after = addresses(store);
        const auto kept = std::set<const int*>(after.begin(), after.end());
        const auto lost = static_cast<std::size_t>(std::ranges::count_if(before, [&](const int* address) { return !kept.contains(address); }));
        URD_CHECK(lost == (size_before > contents.size() ? size_before - contents.size() : 0));
    }

    if constexpr (has<S>(props::contiguous))
    {
        const auto at = addresses(store);
        for (std::size_t pos = 1; pos < at.size(); ++pos)
            URD_CHECK(at[pos] == at[0] + pos);
    }

    // Equal contents, built in another order, give an equal Store.
    if constexpr (has<S>(props::canonical) && !has<S>(props::nonempty))
    {
        auto reversed = std::vector<int>(store.begin(), store.end());
        std::ranges::reverse(reversed);
        URD_CHECK(S::make(reversed) == store);
    }
}

/// Runs random operations on `store`, checking every property after each one.
template<typename S>
void run(S store, unsigned seed)
{
    auto model = Model<S> { std::vector<int>(store.begin(), store.end()) };
    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, universe - 1);
    auto coin = std::bernoulli_distribution(0.6);
    check_step(store, model, {}, 0);

    for (int step = 0; step < steps; ++step)
    {
        const auto before = addresses(store);
        const auto size_before = store.size();
        const int element = pick(random);
        if constexpr (can_insert<S> && can_erase<S>)
        {
            if (coin(random))
                URD_CHECK(store.insert(element) == model.insert(element));
            else
                URD_CHECK(store.erase(element) == model.erase(element));
        }
        else if constexpr (can_insert<S>)
            URD_CHECK(store.insert(element) == model.insert(element));
        check_step(store, model, before, size_before);
    }
}

}  // namespace

int main()
{
    // Sets: hash set, tree set, sorted unique vector.
    run(urd::Store<key<int>>(), 1);
    run(urd::Store<key<int>, urd::sorted>(), 2);
    run(urd::Store<key<int>, urd::small>(), 3);
    run(urd::Store<key<int>, urd::sorted, urd::frozen>::make({ 5, 3, 3, 9 }), 4);
    run(urd::Store<key<int>, urd::nonempty>::assume({ 7 }), 5);

    // Sequences: vector in every order, segmented vector.
    run(urd::Store<value<int>>(), 6);
    run(urd::Store<value<int>, urd::sorted>(), 7);
    run(urd::Store<value<int>, urd::unique>(), 8);
    run(urd::Store<value<int>, urd::sorted, urd::unique>(), 9);
    run(urd::Store<value<int>, urd::monotone>(), 10);
    run(urd::Store<value<int>, urd::nonempty, urd::monotone>::assume({ 1 }), 11);
    run(urd::Store<value<int>, urd::stable_refs, urd::monotone>(), 12);
    run(urd::Store<value<int>, urd::monotone, urd::concurrent>(), 13);
    run(urd::Store<key<int>, urd::stable_refs>(), 14);
    run(urd::Store<key<int>, urd::sorted, urd::stable_refs>(), 15);
}
