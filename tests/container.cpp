// One container's property tests: the suite of tests/properties.hpp for the container and for the Store over it for
// each request it is tested for. CMake compiles this file once per container listed in benchmarks/requests.json
// (`tested`), in objects of a few requests each, through a generated source that includes the container's
// headers first, defines
//
//     URD_CONTAINER       its id (urd::test::name<R>::id)
//     URD_TESTED          the requests it is tested for, as group and position in the grid: g, p, ...
//     URD_CHUNK           the object's index among the container's; the first also tests the container itself
//
// and instantiates the suite for container_types under the object's name. Compiled alone (by a linter, without those
// definitions), the hash set of `Store<key<int>>` stands in.

#if !defined(URD_CONTAINER)
#include "urd/repr/change_logged.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/unordered_flat_map.hpp"
#include "urd/repr/unordered_flat_set.hpp"
#endif

#include "grid.hpp"
#include "properties.hpp"
#include "urd/registry.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace
{

#if defined(URD_CONTAINER)
constexpr urd::detail::fixed_string container = URD_CONTAINER;
constexpr std::array<std::size_t, std::size(std::initializer_list<std::size_t> { URD_TESTED })> positions { URD_TESTED };
constexpr bool with_representation = URD_CHUNK == 0;
#else
constexpr urd::detail::fixed_string container = "unordered_flat_set";
constexpr std::array<std::size_t, 2> positions { 0, 0 };
constexpr bool with_representation = true;
#endif

template<std::size_t Index>
using spec_at = typename urd::test::request_at<positions[2 * Index], positions[(2 * Index) + 1]>::spec;
template<std::size_t Index>
using store_at = urd::detail::Store<typename urd::test::serving_named<container, spec_at<Index>>::type, spec_at<Index>>;

/// The types this object tests: the container (in its first object), then the Store over it for each request.
template<std::size_t... I>
auto types_of(std::index_sequence<I...> /*requests*/)
{
    using representation = typename urd::test::serving_named<container, spec_at<0>>::type;
    if constexpr (with_representation)
        return testing::Types<representation, store_at<I>...> {};
    else
        return testing::Types<store_at<I>...> {};
}

}  // namespace

using container_types = decltype(types_of(std::make_index_sequence<positions.size() / 2>()));

#if !defined(URD_CONTAINER)
INSTANTIATE_TYPED_TEST_SUITE_P(unordered_flat_set, Properties, container_types, urd::test::Names);
#endif
