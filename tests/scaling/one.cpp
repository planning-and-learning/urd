// One Store: what a file holding it costs the compiler must not grow with the number of containers registered
// (tests/scaling.py compiles it with the registry as it is and grown, URD_REGISTRY_COPIES, copies.hpp).

#include "copies.hpp"  // first: it extends the registry before anything builds it
#include "urd/repr/change_logged.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/unordered_flat_map.hpp"
#include "urd/repr/unordered_flat_set.hpp"
#include "urd/store.hpp"

int main()
{
    auto set = urd::Store<urd::key<int>>();
    set.insert(1);
    return set.contains(1) ? 0 : 1;
}
