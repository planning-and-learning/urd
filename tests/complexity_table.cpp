// Writes share/urd/complexity.json from the declared costs (urd::container_costs, beside each representation's
// traits), or checks the committed file against them:
//
//     complexity_table --write share/urd/complexity.json
//     complexity_table --check share/urd/complexity.json     (the `complexity_table` test)
//
// One line per representation the registry can choose for a request of the grid, in the grid's order: what it is,
// the kinds of Store it serves, what it holds, and the cost of each operation it has. The docs render the
// complexity sections of docs/kinds/*.rst from this file (docs/_ext/urd_language.py).

// Every container, before anything that names a Store: a Store's container is resolved where the Store is
// first named, and only containers whose headers precede that are found defined.
#include "urd/repr/queues/addressable_heap.hpp"
#include "urd/repr/records/arena.hpp"
#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/queues/bucket_queue.hpp"
#include "urd/repr/wrappers/change_logged.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/maps/entry_ordered.hpp"
#include "urd/repr/maps/flat_map.hpp"
#include "urd/repr/values/flat_set.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/sequences/jagged.hpp"
#include "urd/repr/wrappers/layer.hpp"
#include "urd/repr/wrappers/layered_pool.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/values/parallel_hash_set.hpp"
#include "urd/repr/maps/parallel_hash_map.hpp"
#include "urd/repr/pools/pools.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/queues/queue.hpp"
#include "urd/repr/records/records.hpp"
#include "urd/repr/wrappers/recycling.hpp"
#include "urd/repr/queues/ring_fifo.hpp"
#include "urd/repr/sequences/row_pool.hpp"
#include "urd/repr/wrappers/scoped.hpp"
#include "urd/repr/sequences/segmented.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/slots/slots.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/values/std_set.hpp"
#include "urd/repr/pools/tree_bitset.hpp"
#include "urd/repr/pools/tree_vector.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
// The tables and the grid.
#include "complexity.hpp"
#include "format.hpp"
#include "generated.hpp"
#include "grid.hpp"
#include "urd/format.hpp"
#include "urd/registry/select.hpp"

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

/// Tags by role as a JSON list of names, each prefixed by its role ("element:can_hash"); the roots left out.
std::string tag_list(std::initializer_list<std::pair<std::string_view, urd::detail::property_set>> roles)
{
    auto result = std::string("[");
    for (const auto& [role, held] : roles)
        for (std::size_t each = 0; each < urd::detail::tag_count; ++each)
            if (held.held[each] != urd::detail::tag_at(each))
                result += (result.size() > 1 ? ", " : "") + json_text(std::string(role) + std::string(urd::tag_name(held.held[each])));
    return result + "]";
}

struct representation
{
    std::string id;
    std::string description;
    std::set<std::string> kinds;
    std::string part;  ///< what a wrapper passes operations on to
    double words = 0;  ///< memory per element, a wrapper's with its part's
    std::string holds;
    /// Its tags, for a tool to match a need against: what it provides, and what it relies on, by role.
    std::string provides;
    std::string relies_on;
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

template<typename S, typename List>
struct collect;
template<typename S, typename... Rs>
struct collect<S, urd::detail::type_list<Rs...>>
{
    template<typename R>
    static void one(std::vector<representation>& into)
    {
        {
            const auto id = std::string(urd::test::name<R>::id);
            auto found = std::ranges::find(into, id, &representation::id);
            if (found == into.end())
            {
                auto made = representation {
                    id, std::string(urd::test::name<R>::value), {}, std::string(urd::test::part_name<R>()), urd::test::words_of<R>().value_or(0), {}, {}, {}
                };
                if constexpr (requires { urd::container_synopsis<R>::holds; })
                    made.holds = std::string(urd::container_synopsis<R>::holds);
                else
                    throw std::runtime_error("no synopsis declared for " + id + " (urd::container_synopsis, beside its container_traits)");
                using declaration = urd::detail::declaration_t<R>;
                made.provides = tag_list({ { "", declaration::provides } });
                made.relies_on = tag_list({ { "", declaration::relies_on.stated },
                                            { "element:", declaration::relies_on.element },
                                            { "mapped:", declaration::relies_on.mapped },
                                            { "row:", declaration::relies_on.row },
                                            { "row_element:", declaration::relies_on.row_element } });
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
                                            using List = urd::test::serving_containers<S>;
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
                costs += ", \"in\": " + json_text(urd::name(how->of)) + "}";
            }
        }
        out += first ? "  " : ",\n  ";
        first = false;
        out += "{\"representation\": " + json_text(each.id);
        out += ", \"description\": " + json_text(each.description);
        out += ", \"kinds\": " + kinds + "]";
        out += ", \"holds\": " + json_text(each.holds);
        out += ", \"provides\": " + each.provides;
        out += ", \"relies_on\": " + each.relies_on;
        if (!each.part.empty())
            out += ", \"part\": " + json_text(each.part);
        out += ", \"words\": " + std::to_string(each.words);
        out += ", \"costs\": " + costs + "}}";
    }
    return out + "\n]}\n";
}

}  // namespace

int main(int argc, char** argv) { return urd::test::write_or_check(argc, argv, "complexity_table", generate); }
