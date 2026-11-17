// One group of the request grid (URD_GROUP, tests/support/grid.hpp) held to the facets' probes (tests/laws/surface.hpp): for every
// request a container serves, each facet of its kind is on exactly where its probe holds. Checked at compile time; the
// first group where compiled alone (by a linter, without CMake's definition).

#include "all_containers.hpp"  // first: a Store's container is resolved where the Store is first named
#include "grid.hpp"
#include "surface.hpp"
#include "urd/registry/select.hpp"
#include "urd/store.hpp"

#include <cstddef>
#include <cstdint>
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
    static_assert(urd::test::takes_by_the_rule<T, R, S, F>(),
                  "facet F of Store T takes elements against the rule: a moving call refused, or a copying call accepted for a "
                  "move-only element or refused for a copyable one");
    static constexpr bool value = true;
};

/// Facet F's moving calls compiled, not only resolved: a representation that would copy what is moved in fails here.
template<typename T, typename R, typename S, template<typename, typename, typename> class F>
void compile_moves()
{
    if constexpr (urd::test::takes<F>::any)
        if constexpr (urd::detail::facet_condition<F>::template on<R, S>() && urd::test::takes<F>::template offered<T, R, S>) [[maybe_unused]]
            constexpr auto move = &urd::test::takes<F>::template move<F<T, R, S>, S>;
}

template<typename T, typename R, typename S, template<typename, typename, typename> class... Fs>
void compile_all_moves(urd::detail::facet_list<Fs...> /*facets*/)
{ (compile_moves<T, R, S, Fs>(), ...); }

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
        compile_all_moves<T, typename T::representation, S>(urd::detail::facets_of<typename S::kind> {});
    }
}

template<typename... Qs>
void check_all(urd::requests<Qs...> /*requests*/)
{ (check<Qs>(), ...); }

#if defined(URD_MOVES)
/// A key that is moved, never copied, as the libraries' records (urd::data<T>) are: hashable, ordered and stored flat by
/// its one member.
struct Moved
{
    std::uint32_t id = 0;

    Moved() = default;
    Moved(const Moved&) = delete;
    Moved& operator=(const Moved&) = delete;
    Moved(Moved&&) = default;
    Moved& operator=(Moved&&) = default;
    ~Moved() = default;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Moved::id> {}; }

    static constexpr auto cista_members() noexcept { return urd::Members<&Moved::id> {}; }
};

/// The grid's maps (urd::test::maps_of) over a key E whose containers hold a move-only key. Not the others: gtl's flat
/// hash maps (the default, small, monotone, retains, concurrent) relocate an entry as its pair<const K, V> (a run-time
/// branch over the pair's mutable form, so the copy is compiled for any key); a scoped map logs a copy of each key it
/// adds; an insertion-ordered map hands out its entries as pairs holding a copy of the key.
template<typename E>
using maps_taking = urd::requests<urd::test::request<urd::map<E, E>, urd::sorted>,
                                  urd::test::request<urd::map<E, E>, urd::sorted, urd::frozen>,
                                  urd::test::request<urd::map<E, E>, urd::multi>,
                                  urd::test::request<urd::map<E, E>, urd::multi, urd::stable_refs>,
                                  urd::test::request<urd::map<E, E>, urd::multi, urd::sorted>,
                                  urd::test::request<urd::map<E, E>, urd::multi, urd::frozen>,
                                  urd::test::request<urd::map<E, E>, urd::stable_refs>,
                                  urd::test::request<urd::map<E, E>, urd::reclaims>,
                                  urd::test::request<urd::map<E, E>, urd::stable_refs, urd::concurrent>,
                                  urd::test::request<urd::map<E, E>, urd::sorted, urd::monotone>,
                                  urd::test::request<urd::map<E, urd::Store<urd::sequence<E>, urd::retains>>>,
                                  urd::test::request<urd::map<E, urd::Store<urd::sequence<E>, urd::retains>>, urd::sorted>>;

/// The grid's shapes of requests that take elements whole, over element type E: each moving call accepted, each copying
/// one accepted for a copyable E (urd::test::Id), refused for a move-only one (Moved).
template<typename E>
using taking = urd::cat_t<urd::test::sets_of<E>,
                          maps_taking<E>,
                          urd::test::queues_of<E>,
                          urd::requests<urd::test::request<urd::sequence<E>>,
                                        urd::test::request<urd::sequence<E>, urd::small>,
                                        urd::test::request<urd::sequence<E>, urd::retains>,
                                        urd::test::request<urd::sequence<E>, urd::reclaims>,
                                        urd::test::request<urd::sequence<E>, urd::contiguous>,
                                        urd::test::request<urd::sequence<E>, urd::monotone>,
                                        urd::test::request<urd::sequence<E>, urd::stable_refs>,
                                        urd::test::request<urd::sequence<E>, urd::concurrent>,
                                        urd::test::request<urd::sequence<E>, urd::frozen>,
                                        urd::test::request<urd::multiset<E>, urd::sorted>,
                                        urd::test::request<urd::slots<E>>,
                                        urd::test::request<urd::slots<E>, urd::stable_refs>,
                                        urd::test::request<urd::slots<E>, urd::concurrent>,
                                        urd::test::request<urd::pool<E>>,
                                        urd::test::request<urd::pool<E>, urd::stable_refs>,
                                        urd::test::request<urd::pool<E>, urd::flat>,
                                        urd::test::request<urd::pool<E>, urd::flat, urd::monotone>,
                                        urd::test::request<urd::pool<E>, urd::flat, urd::stable_refs>,
                                        urd::test::request<urd::pool<E>, urd::flat, urd::concurrent>,
                                        urd::test::request<urd::pool<E>, urd::flat, urd::monotone, urd::layered>>>;
#endif

/// Compiled for the static assertions it instantiates; nothing calls it.
[[maybe_unused]] void check_group()
{
    // Qualified: lookup by argument would complete every Store the requests name.
#if defined(URD_MOVES)
    ::check_all(taking<Moved> {});
    ::check_all(taking<urd::test::Id> {});
#else
    ::check_all(typename std::tuple_element_t<this_group, urd::test::groups>::list {});
#endif
}

}  // namespace
