// One group of the request grid (URD_GROUP, tests/support/grid.hpp) held to the facets' probes (tests/laws/surface.hpp): for every
// request a container serves, each facet of its kind is on exactly where its probe holds. Checked at compile time; the
// first group where compiled alone (by a linter, without CMake's definition).

#include "all_containers.hpp"  // first: a Store's container is resolved where the Store is first named
#include "grid.hpp"
#include "surface.hpp"
#include "urd/registry/select.hpp"
#include "urd/store.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace
{

#if defined(URD_GROUP)
constexpr std::size_t this_group = URD_GROUP;
#else
constexpr std::size_t this_group = 0;
#endif

/// The Store request Q is written as (Store<Ps...>).
template<typename... Ps>
urd::Store<Ps...> store_of(const urd::request<Ps...>* /*request*/);
template<typename Q>
using store_t = decltype(store_of(static_cast<const Q*>(nullptr)));

/// Facet F of Store T (over R, for S) agrees with its probe (urd::test::agrees). A class, so that a failure names F,
/// T and S in the instantiation the compiler reports.
template<typename T, typename R, typename S, template<typename, typename, typename> class F>
struct agreement
{
    static_assert(urd::test::agrees<T, R, S, F>(), "facet F's member is callable on Store T where F is off, or not where it is on");
    static constexpr bool value = true;
};

/// Every facet of a list agrees. A class, not a consteval function: GCC 13 checks a probe's requires-expression
/// differently while evaluating an immediate function.
template<typename T, typename R, typename S, typename List>
struct all_agree;
template<typename T, typename R, typename S, template<typename, typename, typename> class... Fs>
struct all_agree<T, R, S, urd::detail::facet_list<Fs...>>
{
    static constexpr bool value = (agreement<T, R, S, Fs>::value && ...);
};

/// Request Q's Store, where a container serves Q, holds to the probes of its kind's facets.
template<typename Q>
void check()
{
    using S = typename Q::spec;
    if constexpr (urd::detail::choose<S>::found)
    {
        using T = store_t<Q>;
        static_assert(all_agree<T, typename T::representation, S, urd::detail::facets_of<typename S::kind>>::value);
    }
}

template<typename... Qs>
void check_all(urd::requests<Qs...> /*requests*/)
{ (check<Qs>(), ...); }

/// Compiled for the static assertions it instantiates; nothing calls it.
[[maybe_unused]] void check_group()
{
    // Qualified: lookup by argument would complete every Store the requests name.
    ::check_all(typename std::tuple_element_t<this_group, urd::test::groups>::list {});
}

}  // namespace
