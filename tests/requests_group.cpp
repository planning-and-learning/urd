// One group of the request grid (URD_GROUP, tests/grid.hpp) for benchmarks/requests.json (tests/requests.hpp): which
// containers serve each request, from their declarations alone, so no container header is included. Compiled alone (by a
// linter, without CMake's definition), the first group.

#include "complexity.hpp"
#include "grid.hpp"
#include "requests.hpp"
#include "urd/registry/select.hpp"

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

/// The containers serving S, in the order they are preferred, each with the headers a Store of it for S needs:
/// read from their declarations (urd/registry/declaration.hpp), so no container is named.
template<typename S>
std::vector<urd::test::server> servers()
{
    auto result = std::vector<urd::test::server>();
    urd::test::for_each_serving<S>(
        [&]<typename Decl>(std::type_identity<Decl>)
        {
            auto headers = std::vector<std::string>(Decl::headers.begin(), Decl::headers.end());
            if constexpr (!std::is_void_v<typename S::key_type>)
                urd::test::element_headers_of<typename S::key_type>(headers);
            if constexpr (!std::is_void_v<typename S::value_type>)
                urd::test::element_headers_of<typename S::value_type>(headers);
            const auto offer = Decl::offers(urd::detail::features_of<S>());
            auto uninvited = std::vector<std::string>();
            for (const auto each : urd::test::costly)
                if (!S::urd_properties.has(each) && offer->properties.has(each))
                    uninvited.emplace_back(urd::test::tag_name(each));
            result.push_back({ std::string(Decl::id),
                               std::move(headers),
                               std::string(urd::test::part_name<Decl>()),
                               urd::test::words_of<Decl>().value_or(0),
                               std::move(uninvited) });
        });
    return result;
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
            result.rows.push_back({ Q::describe(),
                                    shape_of_spec<S>(),
                                    urd::has<S, urd::small>,
                                    offered<S>(),
                                    servers<S>(),
                                    position,
                                    sizes_of<S>(),
                                    trivially<typename S::key_type> && trivially<typename S::value_type> });
            ++position;
        });
    return result;
}
