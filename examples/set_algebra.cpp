// Algorithms state the properties they need and compute the properties of their result.

#include "urd/algorithms.hpp"

#include <iostream>
#include <set>
#include <vector>

namespace
{

void print(const char* label, const auto& range)
{
    std::cout << label << ":";
    for (const auto& value : range)
        std::cout << ' ' << value;
    std::cout << '\n';
}

}  // namespace

int main()
{
    // std::set is sorted and unique by its type; the vector by its wrapper. Both qualify.
    const auto evens = std::set<int> { 0, 2, 4, 6, 8 };
    const auto picked = urd::SortedUniqueVector<int>::make({ 8, 3, 4, 4, 1 });

    // Linear-time merge, because both inputs are sorted and unique.
    const auto both = urd::intersect(evens, picked);
    print("intersect", both);  // 4 8

    // The result is sorted and unique again, so it feeds the next merge without a check.
    const auto small = urd::SortedUniqueVector<int>::make({ 4, 5 });
    print("intersect again", urd::intersect(both, small));  // 4

    // Union of a nonempty input is nonempty: the property follows from the inputs.
    const auto seed = urd::Refined<std::vector<int>, urd::props::sorted | urd::props::unique | urd::props::nonempty>::assume({ 7 });
    const auto all = urd::unite(picked, seed);
    static_assert(urd::includes(decltype(all)::properties, urd::props::nonempty));
    std::cout << "unite is nonempty, front: " << all.front() << '\n';  // 1

    // A plain std::vector does not compile here, sorted or not; see tests/compile_fail.
}
