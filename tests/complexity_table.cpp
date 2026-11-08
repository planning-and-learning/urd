// Writes share/urd/complexity.json from the declared costs (urd::container_costs, beside each representation's
// traits), or checks the committed file against them:
//
//     complexity_table --write share/urd/complexity.json
//     complexity_table --check share/urd/complexity.json     (the `complexity_table` test)
//
// One line per representation the registry can choose for a request of the grid, in the grid's order: what it is,
// the kinds of Store it serves, what it holds, and the cost of each operation it has. The docs render the
// complexity sections of docs/kinds/*.rst from this file (docs/_ext/urd_language.py). It fails where a candidate
// offers an operation without a cost, or a representation declares a cost no request of the grid offers.

// Every container, before anything that names a Store: a Store's container is resolved where the Store is
// first named, and only containers whose headers precede that are found defined.
#include "all_containers.hpp"
// The tables and the grid.
#include "complexity.hpp"
#include "format.hpp"
#include "generated.hpp"
#include "grid.hpp"
#include "urd/format.hpp"
#include "urd/registry/select.hpp"

#include <array>
#include <concepts>
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
using urd::test::op_count;

using urd::test::json_text;

/// The tags about the Store itself, or about role `role`, as a JSON list of names prefixed by the role ("row:unique"),
/// in the vocabulary's order; the roots left out.
std::string tag_list(const urd::detail::property_set& held, const urd::role_info* role = nullptr, std::string result = "[")
{
    const auto prefix = role != nullptr ? std::string(role->name) + ":" : std::string();
    for (const auto* category : urd::test::category_tables)  // in the vocabulary's order
        if (const auto each = held.in(category, role); each.answer != 0)
            result += (result.size() > 1 ? ", " : "") + json_text(prefix + std::string(urd::tag_name(each)));
    return result;
}
/// The traits about role `role`, as names prefixed by it ("element:can_hash"), in the vocabulary's order.
std::string trait_list(std::string result, const urd::role_info* role, const urd::detail::property_set& held)
{
    for (const auto* each : urd::test::vocabulary_traits)
        if (held.in(each, role).answer != 0)
            result += (result.size() > 1 ? ", " : "") + json_text(std::string(role->name) + ":" + std::string(each->name));
    return result;
}
/// What a declaration relies on, as a JSON list: what a request states, then each role's tags and traits prefixed by
/// its name ("element:can_hash"), in the vocabulary's order.
std::string roles_list(const urd::detail::property_set& relied)
{
    auto result = tag_list(relied);
    for (const auto* role : urd::test::vocabulary_roles)
    {
        result = tag_list(relied, role, result);
        result = trait_list(result, role, relied);
    }
    return result + "]";
}

struct representation
{
    std::string id;
    std::string description;
    std::set<std::string> kinds;
    std::string part;                            ///< what a wrapper passes operations on to
    double words = 0;                            ///< memory per element, a wrapper's with its part's
    urd::quantity words_per = urd::quantity::n;  ///< what words count per: an element, a key, a slot
    std::string holds;
    /// Its tags, for a tool to match a need against: what it provides, and what it relies on, by role.
    std::string provides;
    std::string relies_on;
    /// Each operation's cost, where R has one, and whether R passes it on to its part.
    std::array<std::optional<cost>, op_count> costs {};
    std::array<bool, op_count> passed_on {};
    /// The operations some request's Store over it offers: a cost passed on to the part is listed only for these.
    urd::test::offers offered {};
};

template<typename S>
std::string kind_of()
{ return std::string(S::kind::page(S::urd_properties)); }

template<typename S, typename List>
struct collect;
template<typename S, typename... Rs>
struct collect<S, boost::mp11::mp_list<Rs...>>
{
    template<typename R>
    static void one(std::vector<representation>& into)
    {
        {
            const auto id = std::string(urd::test::name<R>::id);
            auto found = std::ranges::find(into, id, &representation::id);
            if (found == into.end())
            {
                auto made = representation { id,
                                             std::string(urd::test::name<R>::value),
                                             {},
                                             std::string(urd::test::part_name<R>()),
                                             urd::test::words_of<R>().value_or(0),
                                             urd::test::words_per_of<urd::detail::declaration_t<R>>(),
                                             {},
                                             {},
                                             {} };
                if constexpr (requires { urd::container_synopsis<R>::holds; })
                    made.holds = std::string(urd::container_synopsis<R>::holds);
                else
                    throw std::runtime_error("no synopsis declared for " + id + " (urd::container_synopsis, beside its container_traits)");
                using declaration = urd::detail::declaration_t<R>;
                made.provides = tag_list(declaration::provides) + "]";
                made.relies_on = roles_list(declaration::relies_on);
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
    // The costs cover what the registry can choose: every operation a serving candidate's Store offers has a cost, and
    // every cost a representation declares itself is for an operation offered over it or over a wrapper of it.
    auto used = std::set<std::pair<std::string, std::size_t>>();
    for (const auto& each : all)
        for (std::size_t what = 0; what < op_count; ++what)
            if (each.offered[what])
            {
                if (!each.costs[what])
                    throw std::runtime_error(each.id + " " + std::string(urd::test::op_names[what])
                                             + ": no cost declared (urd::container_costs, beside its container_traits)");
                used.emplace(each.id, what);
                if (!each.part.empty())
                    used.emplace(each.part, what);
            }
    for (const auto& each : all)
        for (std::size_t what = 0; what < op_count; ++what)
            if (each.costs[what] && !each.passed_on[what] && !used.contains({ each.id, what }))
                throw std::runtime_error(each.id + " " + std::string(urd::test::op_names[what])
                                         + ": no request of the grid offers it; drop the cost (urd::container_costs)");

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
        out += ", \"memory\": " + json_text(fmt::format("{}", urd::cost { .grows = urd::growth::linear, .of = each.words_per }));
        out += ", \"costs\": " + costs + "}}";
    }
    return out + "\n]}\n";
}

}  // namespace

int main(int argc, char** argv) { return urd::test::write_or_check(argc, argv, "complexity_table", generate); }
