// Code that must not compile. Each case is built twice: without URD_CASE it must compile, so the
// surrounding code is valid; with URD_CASE=<n> it must fail, so case n is what the types reject.

#include "urd/algorithms.hpp"

#include <vector>

int main()
{
    [[maybe_unused]] auto plain = std::vector<int> { 1, 2, 3 };
    [[maybe_unused]] auto sorted = urd::SortedVector<int>::make({ 3, 1, 2 });
    [[maybe_unused]] auto set = urd::SortedUniqueVector<int>::make({ 3, 1, 2 });
    [[maybe_unused]] auto nonempty = urd::Refined<std::vector<int>, urd::props::nonempty>::assume({ 1 });

#if URD_CASE == 1  // a plain vector has no properties, even when its contents are sorted
    (void)urd::intersect(plain, set);
#elif URD_CASE == 2  // sorted is not enough: intersect needs unique too
    (void)urd::intersect(sorted, set);
#elif URD_CASE == 3  // gaining a property is never implicit
    const urd::SortedUniqueVector<int> gained = sorted;
#elif URD_CASE == 4  // only a nonempty range has a front without a check
    (void)sorted.front();
#elif URD_CASE == 5  // erasing could empty a nonempty range
    (void)nonempty.erase(1);
#elif URD_CASE == 6  // a view is read-only
    (void)set.view().insert(4);
#elif URD_CASE == 7  // the elements are read-only: no write access to the container
    set.get().push_back(0);
#elif URD_CASE == 8  // a nonempty range cannot be default-constructed empty
    const auto empty = urd::Refined<std::vector<int>, urd::props::nonempty>();
#endif
}
