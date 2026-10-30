#pragma once

// Prints what a Store request resolved to, for the examples. Downstream code never needs this: it uses the
// Store and never names the representation.

#include "names.hpp"
#include "urd/store.hpp"
#include "urd/use/all.hpp"

#include <iostream>
#include <string_view>

namespace example
{

/// Prints `request` and the container urd chose for Store S.
template<typename S>
void show(std::string_view request)
{
    std::cout << "  " << request << "\n      -> " << urd::test::name<typename S::representation>::value << '\n';
}

}  // namespace example
