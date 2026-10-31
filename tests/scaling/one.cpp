// One Store: what a file holding it costs the compiler must not grow with the number of containers registered
// (tests/scaling.py compiles it with the registry as it is and grown, URD_REGISTRY_COPIES).

#include "urd/store.hpp"
#include "urd/use/hash.hpp"

int main()
{
    auto set = urd::Store<urd::key<int>>();
    set.insert(1);
    return set.contains(1) ? 0 : 1;
}
