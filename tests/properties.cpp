// Every container keeps the properties it declares through every operation: urd::verify runs random inserts,
// erases and clears on each representation and each Store, and checks every declared property after each step.
// A container that declares a property it lacks is caught.

#include "check.hpp"
#include "urd/store.hpp"
#include "urd/verify.hpp"

#include <boost/container/flat_set.hpp>

#include <cstdio>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

using urd::key;
using urd::value;

template<typename R>
void verified(R range, unsigned seed)
{
    const auto found = urd::verify(std::move(range), seed);
    if (found)
        std::fprintf(stderr, "%s\n", found->c_str());
    URD_CHECK(!found);
}

/// A vector that wrongly claims its elements never move.
struct ClaimsStableRefs
{
    std::vector<int> elements;
    [[nodiscard]] auto begin() const { return elements.begin(); }
    [[nodiscard]] auto end() const { return elements.end(); }
    [[nodiscard]] std::size_t size() const { return elements.size(); }
};

}  // namespace

template<>
struct urd::container_traits<ClaimsStableRefs> : urd::properties_of<urd::stable_refs, urd::growable>
{
};

template<>
struct urd::repr::ops<ClaimsStableRefs>
{
    static bool insert(ClaimsStableRefs& range, int element)
    {
        range.elements.push_back(element);
        return true;
    }
};

int main()
{
    // The representations, as they are.
    verified(std::vector<int>(), 1);
    verified(std::set<int>(), 2);
    verified(std::unordered_set<int>(), 3);
    verified(boost::container::flat_set<int>(), 4);
    verified(boost::container::flat_multiset<int>(), 5);
    verified(urd::repr::SegmentedVector<int>(), 6);
    verified(urd::repr::SegmentedVector<int, urd::repr::default_first_segment_size, true>(), 7);

    // Stores, which promise what they were asked for.
    verified(urd::Store<key<int>>(), 11);
    verified(urd::Store<key<int>, urd::sorted>(), 12);
    verified(urd::Store<key<int>, urd::small>(), 13);
    verified(urd::Store<key<int>, urd::sorted, urd::frozen>::make({ 5, 3, 3, 9 }), 14);
    verified(urd::Store<key<int>, urd::nonempty>::assume({ 7 }), 15);
    verified(urd::Store<key<int>, urd::stable_refs>(), 16);
    verified(urd::Store<key<int>, urd::sorted, urd::stable_refs>(), 17);
    verified(urd::Store<value<int>>(), 21);
    verified(urd::Store<value<int>, urd::sorted>(), 22);
    verified(urd::Store<value<int>, urd::sorted, urd::unique>(), 23);
    verified(urd::Store<value<int>, urd::monotone>(), 24);
    verified(urd::Store<value<int>, urd::nonempty, urd::monotone>::assume({ 1 }), 25);
    verified(urd::Store<value<int>, urd::stable_refs, urd::monotone>(), 26);
    verified(urd::Store<value<int>, urd::monotone, urd::concurrent>(), 27);

    // A false claim fails: the vector moves its elements as it grows.
    const auto found = urd::verify(ClaimsStableRefs(), 31);
    URD_CHECK(found && found->find("stable_refs") != std::string::npos);
}
