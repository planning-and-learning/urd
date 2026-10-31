// Writes share/urd/complexity.json from the declared costs (urd::container_costs, beside each representation's
// traits), or checks the committed file against them:
//
//     complexity_table --write share/urd/complexity.json
//     complexity_table --check share/urd/complexity.json     (the `complexity_table` test)
//
// One line per representation the registry can choose for a request of the grid, in the grid's order: what it is,
// the kinds of Store it serves, what it holds, and the cost of each operation it has. The docs render the
// complexity sections of docs/kinds/*.rst from this file (docs/_ext/urd_language.py).

#include "complexity.hpp"
#include "format.hpp"
#include "generated.hpp"
#include "grid.hpp"
#include "urd/format.hpp"
#include "urd/registry.hpp"
// Every family: facet_switches reads the representations.
#include "urd/use/contiguous.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/layers.hpp"
#include "urd/use/nodes.hpp"
#include "urd/use/pools.hpp"
#include "urd/use/queues.hpp"
#include "urd/use/records.hpp"
#include "urd/use/scopes.hpp"
#include "urd/use/sequences.hpp"
#if defined(URD_HAS_GTL)
#include "urd/use/parallel.hpp"
#endif

#include <array>
#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using urd::test::cost;
using urd::test::op;

using urd::test::json_text;

struct representation
{
    std::string id;
    std::string description;
    std::set<std::string> kinds;
    std::string part;  ///< what a wrapper passes operations on to
    double words = 0;  ///< memory per element, a wrapper's with its part's
    std::string holds;
    /// Each operation's cost, where R has one, and whether R passes it on to its part.
    std::array<std::optional<cost>, static_cast<std::size_t>(op::count)> costs {};
    std::array<bool, static_cast<std::size_t>(op::count)> passed_on {};
    /// The operations some request's Store over it offers: a cost passed on to the part is listed only for these.
    urd::test::offers offered {};
};

template<typename S>
std::string kind_of()
{
    if constexpr (S::is_partition)
        return "partitions";
    else if constexpr (S::is_pool)
        return urd::has<S, urd::flat> ? "records" : "pools";
    else if constexpr (S::is_queue)
        return "queues";
    else if constexpr (S::is_map)
        return "maps";
    else if constexpr (S::is_set)
        return "sets";
    else
        return "sequences";
}

template<typename R, typename S>
struct serving
{
    static constexpr bool value = urd::detail::serves<R, S>;
};

template<typename S, typename List>
struct collect;
template<typename S, typename... Rs>
struct collect<S, urd::detail::type_list<Rs...>>
{
    template<typename R>
    static void one(std::vector<representation>& into)
    {
        if constexpr (serving<R, S>::value)
        {
            const auto id = std::string(urd::test::name<R>::id);
            auto found = std::ranges::find(into, id, &representation::id);
            if (found == into.end())
            {
                auto made = representation {
                    id, std::string(urd::test::name<R>::value), {}, std::string(urd::test::part_name<R>()), urd::test::words_of<R>().value_or(0), {}
                };
                if constexpr (requires { urd::container_synopsis<R>::holds; })
                    made.holds = std::string(urd::container_synopsis<R>::holds);
                else
                    throw std::runtime_error("no synopsis declared for " + id + " (urd::container_synopsis, beside its container_traits)");
                for (std::size_t what = 0; what < made.costs.size(); ++what)
                {
                    made.costs[what] = urd::test::cost_of<R>(static_cast<op>(what));
                    made.passed_on[what] = urd::test::passed_on<R>(static_cast<op>(what));
                }
                into.push_back(std::move(made));
                found = std::prev(into.end());
            }
            found->kinds.insert(kind_of<S>());
            const auto offers = urd::test::offered<R, S>();
            for (std::size_t what = 0; what < offers.size(); ++what)
                found->offered[what] = found->offered[what] || offers[what];
        }
    }
    static void run(std::vector<representation>& into) { (one<Rs>(into), ...); }
};

std::string generate()
{
    auto all = std::vector<representation>();
    urd::test::for_each_group(
        [&](const char* /*title*/, auto list)
        {
            urd::test::for_each_request(list,
                                        [&]<typename Q>(std::type_identity<Q>)
                                        {
                                            using S = typename Q::spec;
                                            using List = typename urd::detail::ordered_for<S, typename urd::detail::candidates_for<S>::type>::type;
                                            collect<S, List>::run(all);
                                        });
        });

    auto out = std::string("{\"generated_by\": \"complexity_table --write share/urd/complexity.json, from urd::container_costs\", "
                           "\"marks\": {\"*\": \"amortized\", \"~\": \"expected\"}, \"operations\": [");
    for (std::size_t each = 0; each < urd::test::op_names.size(); ++each)
        out += (each == 0 ? "" : ", ") + json_text(urd::test::op_names[each]);
    // The growths in order, and the scale each quantity grows on (urd::name, urd::scale_of): what a comparison of costs
    // reads (benchmarks/record.py).
    out += "], \"growths\": [";
    for (auto each = urd::growth::one;; each = static_cast<urd::growth>(static_cast<int>(each) + 1))
    {
        out += (each == urd::growth::one ? "" : ", ") + json_text(urd::name(each));
        if (each == urd::growth::n_times)
            break;
    }
    out += "], \"scales\": {";
    for (auto each = urd::quantity::n;; each = static_cast<urd::quantity>(static_cast<int>(each) + 1))
    {
        out += (each == urd::quantity::n ? "" : ", ") + json_text(urd::name(each)) + ": " + json_text(urd::name(urd::scale_of(each)));
        if (each == urd::quantity::changes)
            break;
    }
    out += "}, \"representations\": [\n";
    bool first = true;
    for (const auto& each : all)
    {
        auto kinds = std::string("[");
        for (const auto& kind : each.kinds)
            kinds += (kinds.size() > 1 ? ", " : "") + json_text(kind);
        auto costs = std::string("{");
        for (std::size_t what = 0; what < urd::test::op_names.size(); ++what)
        {
            const auto& how = each.costs[what];
            if (!how || (each.passed_on[what] && !each.offered[what]))
                continue;
            costs += (costs.size() > 1 ? ", " : "") + json_text(urd::test::op_names[what]) + ": ";
            if (each.passed_on[what])
                costs += "{\"as_part\": true}";
            else
            {
                costs += "{\"cost\": " + json_text(fmt::format("{}", *how));
                costs += ", \"growth\": " + json_text(urd::name(how->grows));
                costs += ", \"in\": " + json_text(urd::name(how->of));
                costs += ", \"verified\": " + json_text(how->timed ? "timed" : "counted") + "}";
            }
        }
        out += first ? "  " : ",\n  ";
        first = false;
        out += "{\"representation\": " + json_text(each.id);
        out += ", \"description\": " + json_text(each.description);
        out += ", \"kinds\": " + kinds + "]";
        out += ", \"holds\": " + json_text(each.holds);
        if (!each.part.empty())
            out += ", \"part\": " + json_text(each.part);
        out += ", \"words\": " + std::to_string(each.words);
        out += ", \"costs\": " + costs + "}}";
    }
    return out + "\n]}\n";
}

}  // namespace

int main(int argc, char** argv) { return urd::test::write_or_check(argc, argv, "complexity_table", generate); }
