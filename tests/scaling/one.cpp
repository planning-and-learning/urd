// One Store: it adds an instantiation of its own container and of no other registered one (tests/scaling.py).

#include "urd/repr/change_logged.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/unordered_flat_map.hpp"
#include "urd/repr/unordered_flat_set.hpp"
#include "urd/store.hpp"

int main()
{
#if !defined(URD_SCALING_BASELINE)  // the same file without its Store: what the headers instantiate themselves
    auto set = urd::Store<urd::key<int>>();
    set.insert(1);
    return set.contains(1) ? 0 : 1;
#else
    return 0;
#endif
}
