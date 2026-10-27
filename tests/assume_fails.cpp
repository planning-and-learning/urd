// A false claim to assume() stops a Debug build. CTest passes this program when assert's message
// names the claim; the abort becomes a plain failing exit, which CTest can judge.

#include "urd/store.hpp"

#include <csignal>
#include <cstdlib>

int main()
{
    if (std::signal(SIGABRT, [](int) { std::_Exit(EXIT_FAILURE); }) == SIG_ERR)
        return EXIT_FAILURE;
    try
    {
        [[maybe_unused]] const auto wrong = urd::Store<urd::value<int>, urd::sorted>::assume({ 3, 1, 2 });
    }
    catch (...)
    {
        return EXIT_FAILURE;  // not the failure this program is for
    }
}
