// Resolving a request completes nothing: not the candidates, and not a Store named as the request's element. The
// inner Store here is a pool of records stored flat, whose representation (the arena interner) is declared but
// not defined in this file (urd/flat.hpp is not included). Were the inner Store completed while the outer is
// resolved, its representation's completeness check would fire. The chosen representation is read off the resolved
// type by pattern, never through the outer Store's members. It compiles or it does not: there is nothing to run.
#include "urd/store.hpp"

#include <concepts>
#include <string>
#include <vector>

namespace
{

using urd::key;
using urd::Store;
using urd::value;

/// A record stored flat: not trivially copyable (the text), its members named for cista.
struct Note
{
    std::string text;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Note::text> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Note::text> {}; }
};
using Inner = Store<value<Note>, urd::handles, urd::unique, urd::flat>;

template<typename T>
struct representation_of;
template<typename R, typename S>
struct representation_of<urd::detail::Store<R, S>>
{
    using type = R;
};
/// What a request resolves to, by its name alone.
template<typename... Ps>
using chosen = typename representation_of<typename urd::detail::resolve<Ps...>::type>::type;

static_assert(std::same_as<chosen<value<Note>, urd::handles, urd::unique, urd::flat>, urd::repr::ArenaInterner<Note>>);
static_assert(std::same_as<chosen<key<int>, value<Inner>>, boost::unordered_flat_map<int, Inner, urd::Hash<int>, urd::EqualTo<int>>>);
static_assert(std::same_as<chosen<key<int>, value<Inner>, urd::dense>, urd::repr::IndexMap<int, Inner, false>>);
static_assert(std::same_as<chosen<key<int>, value<Inner>, urd::dense, urd::constant_clear>, urd::repr::IndexMap<int, Inner, true>>);
static_assert(std::same_as<chosen<key<int>, value<Inner>, urd::scoped>, urd::repr::ScopedMap<int, Inner>>);
static_assert(std::same_as<chosen<key<int>, value<Inner>, urd::layered>, urd::repr::Layered<boost::unordered_flat_map<int, Inner, urd::Hash<int>, urd::EqualTo<int>>>>);
static_assert(std::same_as<chosen<key<int>, value<Inner>, urd::sorted>, std::map<int, Inner, urd::Less<int>>>);
static_assert(std::same_as<chosen<value<Inner>>, std::vector<Inner>>);
static_assert(std::same_as<chosen<value<Inner>, urd::stable_refs, urd::monotone>, urd::repr::SegmentedVector<Inner>>);
// A pool of rows, the rows a Store: resolved by the rows' request, which says what they iterate.
using Row = Store<value<int>, urd::frozen>;
static_assert(std::same_as<chosen<value<Row>, urd::handles, urd::unique>, urd::repr::RowInterner<Row>>);
static_assert(std::same_as<chosen<value<Row>, urd::handles, urd::unique, urd::structural_sharing>, urd::repr::TreeVectorSet<Row>>);

}  // namespace
