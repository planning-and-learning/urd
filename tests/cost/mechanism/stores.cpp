// What urd's mechanism costs to compile on the mix of Stores a downstream library instantiates (tests/cost/mechanism.py):
// per record type, the families tyr's planning code holds (interning pools of records, slots, relations of rows, maps and
// sets keyed by indices and by index_views, sequences, nested Stores), with tyr's tags. URD_MECHANISM_RECORDS record types,
// each with its own families, held three ways:
//
//   full      the Stores as written;
//   resolved  (URD_MECHANISM_RESOLVED) the same Stores, their spec and container given (mechanism_resolved.hpp, made by
//             mechanism.py from a probe of this file): the facets and the Store class, without the selection;
//   direct    (URD_MECHANISM_DIRECT) the containers the Stores chose, named directly (mechanism_resolved.hpp).
//
// With URD_MECHANISM_PROBE it prints, as deprecation warnings, each Store's request, spec and container, which
// mechanism.py reads to write mechanism_resolved.hpp.

#include "all_containers.hpp"
#include "urd/elements/view.hpp"
#include "urd/store.hpp"

#include <boost/mp11/list.hpp>
#include <cstddef>
#include <cstdint>
#include <utility>

#if defined(URD_MECHANISM_RECORDS)
inline constexpr int records = URD_MECHANISM_RECORDS;
#else
inline constexpr int records = 1;
#endif

namespace mechanism
{

/// The record types of family I, as tyr's tags of records (urd::data<Tag> holds the fields): a ground one (an index
/// and a reference), a lifted one (with a nested Store of references), and a row of a relation.
template<int I>
struct A
{
};
template<int I>
struct B
{
};
template<int I>
struct Row
{
};

/// The context index_views of family I read through, named as tyr's repositories are (a long list of record types),
/// resolving its records' indices as a repository does. Declared only: nothing here runs.
template<int I, typename... Records>
struct Repository
{
    const urd::data<A<I>>& operator[](urd::index<A<I>> at) const;
    const urd::data<B<I>>& operator[](urd::index<B<I>> at) const;
    urd::index_view<A<I>, Repository> view(urd::index<A<I>> at) const;
    urd::index_view<B<I>, Repository> view(urd::index<B<I>> at) const;
    std::size_t layer() const;
};
template<int I>
using Context = Repository<I,
                           A<I>,
                           B<I>,
                           Row<I>,
                           A<I + 1000>,
                           A<I + 1001>,
                           A<I + 1002>,
                           A<I + 1003>,
                           A<I + 1004>,
                           A<I + 1005>,
                           A<I + 1006>,
                           A<I + 1007>,
                           A<I + 1008>,
                           A<I + 1009>,
                           A<I + 1010>,
                           A<I + 1011>,
                           A<I + 1012>,
                           A<I + 1013>,
                           A<I + 1014>,
                           A<I + 1015>,
                           A<I + 1016>,
                           A<I + 1017>,
                           A<I + 1018>,
                           A<I + 1019>,
                           B<I + 1000>,
                           B<I + 1001>,
                           B<I + 1002>,
                           B<I + 1003>,
                           B<I + 1004>,
                           B<I + 1005>,
                           B<I + 1006>,
                           B<I + 1007>,
                           B<I + 1008>,
                           B<I + 1009>,
                           B<I + 1010>,
                           B<I + 1011>,
                           B<I + 1012>,
                           B<I + 1013>,
                           B<I + 1014>,
                           B<I + 1015>,
                           B<I + 1016>,
                           B<I + 1017>,
                           B<I + 1018>,
                           B<I + 1019>>;

}  // namespace mechanism

template<int I>
struct urd::data<mechanism::A<I>>
{
    urd::index<mechanism::A<I>> index;
    urd::index<mechanism::Row<I>> row;

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::row> {}; }
    static constexpr auto identifying_members() noexcept { return urd::Members<&data::row> {}; }
};

template<int I>
struct urd::data<mechanism::B<I>>
{
    urd::index<mechanism::B<I>> index;
    urd::index<mechanism::A<I>> head;
    urd::Store<urd::sequence<urd::index<mechanism::A<I>>>> arguments;

    data() = default;
    data(const data& other) = delete;
    data& operator=(const data& other) = delete;
    data(data&& other) = default;
    data& operator=(data&& other) = default;
    ~data() = default;

    static constexpr auto cista_members() noexcept { return urd::Members<&data::index, &data::head, &data::arguments> {}; }
    static constexpr auto identifying_members() noexcept { return urd::Members<&data::head, &data::arguments> {}; }
};

namespace mechanism
{

template<typename T>
using Index = urd::index<T>;
template<typename T, int I>
using View = urd::index_view<T, Context<I>>;

/// The Stores of family I, as written: tyr's families, with tyr's tags (counted from tyr's lifted task grounder).
template<int I>
using Requests = boost::mp11::mp_list<urd::Store<urd::pool<urd::data<A<I>>, Index<A<I>>>, urd::flat, urd::stable_refs, urd::layered, urd::concurrent>,
                                      urd::Store<urd::pool<urd::data<B<I>>, Index<B<I>>>, urd::flat, urd::stable_refs, urd::layered, urd::concurrent>,
                                      urd::Store<urd::slots<urd::data<A<I>>>>,
                                      urd::Store<urd::slots<urd::data<B<I>>>>,
                                      urd::Store<urd::sequence<Index<A<I>>>>,
                                      urd::Store<urd::sequence<Index<B<I>>>>,
                                      urd::Store<urd::map<Index<A<I>>, Index<B<I>>>, urd::dense, urd::layered, urd::concurrent>,
                                      urd::Store<urd::pool<urd::Store<urd::sequence<unsigned long>, urd::frozen>, Index<Row<I>>>,
                                                 urd::monotone_until_clear,
                                                 urd::by_column_widths,
                                                 urd::layered,
                                                 urd::concurrent>,
                                      urd::Store<urd::map<View<A<I>, I>, urd::Store<urd::sequence<Index<B<I>>>>>>,
                                      urd::Store<urd::set<Index<A<I>>>, urd::dense>,
                                      urd::Store<urd::sequence<View<B<I>, I>>>,
                                      urd::Store<urd::map<Index<B<I>>, urd::Store<urd::sequence<Index<A<I>>>>>>>;

}  // namespace mechanism

#if defined(URD_MECHANISM_RESOLVED) || defined(URD_MECHANISM_DIRECT)
#include "mechanism_resolved.hpp"
#endif

namespace mechanism
{

#if defined(URD_MECHANISM_DIRECT)
/// The container a Store chose: mechanism_resolved.hpp's table.
template<typename Store>
using held = typename chosen<Store>::type;
#else
template<typename Store>
using held = Store;
#endif

#if defined(URD_MECHANISM_PROBE)
/// Each Store's request (its kind and tags), spec and container, printed by the compiler as a deprecation warning.
template<typename Store>
struct request_of;
template<typename Kind, typename... Tags>
struct request_of<urd::Store<Kind, Tags...>>
{
    using type = boost::mp11::mp_list<Kind, Tags...>;
    using parts = urd::detail::store_parts<Kind, Tags...>;
};
template<int I, int N, typename Request, typename Spec, typename Representation>
[[deprecated("urd mechanism probe")]] constexpr bool probe()
{ return true; }
template<int I, int N, typename Store>
constexpr bool show()
{ return probe<I, N, typename request_of<Store>::type, typename request_of<Store>::parts::spec, typename request_of<Store>::parts::representation>(); }
#endif

/// Every Store of family I made complete, as a class a library holds is.
template<int I, typename... Stores>
constexpr std::size_t hold(boost::mp11::mp_list<Stores...> /*stores*/)
{
#if defined(URD_MECHANISM_PROBE)
    [[maybe_unused]] constexpr bool shown = [&]<std::size_t... N>(std::index_sequence<N...>)
    { return (show<I, N, Stores>() && ...); }(std::index_sequence_for<Stores...> {});
#endif
    return (sizeof(held<Stores>) + ...);
}

template<int... I>
constexpr std::size_t hold_all(std::integer_sequence<int, I...> /*records*/)
{ return (mechanism::hold<I>(Requests<I> {}) + ...); }  // qualified: no ADL, which would complete every Store listed

}  // namespace mechanism

int main() { return static_cast<int>(mechanism::hold_all(std::make_integer_sequence<int, records> {}) % 2); }
