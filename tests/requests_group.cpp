// One group of the request grid (URD_GROUP, tests/grid.hpp) for benchmarks/requests.json (tests/requests.hpp): which
// containers serve each request, from their traits alone, so no container header is included. Compiled alone (by a
// linter, without CMake's definition), the first group.

#include "complexity.hpp"
#include "grid.hpp"
#include "requests.hpp"
#include "urd/registry.hpp"
#include "urd/registry/candidates.hpp"

#include <cstddef>
#include <stdexcept>
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

/// The containers serving S, in the registry's order, each with the headers a Store of it for S needs.
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
            auto headers = std::vector<std::string>();
            urd::test::headers_of<R>(headers);
            if constexpr (!std::is_void_v<typename S::key_type>)
                urd::test::element_headers_of<typename S::key_type>(headers);
            if constexpr (!std::is_void_v<typename S::value_type>)
                urd::test::element_headers_of<typename S::value_type>(headers);
            auto part = std::string(urd::test::part_name<R>());
            auto uninvited = std::vector<std::string>();
            for (const auto each : urd::test::costly)
                if (!S::urd_properties.has(each) && urd::container_traits<R>::urd_properties.has(each))
                    uninvited.emplace_back(urd::test::tag_name(each));
            into.push_back(
                { std::string(urd::test::name<R>::id), std::move(headers), std::move(part), urd::test::words_of<R>().value_or(0), std::move(uninvited) });
        }
    }
    static std::vector<urd::test::server> list()
    {
        auto result = std::vector<urd::test::server>();
        (one<Rs>(result), ...);
        return result;
    }
};

/// Whether the scan over the rows (choose<S>, urd/registry/select.hpp) picks the container the walk over every
/// registered type picks (the same id, or none for both), with the headers its declarations name (urd::defined_in,
/// urd::parts). That the id names that type is checked where the container's header is included: the per-container
/// tests resolve through it (tests/container.cpp).
template<typename S, typename List>
bool rows_agree()
{
    using walked = urd::detail::choose_in<S, List>;
    constexpr auto row = urd::detail::choose<S>::row;
    if constexpr (!walked::found)
        return row == nullptr;
    else
    {
        if (row == nullptr || row->id != urd::test::name<typename walked::type>::id)
            return false;
        auto declared = std::vector<std::string>();
        urd::test::headers_of<typename walked::type>(declared);
        auto listed = std::vector<std::string>();
        for (const auto header : row->headers)
            if (!header.empty())
                listed.emplace_back(header);
        return listed == declared;
    }
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
            if (!rows_agree<S, List>())
                throw std::runtime_error("the rows choose another container, or other headers, than the declarations for " + Q::describe());
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
