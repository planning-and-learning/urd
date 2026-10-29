// Built only where URD_WITH_GTL is off: a concurrent set or map by hashed keys is gtl's sharded table, so without gtl
// the request has no representation. The control build (no case) must compile.
#include <urd/store.hpp>

#if URD_CASE == 1  // a concurrent set by hashed keys needs gtl
using Shared = urd::Store<urd::key<int>, urd::concurrent>;
Shared shared;
#elif URD_CASE == 2  // a concurrent map by an integer key too: a dense slot per index is no stand-in, its keys were not promised
using Counts = urd::Store<urd::key<int>, urd::value<int>, urd::concurrent>;
Counts counts;
#endif

int main() {}
