// One group of the request grid (URD_GROUP, tests/grid.hpp) for benchmarks/requests.json (tests/requests.hpp): which
// containers serve each request, from their traits alone, so no family header is included. Compiled alone (by a
// linter, without CMake's definition), the first group.

#include "complexity.hpp"
#include "grid.hpp"
#include "requests.hpp"
#include "urd/registry.hpp"
#include "urd/registry/features.hpp"

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

/// Whether elements of type T are trivially copyable (none, void, counts as such). A Store owns its storage, so it is
/// not, which is decided before asking the trait: asking it would complete the Store.
template<typename T>
consteval bool is_trivial_element()
{
    if constexpr (std::is_void_v<T>)
        return true;
    else if constexpr (urd::is_store<T>)
        return false;
    else
        return std::is_trivially_copyable_v<T>;
}
template<typename T>
constexpr bool trivially = is_trivial_element<T>();

#if defined(URD_GROUP)
constexpr std::size_t this_group = URD_GROUP;
#else
constexpr std::size_t this_group = 0;
#endif

template<typename R, typename S>
struct serving
{
    static constexpr bool value = urd::detail::serves<R, S>;
};

/// The containers serving S, in the registry's order, each with the families a Store of it for S needs.
template<typename S, typename List>
struct servers;
template<typename S, typename... Rs>
struct servers<S, urd::detail::type_list<Rs...>>
{
    template<typename R>
    static void one(std::vector<urd::test::server>& into)
    {
        if constexpr (serving<R, S>::value)
        {
            auto families = std::vector<std::string>();
            urd::test::families_of<R>(families);
            if constexpr (!std::is_void_v<typename S::key_type>)
                urd::test::element_families_of<typename S::key_type>(families);
            if constexpr (!std::is_void_v<typename S::value_type>)
                urd::test::element_families_of<typename S::value_type>(families);
            auto part = std::string(urd::test::part_name<R>());
            auto uninvited = std::vector<std::string>();
            for (const auto each : urd::test::costly)
                if (!S::urd_properties.has(each) && urd::container_traits<R>::urd_properties.has(each))
                    uninvited.emplace_back(urd::test::tag_name(each));
            into.push_back(
                { std::string(urd::test::name<R>::id), std::move(families), std::move(part), urd::test::words_of<R>().value_or(0), std::move(uninvited) });
        }
    }
    static std::vector<urd::test::server> list()
    {
        auto result = std::vector<urd::test::server>();
        (one<Rs>(result), ...);
        return result;
    }
};

/// Whether choosing among the rows as values (urd/registry/rows.hpp) picks the container choose<S> picks, by id ("" for
/// none): the two selections agree before one replaces the other (claude.layout.md, step 2).
template<typename S, typename List>
consteval bool rows_agree()
{
    using chosen = urd::detail::choose_in<S, List>;
    if constexpr (chosen::found)
        return urd::detail::row_choice<S>() == urd::test::name<typename chosen::type>::id;
    else
        return urd::detail::row_choice<S>().empty();
}

}  // namespace

template<>
urd::test::request_group urd::test::requests_of<this_group>()
{
    auto result = request_group {};
    std::size_t position = 0;
    urd::test::for_each_request(
        typename std::tuple_element_t<this_group, groups>::list {},  // qualified: lookup by argument would complete every Store named in a request
        [&]<typename Q>(std::type_identity<Q>)
        {
            using S = typename Q::spec;
            using List = typename urd::detail::ordered_for<S, typename urd::detail::candidates_for<S>::type>::type;
            static_assert(rows_agree<S, List>(), "the rows choose another container than choose<S> (the request is in the instantiation above)");
            result.rows.push_back({ Q::describe(),
                                    shape_of_spec<S>(),
                                    urd::has<S, urd::small>,
                                    offered<S>(),
                                    servers<S, List>::list(),
                                    position,
                                    sizes_of<S>(),
                                    trivially<typename S::key_type> && trivially<typename S::value_type> });
            ++position;
        });
    return result;
}
