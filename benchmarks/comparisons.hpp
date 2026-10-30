#pragma once

// The claims the registry makes, derived: for every request of the grid, the container chosen (A) and every other
// one that serves it (B), compared by what the operations the request's Store offers cost each (tests/complexity.hpp).
//
// - settled: A grows slower on some operation and faster on none; no benchmark needed.
// - wrong_order: B grows slower on some and faster on none; the registry's order is wrong.
// - measure: they grow alike on every operation (or the request says small, where constants decide): a benchmark
//   shows which is faster (benchmarks/comparisons.cpp, benchmarks/record.py).
// - trade_off: each grows slower somewhere, a cost is in another quantity, or B grows slower but A takes less
//   memory (memory_table); a reason is written down
//   (tests/registry.cpp).
//
// Comparisons with the same A, B, operations, element shape and small are one: the first request of the grid stands
// for the others. The tests, the generator (benchmarks/comparison_table.cpp) and the benchmarks all read this header, so
// they cannot disagree. Includes every family: costs read the representations.

#include "complexity.hpp"
#include "grid.hpp"
#include "urd/registry.hpp"
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
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace urd::test
{

enum class outcome : std::uint8_t
{
    settled,
    wrong_order,
    measure,
    trade_off,
};
inline constexpr std::array<std::string_view, 4> outcome_names { "settled", "wrong_order", "measure", "trade_off" };

using offers = std::array<bool, static_cast<std::size_t>(op::count)>;

/// A benchmark: one operation of one representation at one size, on one shape of element.
struct cell
{
    std::string id;  ///< "<representation>/<operation>/<shape>[/small]/<n>"
    op what = op::insert;
    std::size_t size = 0;
};

struct comparison
{
    std::string id;  ///< "A > B / operations / shape[ / small]"
    std::string chosen;
    std::string other;
    offers operations {};
    std::string shape;
    bool small = false;
    /// Words per element (memory_table): what lets A's slower growth be a trade-off rather than a wrong order.
    double chosen_words = 0;
    double other_words = 0;
    outcome result = outcome::settled;
    std::vector<std::string> requests;
    /// The sizes measured: elements, or a row's length for a pool's row operations.
    std::vector<std::size_t> sizes;
    /// The benchmark cells it reads (cells_of), for A and for B; none for a settled comparison.
    std::vector<cell> chosen_cells;
    std::vector<cell> other_cells;
};

/// Whether a workload runs `what` n times (and so is quadratic where one costs O(n)): inserts, pushes and pops
/// run over every element, for their amortized cost; lookups and erasures probe a fixed number.
constexpr bool runs_n_times(op what) { return what == op::insert || what == op::push || what == op::pop; }

/// The cells a comparison reads for one of its representations: "<representation>/<operation>/<shape>/<n>", with
/// "/small" before the size for a small request.
inline std::vector<cell> cells_of(const comparison& found, bool chosen, const std::array<std::optional<cost>, static_cast<std::size_t>(op::count)>& costs)
{
    auto result = std::vector<cell>();
    if (found.result != outcome::measure && found.result != outcome::trade_off)
        return result;
    for (std::size_t each = 0; each < found.operations.size(); ++each)
    {
        if (!found.operations[each] || !costs[each])
            continue;
        const auto what = static_cast<op>(each);
        for (const auto size : found.sizes)
        {
            if (runs_n_times(what) && costs[each]->grows >= growth::linear && size > 10000)
                continue;  // quadratic over the run
            result.push_back({ (chosen ? found.chosen : found.other) + "/" + std::string(op_names[each]) + "/" + found.shape
                                   + (found.small ? "/small" : "") + "/" + std::to_string(size),
                               what, size });
        }
    }
    return result;
}

template<typename R>
std::array<std::optional<cost>, static_cast<std::size_t>(op::count)> costs_of()
{
    auto result = std::array<std::optional<cost>, static_cast<std::size_t>(op::count)>();
    for (std::size_t each = 0; each < result.size(); ++each)
        result[each] = cost_of<R>(static_cast<op>(each));
    return result;
}

namespace comparisons_detail
{

/// Which quantity a cost grows in, for comparing: the element count, the key range and the capacity are one.
constexpr int scale(in of)
{
    switch (of)
    {
        case in::n:
        case in::keys:
        case in::capacity:
            return 0;
        case in::row:
            return 1;
        case in::delta:
            return 2;
        case in::changes:
            return 3;
    }
    return 0;
}

template<typename T>
std::string shape_of()
{
    if constexpr (std::is_void_v<T>)
        return "-";
    else if constexpr (urd::is_store<T>)
        return "store";
    else if constexpr (std::is_same_v<T, std::string>)
        return "string";
    else
        return "integral";
}

template<typename A, typename B, typename S, typename Q>
comparison compare()
{
    auto result = comparison {};
    result.chosen = std::string(name<A>::id);
    result.other = std::string(name<B>::id);
    result.operations = offered<A, S>();
    result.shape = shape_of<typename S::key_type>() + "," + shape_of<typename S::value_type>();
    result.small = urd::has<S, urd::small>;
    result.chosen_words = words_of<A>().value_or(0);
    result.other_words = words_of<B>().value_or(0);
    result.requests.push_back(Q::describe());
    bool a_better = false;
    bool b_better = false;
    bool apart = false;
    bool rows = false;
    for (std::size_t each = 0; each < result.operations.size(); ++each)
    {
        if (!result.operations[each])
            continue;
        const auto what = static_cast<op>(each);
        rows = rows || what == op::insert_row || what == op::find_row;
        const auto a = cost_of<A>(what);
        const auto b = cost_of<B>(what);
        if (!a || !b)
            continue;  // tests/complexity.cpp names it
        if (scale(a->of) != scale(b->of))
            apart = true;
        else if (a->grows < b->grows)
            a_better = true;
        else if (b->grows < a->grows)
            b_better = true;
    }
    if (!result.small && (apart || (a_better && b_better)))
        result.result = outcome::trade_off;
    else if (!result.small && a_better)
        result.result = outcome::settled;
    else if (!result.small && b_better)
        // B grows slower: only A's smaller memory can justify choosing it.
        result.result = result.chosen_words < result.other_words ? outcome::trade_off : outcome::wrong_order;
    else
        result.result = outcome::measure;  // alike, or small: constants decide
    if (result.small)
        result.sizes = { 8, 64 };
    else if (rows)
        result.sizes = { 16, 256, 4096 };
    else
        result.sizes = { 100, 10000, 1000000 };
    auto names = std::string();
    for (std::size_t each = 0; each < result.operations.size(); ++each)
        if (result.operations[each])
            names += (names.empty() ? "" : ",") + std::string(op_names[each]);
    result.chosen_cells = cells_of(result, true, costs_of<A>());
    result.other_cells = cells_of(result, false, costs_of<B>());
    result.id = result.chosen + " > " + result.other + " / " + names + " / " + result.shape + (result.small ? " / small" : "");
    return result;
}

template<typename R, typename S>
struct serving
{
    static constexpr bool value = urd::detail::serves<R, S>;
};

/// For request Q: every serving B other than the chosen A, compared; f is called once per new comparison id with the
/// types, and every comparison (new or not) is recorded in `into`.
template<typename Q, typename List>
struct visit;
template<typename Q, typename... Rs>
struct visit<Q, urd::detail::type_list<Rs...>>
{
    using S = typename Q::spec;

    template<typename A, typename B, typename F>
    static void one(bool& passed_chosen, std::map<std::string, comparison>& into, F& f)
    {
        if constexpr (serving<B, S>::value)
        {
            if (!passed_chosen)
            {
                passed_chosen = true;  // B is A: the first that serves
                return;
            }
            auto found = compare<A, B, S, Q>();
            const auto existing = into.find(found.id);
            if (existing != into.end())
            {
                existing->second.requests.push_back(Q::describe());
                return;
            }
            f.template operator()<Q, A, B>(found);
            into.emplace(found.id, std::move(found));
        }
    }

    template<typename F>
    static void run(std::map<std::string, comparison>& into, F& f)
    {
        if constexpr (urd::detail::choose<S>::found)
        {
            using A = typename urd::detail::choose<S>::type;
            bool passed_chosen = false;
            (one<A, Rs>(passed_chosen, into, f), ...);
        }
    }
};

}  // namespace comparisons_detail

/// Every comparison of the grid, by id, and for each new one f.template operator()<Q, A, B>(comparison) with the
/// request type that stands for it and the two representations.
template<typename F>
std::map<std::string, comparison> for_each_comparison(F f)
{
    auto all = std::map<std::string, comparison>();
    for_each_group(
        [&](const char* /*title*/, auto list)
        {
            for_each_request(list,
                             [&]<typename Q>(std::type_identity<Q>)
                             {
                                 using S = typename Q::spec;
                                 using List = typename urd::detail::ordered_for<S, typename urd::detail::candidates_for<S>::type>::type;
                                 comparisons_detail::visit<Q, List>::run(all, f);
                             });
        });
    return all;
}

inline std::map<std::string, comparison> all_comparisons()
{
    return for_each_comparison([]<typename Q, typename A, typename B>(const comparison& /*found*/) {});
}

}  // namespace urd::test
