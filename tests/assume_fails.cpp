// A false claim to assume() stops a Debug build. CTest passes this program when assert's message
// names the claim; the abort becomes a plain failing exit, which CTest can judge.

#include "urd/refined.hpp"

#include <csignal>
#include <cstdlib>
#include <vector>

int main()
{
    std::signal(SIGABRT, [](int) { std::_Exit(EXIT_FAILURE); });
    [[maybe_unused]] const auto wrong = urd::SortedVector<int>::assume(std::vector<int> { 3, 1, 2 });
}
