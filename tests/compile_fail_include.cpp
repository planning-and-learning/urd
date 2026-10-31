// A Store whose container's header is not included fails at the Store, with one error naming exactly the headers
// missing: the container's own and, for a wrapper, its part's, the part first. The control build includes every
// header and holds a Store of each; a case leaves headers out and holds the Store that needs them. (std::set and
// std::vector are always declared through store.hpp today, so they have no case until urd declares its own.)
#include <urd/store.hpp>
#if URD_CASE != 1
#include <urd/repr/arena.hpp>
#endif
#if URD_CASE != 2
#include <urd/repr/dense_set.hpp>
#endif
#if URD_CASE != 3 && URD_CASE != 8 && URD_CASE != 11 && URD_CASE != 12 && URD_CASE != 13
#include <urd/repr/unordered_flat_set.hpp>
#endif
#if URD_CASE != 4
#include <urd/repr/flat_set.hpp>
#endif
#if URD_CASE != 5
#include <urd/repr/parallel_hash.hpp>
#endif
#if URD_CASE != 6
#include <urd/repr/queue.hpp>
#endif
#if URD_CASE != 7
#include <urd/repr/pools.hpp>
#endif
#if URD_CASE != 8
#include <urd/repr/unordered_flat_map.hpp>
#endif
#if URD_CASE != 8 && URD_CASE != 9
#include <urd/repr/layer.hpp>
#endif
#if URD_CASE != 10
#include <urd/repr/scoped.hpp>
#endif
#include <string>
#include <urd/repr/index_map.hpp>
#include <urd/repr/sequence.hpp>

namespace
{

/// A record stored flat (not trivially copyable, its members named for cista): an arena interner holds it.
struct Note
{
    std::string text;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Note::text> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Note::text> {}; }
};

}  // namespace

#if URD_CASE == 12
// The header included after a Store was instantiated: the check fired already, and the error names the header all
// the same (the marker is read once per file, so a container's header goes before the first Store that needs it).
inline auto early = urd::Store<urd::key<int>>();
#include <urd/repr/unordered_flat_set.hpp>
#endif

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): the condition of each case, in #if lines, where no function can go
#define URD_HOLDS(n) (!defined(URD_CASE) || URD_CASE == (n))

int main()
{
#if URD_HOLDS(1)  // the arena interner, without urd/repr/arena.hpp
    auto notes = urd::Store<urd::value<Note>, urd::handles, urd::unique, urd::flat>();
    (void) notes.size();
#endif
#if URD_HOLDS(2)  // the bitset, without urd/repr/dense_set.hpp
    auto bits = urd::Store<urd::key<int>, urd::dense>();
    (void) bits.size();
#endif
#if URD_HOLDS(3)  // Boost's flat hash set, declared through Boost's forward header, without its definition
    auto keys = urd::Store<urd::key<int>>();
    (void) keys.size();
#endif
#if URD_HOLDS(4)  // Boost's flat set likewise
    auto sorted = urd::Store<urd::key<int>, urd::sorted, urd::contiguous>();
    (void) sorted.size();
#endif
#if URD_HOLDS(5)  // gtl's sharded table
    auto shared = urd::Store<urd::key<int>, urd::concurrent>();
    (void) shared.size();
#endif
#if URD_HOLDS(6)  // the queue
    auto fifo = urd::Store<urd::value<int>, urd::fifo>();
    (void) fifo.size();
#endif
#if URD_HOLDS(7)  // the row interner (its rows are vectors, always declared)
    using Row = urd::Store<urd::value<int>, urd::frozen>;
    auto rows = urd::Store<urd::value<Row>, urd::handles, urd::unique>();
    (void) rows;
#endif
#if URD_HOLDS(8)  // a layered map missing its part's header and its own: one error names both, the part first
    auto layered = urd::Store<urd::key<int>, urd::value<int>, urd::layered>();
    (void) layered.size();
#endif
#if URD_HOLDS(9)  // a layered map whose part is included: only the wrapper's header is named
    auto layered_only = urd::Store<urd::key<int>, urd::value<int>, urd::layered>();
    (void) layered_only.size();
#endif
#if URD_HOLDS(10)  // a scoped map: the wrapper's header
    auto scoped = urd::Store<urd::key<int>, urd::value<int>, urd::scoped>();
    (void) scoped.size();
#endif
#if URD_HOLDS(11)  // a dense map of hash sets: the inner Store's header is needed where the outer is held
    using Inner = urd::Store<urd::key<int>>;
    auto nested = urd::Store<urd::key<int>, urd::value<Inner>, urd::dense>();
    (void) nested.size();
#endif
#if URD_CASE == 12
    (void) early.size();
#endif
#if URD_HOLDS(13)  // two Stores, one container's header included: only the other's is named
    auto included_bits = urd::Store<urd::key<int>, urd::dense>();
    auto missing_keys = urd::Store<urd::key<int>>();
    (void) included_bits.size();
    (void) missing_keys.size();
#endif
}
