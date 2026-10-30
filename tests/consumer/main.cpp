// What a consumer of the package needs: the urd::urd target, and with it Boost, cista and, where configured, gtl;
// and for each Store, the header of its family (docs/RESOLUTION.md names it, as does the error of a Store without it).
#include <urd/store.hpp>
#include <urd/use/hash.hpp>
#if defined(URD_HAS_GTL)
#include <urd/use/parallel.hpp>
#endif

int main()
{
    auto ids = urd::Store<urd::key<int>>();
    ids.insert(3);
#if defined(URD_HAS_GTL)
    auto shared = urd::Store<urd::key<int>, urd::concurrent>();
    shared.insert(3);
    if (!shared.contains(3))
        return 1;
#endif
    return ids.contains(3) ? 0 : 1;
}
