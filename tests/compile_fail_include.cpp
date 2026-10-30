// A Store whose representation's family is not included fails at the Store and names the header to include. The
// control build includes every family and holds a Store of each; a case leaves a header out and holds the Store
// that needs it.
#include <urd/store.hpp>
#if URD_CASE != 1
#include <urd/use/records.hpp>
#endif
#if URD_CASE != 2
#include <urd/use/dense.hpp>
#endif
#if URD_CASE != 3
#include <urd/use/sequences.hpp>
#endif
#if URD_CASE != 4 && URD_CASE != 10 && URD_CASE != 12
#include <urd/use/hash.hpp>
#endif
#if URD_CASE != 5
#include <urd/use/contiguous.hpp>
#endif
#if URD_CASE != 6 && defined(URD_HAS_GTL)
#include <urd/use/parallel.hpp>
#endif
#if URD_CASE != 7 && URD_CASE != 13
#include <urd/use/nodes.hpp>
#endif
#if URD_CASE != 8
#include <urd/use/queues.hpp>
#endif
#if URD_CASE != 9
#include <urd/use/pools.hpp>
#endif
#if URD_CASE != 10
#include <urd/use/layers.hpp>
#endif
#if URD_CASE != 11
#include <urd/use/scopes.hpp>
#endif

#include <string>

/// A record stored flat (not trivially copyable, its members named for cista): an arena interner holds it.
struct Note
{
    std::string text;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Note::text> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Note::text> {}; }
};

#if URD_CASE == 13
// The header included after a Store was instantiated: the check fired already, and the error names the header all
// the same (the marker is read once per file, so a family header goes before the first Store that needs it).
inline auto early = urd::Store<urd::key<int>, urd::sorted, urd::reclaims>();
#include <urd/use/nodes.hpp>
#endif

#define URD_HOLDS(n) (!defined(URD_CASE) || URD_CASE == (n))

int main()
{
#if URD_HOLDS(1)  // records: the arena interner is defined by urd/use/records.hpp
    auto notes = urd::Store<urd::value<Note>, urd::handles, urd::unique, urd::flat>();
    (void) notes.size();
#endif
#if URD_HOLDS(2)  // dense: the bitset is defined by urd/use/dense.hpp (the rows' header brings the definition, not the marker)
    auto bits = urd::Store<urd::key<int>, urd::dense>();
    (void) bits.size();
#endif
#if URD_HOLDS(3)  // sequences: std::vector is the standard's, its marker urd/use/sequences.hpp's
    auto items = urd::Store<urd::value<int>>();
    (void) items.size();
#endif
#if URD_HOLDS(4)  // hash: Boost's flat hash set, declared through Boost's forward header, defined by urd/use/hash.hpp
    auto keys = urd::Store<urd::key<int>>();
    (void) keys.size();
#endif
#if URD_HOLDS(5)  // contiguous: Boost's flat set likewise, by urd/use/contiguous.hpp
    auto sorted = urd::Store<urd::key<int>, urd::sorted, urd::contiguous>();
    (void) sorted.size();
#endif
#if URD_HOLDS(6) && defined(URD_HAS_GTL)  // parallel: gtl's sharded table, by urd/use/parallel.hpp
    auto shared = urd::Store<urd::key<int>, urd::concurrent>();
    (void) shared.size();
#endif
#if URD_HOLDS(7)  // nodes: std::set is the standard's, its marker urd/use/nodes.hpp's
    auto tree = urd::Store<urd::key<int>, urd::sorted, urd::reclaims>();
    (void) tree.size();
#endif
#if URD_HOLDS(8)  // queues: the deque queue, by urd/use/queues.hpp
    auto fifo = urd::Store<urd::value<int>, urd::fifo>();
    (void) fifo.size();
#endif
#if URD_HOLDS(9)  // pools: the row interner, by urd/use/pools.hpp (its rows' family, sequences, is included)
    using Row = urd::Store<urd::value<int>, urd::frozen>;
    auto rows = urd::Store<urd::value<Row>, urd::handles, urd::unique>();
    (void) rows;
#endif
#if URD_HOLDS(10)  // a layered map without hash and layers: one compile names both headers, the part first
    auto layered = urd::Store<urd::key<int>, urd::value<int>, urd::layered>();
    (void) layered.size();
#endif
#if URD_HOLDS(11)  // a scoped map without scopes: the wrapper's header
    auto scoped = urd::Store<urd::key<int>, urd::value<int>, urd::scoped>();
    (void) scoped.size();
#endif
#if URD_HOLDS(12)  // a dense map of hash sets without hash: the inner Store's family is needed where the outer is held
    using Inner = urd::Store<urd::key<int>>;
    auto nested = urd::Store<urd::key<int>, urd::value<Inner>, urd::dense>();
    (void) nested.size();
#endif
#if URD_CASE == 13
    (void) early.size();
#endif
}
