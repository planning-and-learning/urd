// A Store whose representation's family is not included fails at the Store and names the header to include. The
// control build includes it; each case leaves one out.
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

#include <string>

/// A record stored flat (not trivially copyable, its members named for cista): an arena interner holds it.
struct Note
{
    std::string text;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Note::text> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Note::text> {}; }
};

int main()
{
    // records: the arena interner is defined by urd/use/records.hpp
    auto notes = urd::Store<urd::value<Note>, urd::handles, urd::unique, urd::flat>();
    (void) notes.size();
    // dense: the bitset is defined by urd/use/dense.hpp (urd/use/sequences.hpp brings the definition in through the
    // rows, but not the marker: the check reads the marker)
    auto bits = urd::Store<urd::key<int>, urd::dense>();
    (void) bits.size();
    // sequences: std::vector is the standard's, its marker urd/use/sequences.hpp's
    auto items = urd::Store<urd::value<int>>();
    (void) items.size();
}
