// What a consumer of the package needs: the urd::urd target, and with it Boost, cista and, where configured, gtl.
#include <urd/store.hpp>

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
