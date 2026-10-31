// What a consumer of the package needs: the urd::urd target, and with it Boost, cista and, where configured, gtl;
// and for each Store, the header of the container it resolves to (docs/RESOLUTION.md names it, as does the error of a
// Store without it).
#include <urd/repr/parallel_hash.hpp>
#include <urd/repr/unordered_flat_set.hpp>
#include <urd/store.hpp>

int main()
{
    auto ids = urd::Store<urd::key<int>>();
    ids.insert(3);
    auto shared = urd::Store<urd::key<int>, urd::concurrent>();
    shared.insert(3);
    if (!shared.contains(3))
        return 1;
    return ids.contains(3) ? 0 : 1;
}
