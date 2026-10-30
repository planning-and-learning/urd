// Writes docs/RESOLUTION.md from the request grid (tests/grid.hpp), or checks the committed file against it:
//
//     resolution_table --write docs/RESOLUTION.md
//     resolution_table --check docs/RESOLUTION.md     (the `resolution` test; exits 1 and names the first difference)
//
// No Store is instantiated: each request resolves through the registry's choose, which reads only traits.

#include "grid.hpp"
#include "urd/registry.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

template<typename S>
std::string_view resolved_name()
{
    if constexpr (urd::detail::choose<S>::found)
        return urd::test::name<typename urd::detail::choose<S>::type>::id;
    else
        return "none";
}

/// The families whose headers a Store of R needs: R's, and its parts' (a wrapper's part comes first, as its error
/// does). Named by the header's stem: `hash`, `hash + layers`.
template<typename R>
void families_of(std::vector<std::string>& into)
{
    [&]<typename... Ps>(urd::detail::type_list<Ps...>) { (families_of<Ps>(into), ...); }(typename urd::parts<R>::type {});
    auto header = std::string(urd::defined_in<R>.c_str());
    header = header.substr(header.rfind('/') + 1);
    header = header.substr(0, header.rfind('.'));
    if (header == "<unnamed>")
        throw std::runtime_error("a registered representation names no family header (urd::defined_in, beside its declaration): "
                                 + std::string(urd::test::name<R>::id));
    if (std::find(into.begin(), into.end(), header) == into.end())
        into.push_back(header);
}
template<typename S>
std::string families()
{
    if constexpr (!urd::detail::choose<S>::found)
        return "";
    else
    {
        auto names = std::vector<std::string>();
        families_of<typename urd::detail::choose<S>::type>(names);
        auto result = std::string();
        for (const auto& name : names)
            result += (result.empty() ? "" : " + ") + name;
        return result;
    }
}

/// The spec's tags: what the request implies beyond what it states.
template<typename S>
std::string spec_tags()
{
    auto result = std::string();
    for (std::size_t each = 0; each < urd::detail::tag_count; ++each)
    {
        const auto root = static_cast<urd::tag>(each);
        if (urd::detail::root_of(root) == root && S::urd_properties.in(root) != root)
        {
            if (!result.empty())
                result += ", ";
            result += urd::test::tag_name(S::urd_properties.in(root));
        }
    }
    return result;
}

/// Why a request of the grid resolves to `none`, keyed by the request as written. A missing representation is
/// one that could be written (nobody has needed it yet); an incompatible one is a contract no representation can
/// keep, which the registry rejects with culprits (its compile-fail case is named). generate() fails on a `none`
/// row without a note, and on a note whose request is served now.
const std::map<std::string, std::string> notes = {
    { "Store<key<int>, dense, concurrent>", "missing: a dense set written by threads (an atomic bitset); docs/SURVEY.md gap 5" },
    { "Store<key<Id>, dense, concurrent>", "missing: a dense set written by threads (an atomic bitset); docs/SURVEY.md gap 5" },
    { "Store<key<int>, dense, stable_refs>", "missing: dense keys in segmented slots; M14" },
    { "Store<key<Id>, dense, stable_refs>", "missing: dense keys in segmented slots; M14" },
    { "Store<key<index<Id>>, value<Store<value<Store<value<index<Id>>, dense, stable_refs, frozen, functional, positional>>, unique, "
      "concurrent, layered, handles, growable, monotone>>, dense, layered, concurrent>",
      "missing: the concurrent slot directory (compile-fail case 43)" },
    { "Store<key<int>, value<int>, concurrent, retains>", "incompatible: the concurrent tables return their storage on clear" },
    { "Store<key<std::string>, value<double>, concurrent, retains>", "incompatible: the concurrent tables return their storage on clear" },
    { "Store<key<Id>, value<Id>, concurrent, retains>", "incompatible: the concurrent tables return their storage on clear" },
    { "Store<value<int>, handles, layered>", "incompatible: slots are recycled, so no layer continues their handles (compile-fail case 40)" },
    { "Store<value<Store<value<int>, dense, stable_refs, frozen, functional, positional>>, handles, unique, structural_sharing, layered>",
      "incompatible: a tree pool numbers nodes, not rows (compile-fail case 41)" },
    { "Store<key<Store<key<int>, unique, shrinkable>>>", "incompatible: an unordered Store is no hashed key (compile-fail case 32)" },
    { "Store<key<Store<key<int>, unique, dense, stable_refs, frozen>>>", "incompatible: dense alone fixes no order (compile-fail case 33)" },
    { "Store<key<int>, insertion_ordered, reclaims>", "missing: an insertion-ordered set returning storage per erase (a tombstoned node table); no site asks" },
    { "Store<key<int>, scoped>", "missing: a scoped set (the Scoped wrapper over a set); runir's parser scopes are maps" },
    { "Store<key<int>, positional, monotone, layered>", "missing: a layered interner continuing its parent's positions, an iterator over both; own milestone" },
    { "Store<key<int>, change_log, concurrent>", "missing: a change log written by threads at once; no site asks" },
    { "Store<key<std::string>, insertion_ordered, reclaims>",
      "missing: an insertion-ordered set returning storage per erase (a tombstoned node table); no site asks" },
    { "Store<key<std::string>, scoped>", "missing: a scoped set (the Scoped wrapper over a set); runir's parser scopes are maps" },
    { "Store<key<std::string>, positional, monotone, layered>",
      "missing: a layered interner continuing its parent's positions, an iterator over both; own milestone" },
    { "Store<key<std::string>, change_log, concurrent>", "missing: a change log written by threads at once; no site asks" },
    { "Store<key<Id>, insertion_ordered, reclaims>", "missing: an insertion-ordered set returning storage per erase (a tombstoned node table); no site asks" },
    { "Store<key<Id>, scoped>", "missing: a scoped set (the Scoped wrapper over a set); runir's parser scopes are maps" },
    { "Store<key<Id>, positional, monotone, layered>", "missing: a layered interner continuing its parent's positions, an iterator over both; own milestone" },
    { "Store<key<Id>, change_log, concurrent>", "missing: a change log written by threads at once; no site asks" },
    { "Store<key<int>, value<int>, concurrent, layered>",
      "missing: a layer over a concurrent hash map (lookups fall through under the shard locks); no site asks" },
    { "Store<key<std::string>, value<double>, concurrent, layered>",
      "missing: a layer over a concurrent hash map (lookups fall through under the shard locks); no site asks" },
    { "Store<key<Id>, value<Id>, concurrent, layered>",
      "missing: a layer over a concurrent hash map (lookups fall through under the shard locks); no site asks" },
    { "Store<key<int>, dense, sorted, constant_clear>", "missing: a generation-stamped bitset (a stamp per word, sorted iteration); no site asks" },
    { "Store<key<int>, dense, sorted, contiguous, frozen>",
      "incompatible in effect: drop dense for flat_set (sorted, contiguous), or drop sorted for SparseSet (dense, contiguous)" },
    { "Store<key<int>, value<int>, dense, contiguous>",
      "missing: values() of a dense, insertion_ordered map is the span wanted; no slot-per-index map is contiguous in its values" },
    { "Store<key<int>, value<int>, multi, dense, bidirectional>",
      "missing: the dynamic bidirectional graph (Bidirectional over DenseGroups); docs/SURVEY.md gap 6, after M15" },
    { "Store<key<Id>, dense, sorted, constant_clear>", "missing: a generation-stamped bitset (a stamp per word, sorted iteration); no site asks" },
    { "Store<key<Id>, dense, sorted, contiguous, frozen>",
      "incompatible in effect: drop dense for flat_set (sorted, contiguous), or drop sorted for SparseSet (dense, contiguous)" },
    { "Store<key<Id>, value<double>, dense, contiguous>",
      "missing: values() of a dense, insertion_ordered map is the span wanted; no slot-per-index map is contiguous in its values" },
    { "Store<key<Id>, value<Id>, multi, dense, bidirectional>",
      "missing: the dynamic bidirectional graph (Bidirectional over DenseGroups); docs/SURVEY.md gap 6, after M15" },
    { "Store<key<int>, value<int>, sorted, small, multi, order_by<P>>", "missing: a multimap ordered by its values (FlatEntryMap is unique); no site asks" },
    { "Store<key<int>, value<int>, sorted, small, stable_refs, order_by<P>>",
      "missing: entries ordered by value in nodes (a tree keyed by the entry); no site asks" },
    { "Store<value<int>, reclaims>",
      "missing: a vector returns its storage only whole (shrink_to_fit); a sequence returning storage per erase is a deque of blocks" },
    { "Store<value<int>, lifo, constant_clear>",
      "missing: a stack clears by size reset already (proportional in its destructors); constant_clear as a promise on queues is not declared" },
    { "Store<value<int>, by_priority, constant_clear>", "missing: a heap clears by size reset already; constant_clear as a promise on queues is not declared" },
    { "Store<value<Id>, lifo, constant_clear>",
      "missing: a stack clears by size reset already (proportional in its destructors); constant_clear as a promise on queues is not declared" },
    { "Store<value<Id>, by_priority, constant_clear>", "missing: a heap clears by size reset already; constant_clear as a promise on queues is not declared" },
    { "Store<key<std::string>, partition>", "missing: a partition over hashed keys (a map from key to position plus the forest); no site asks" },
};

std::string generate()
{
    auto out = std::ostringstream();
    out << "# What a request resolves to\n\n"
           "Generated by `resolution_table --write docs/RESOLUTION.md` from the request grid in `tests/grid.hpp`, and checked\n"
           "against the registry by the `resolution` test: a change to `include/urd/registry.hpp` that moves a request shows up\n"
           "here. Each row is a request as written, the properties its spec holds (what the request implies beyond what it\n"
           "states), the representation chosen, and the family header (`urd/use/<family>.hpp`) a file holding such a Store\n"
           "includes, a wrapper's part first. This table is built with no family header included: resolving completes nothing.\n"
           "A request is one of three things: served; `none` and missing, a\n"
           "representation that could be written (its note says which); or incompatible, a contract no representation can\n"
           "keep, which `Store` rejects with a message (the rules are listed in docs/properties.rst, \"Incompatible\n"
           "combinations\"; a request rejected by a rule cannot appear here, one rejected by the registry appears as `none`\n"
           "with its culprits' compile-fail case). `Id` is a key identified by one integral member (hashable, orderable,\n"
           "indexable).\n";
    auto noted = std::set<std::string>();
    urd::test::for_each_group(
        [&](const char* title, auto list)
        {
            out << "\n## " << title << "\n\n| Request | Spec | Representation | Include |\n|---|---|---|---|\n";
            urd::test::for_each_request(list,
                                        [&]<typename R>(std::type_identity<R>)
                                        {
                                            const auto request = R::describe();
                                            const auto resolved = resolved_name<typename R::spec>();
                                            out << "| `" << request << "` | " << spec_tags<typename R::spec>() << " | `" << resolved << "`";
                                            if (resolved == "none")
                                            {
                                                const auto note = notes.find(request);
                                                if (note == notes.end())
                                                    throw std::runtime_error("no note says why `" + request
                                                                             + "` resolves to none (notes in tests/resolution_table.cpp)");
                                                out << " — " << note->second;
                                                noted.insert(request);
                                            }
                                            else if (notes.contains(request))
                                                throw std::runtime_error("`" + request + "` is served now; drop its note in tests/resolution_table.cpp");
                                            out << " | " << families<typename R::spec>() << " |\n";
                                        });
        });
    for (const auto& [request, note] : notes)
        if (!noted.contains(request))
            throw std::runtime_error("the note for `" + request + "` names no request of the grid (tests/grid.hpp)");
    return out.str();
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 3 || (std::strcmp(argv[1], "--write") != 0 && std::strcmp(argv[1], "--check") != 0))
    {
        std::cerr << "usage: resolution_table --write <path> | --check <path>\n";
        return 2;
    }
    const auto path = std::string(argv[2]);
    auto generated = std::string();
    try
    {
        generated = generate();
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << "resolution_table: " << error.what() << "\n";
        return 1;
    }
    if (std::strcmp(argv[1], "--write") == 0)
    {
        auto file = std::ofstream(path);
        file << generated;
        return file ? 0 : 1;
    }
    auto file = std::ifstream(path);
    if (!file)
    {
        std::cerr << "resolution_table: cannot read " << path << "\n";
        return 1;
    }
    const auto committed = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (committed == generated)
        return 0;
    auto expected = std::istringstream(generated);
    auto actual = std::istringstream(committed);
    auto expected_line = std::string();
    auto actual_line = std::string();
    for (int line = 1;; ++line)
    {
        const bool more_expected = static_cast<bool>(std::getline(expected, expected_line));
        const bool more_actual = static_cast<bool>(std::getline(actual, actual_line));
        if (!more_expected && !more_actual)
            break;
        if (expected_line != actual_line || more_expected != more_actual)
        {
            std::cerr << "resolution_table: " << path << " differs from the registry at line " << line << ":\n  committed: " << actual_line
                      << "\n  registry:  " << expected_line << "\nregenerate with: resolution_table --write " << path << "\n";
            return 1;
        }
    }
    return 1;
}
