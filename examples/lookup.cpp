// contains() picks its lookup from what the type guarantees, not from a guess.

#include "urd/algorithms.hpp"

#include <iostream>
#include <set>
#include <unordered_set>
#include <vector>

namespace
{

const char* name(urd::lookup strategy)
{
    switch (strategy)
    {
        case urd::lookup::member:
            return "the container's own lookup";
        case urd::lookup::binary_search:
            return "binary search, O(log n)";
        case urd::lookup::linear_scan:
            return "linear scan, O(n)";
    }
    return "";
}

template<typename R>
void report(const char* label, const R& range, int value)
{
    std::cout << label << ": contains(" << value << ") = " << std::boolalpha << urd::contains(range, value) << " by "
              << name(urd::lookup_of<R>()) << '\n';
}

}  // namespace

int main()
{
    const auto values = std::vector<int> { 1, 3, 5, 7 };

    // Sorted by content, but the type does not say so: a linear scan is all that is safe.
    report("std::vector", values, 5);

    // The same contents, with the property in the type: binary search.
    report("urd::SortedVector", urd::SortedVector<int>::assume(values), 5);

    report("std::set", std::set<int>(values.begin(), values.end()), 5);
    report("std::unordered_set", std::unordered_set<int>(values.begin(), values.end()), 5);
}
