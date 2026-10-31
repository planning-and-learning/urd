// Writes share/urd/vocabulary.json from the tags and their words (urd::tag_synopsis, urd/tag_synopsis.hpp),
// or checks the committed file against them:
//
//     vocabulary_table --write share/urd/vocabulary.json
//     vocabulary_table --check share/urd/vocabulary.json     (the `vocabulary` test)
//
// One line per tag, then one per category, one per group and one per heading. Description, phrases, promise, groups,
// headings and questions are written by hand; the role is derived from the group and the promise (tests/names.hpp),
// and category, implies, excludes and statable come from the tag hierarchy, so they cannot drift from it.
// A request holds one tag per category: a tag excludes every other of its category, even one it implies (frozen and
// monotone).

#include "format.hpp"
#include "generated.hpp"
#include "names.hpp"
#include "urd/traits.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

using urd::detail::at_least;
using urd::detail::root_of;
using urd::detail::tag_count;

using urd::test::json_text;

/// The tags, other than `of`, that are no root and pass `keep`, as a JSON array.
template<typename Keep>
std::string tags_where(urd::tag of, Keep keep)
{
    auto result = std::string("[");
    for (std::size_t each = 0; each < tag_count; ++each)
    {
        const auto other = static_cast<urd::tag>(each);
        if (other == of || root_of(other) == other || !keep(other))
            continue;
        if (result.size() > 1)
            result += ", ";
        result += json_text(urd::test::tag_name(other));
    }
    return result + "]";
}

/// The entry of `of`'s category.
const urd::test::category_entry& category_of(urd::tag of) { return urd::test::category_vocabulary[urd::detail::index(root_of(of))]; }

/// A derived role is consistent with the category: kinds and guarantees are of categories containers declare, requests
/// and hints of categories only a request states; a demand may be either.
void check_role(urd::tag of, std::string_view role)
{
    const bool guaranteed = urd::detail::in(root_of(of), urd::detail::guaranteed_roots);
    const bool consistent =
        role == "demand" || ((role == "guarantee" || role == "kind") && guaranteed) || ((role == "request" || role == "hint") && !guaranteed);
    if (!consistent)
        throw std::runtime_error("the role `" + std::string(role) + "` of `" + std::string(urd::test::tag_name(of))
                                 + "` does not match its category (urd::tag_synopsis, urd/tag_synopsis.hpp)");
}

std::string generate()
{
    auto out = std::string("{\"generated_by\": \"vocabulary_table --write share/urd/vocabulary.json, from urd/tags.hpp and urd/tag_synopsis.hpp\", "
                           "\"incompatible\": \"docs/language/index.rst, Incompatible combinations\", \"tags\": [\n");
    bool first = true;
    for (std::size_t each = 0; each < tag_count; ++each)
    {
        const auto of = static_cast<urd::tag>(each);
        if (root_of(of) == of)
            continue;
        const auto& entry = urd::test::tag_vocabulary[each];
        check_role(of, entry.role);
        auto phrases = std::string("[");
        for (const auto phrase : entry.phrases)
            if (!phrase.empty())
                phrases += (phrases.size() > 1 ? ", " : "") + json_text(phrase);
        phrases += "]";
        out += first ? "  " : ",\n  ";
        first = false;
        out += "{\"tag\": " + json_text(urd::test::tag_name(of)) + ", \"category\": " + json_text(urd::test::tag_name(root_of(of)))
               + ", \"role\": " + json_text(entry.role) + ", \"description\": " + json_text(entry.description) + ", \"phrases\": " + phrases
               + ", \"implies\": " + tags_where(of, [of](urd::tag other) { return at_least(of, other); })
               + ", \"excludes\": " + tags_where(of, [of](urd::tag other) { return root_of(other) == root_of(of); })
               + ", \"group\": " + json_text(category_of(of).group) + (entry.promise.empty() ? std::string() : ", \"promise\": " + json_text(entry.promise))
               + ", \"statable\": " + (urd::detail::statable(of) ? "true" : "false") + "}";
    }
    out += "\n], \"categories\": [\n";
    first = true;
    for (std::size_t each = 0; each < tag_count; ++each)
    {
        const auto of = static_cast<urd::tag>(each);
        if (root_of(of) != of)
            continue;
        out += first ? "  " : ",\n  ";
        first = false;
        out += "{\"category\": " + json_text(urd::test::tag_name(of)) + ", \"group\": " + json_text(category_of(of).group)
               + (category_of(of).heading.empty() ? std::string() : ", \"heading\": " + json_text(category_of(of).heading))
               + ", \"question\": " + json_text(category_of(of).question) + ", \"statable\": " + (urd::detail::statable(of) ? "true" : "false")
               + ", \"tags\": " + tags_where(of, [of](urd::tag other) { return root_of(other) == of; }) + "}";
    }
    out += "\n], \"groups\": [\n";
    first = true;
    for (const auto& group : urd::test::group_vocabulary)
    {
        out += first ? "  " : ",\n  ";
        first = false;
        out += "{\"group\": " + json_text(group.id) + ", \"title\": " + json_text(group.title) + ", \"test\": " + json_text(group.test)
               + ", \"summary\": " + json_text(group.summary) + ", \"literature\": " + json_text(group.literature) + "}";
    }
    out += "\n], \"headings\": [\n";
    first = true;
    for (const auto& heading : urd::test::heading_vocabulary)
    {
        out += first ? "  " : ",\n  ";
        first = false;
        out += "{\"heading\": " + json_text(heading.id) + ", \"group\": \"semantics\", \"title\": " + json_text(heading.title) + "}";
    }
    return out + "\n]}\n";
}

}  // namespace

int main(int argc, char** argv) { return urd::test::write_or_check(argc, argv, "vocabulary_table", generate); }
