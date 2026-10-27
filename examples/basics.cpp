// How a property is established, kept, and given up.

#include "urd/refined.hpp"

#include <iostream>
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
    // Established by doing the work: sorted, then duplicates removed.
    auto ids = urd::SortedUniqueVector<int>::make({ 5, 3, 9, 3, 1 });
    print("make", ids);  // 1 3 5 9

    // Kept by every change: insert goes to the sorted position, a duplicate is refused.
    ids.insert(4);
    const bool inserted = ids.insert(5);
    print("after insert(4), insert(5)", ids);  // 1 3 4 5 9
    std::cout << "insert(5) inserted: " << std::boolalpha << inserted << '\n';

    // Established by a claim, which Debug builds check (see tests/assume_fails.cpp).
    const auto nonempty = urd::Refined<std::vector<int>, urd::props::sorted | urd::props::nonempty>::assume({ 2, 7, 7 });
    std::cout << "front of a nonempty range, no check needed: " << nonempty.front() << '\n';

    // Dropping a property is implicit: a nonempty sorted range is a sorted range.
    const urd::SortedVector<int> sorted = nonempty;
    print("as SortedVector", sorted);

    // A view keeps the properties of what it views.
    const auto view = ids.view();
    static_assert(urd::includes(decltype(view)::properties, urd::props::sorted | urd::props::unique));
    std::cout << "view size: " << view.size() << '\n';

    // Giving up every property is explicit and visible.
    std::vector<int> plain = std::move(ids).release();
    plain.push_back(0);  // now anything goes
    print("released, then push_back(0)", plain);
}
