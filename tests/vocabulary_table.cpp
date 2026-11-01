// Writes share/urd/vocabulary.json from the tags and their words (beside each tag, urd/tags.hpp),
// or checks the committed file against them:
//
//     vocabulary_table --write share/urd/vocabulary.json
//     vocabulary_table --check share/urd/vocabulary.json     (the `vocabulary` test)
//
// One line per tag, then one per category, one per group and one per heading. Description, phrases, promise, groups,
// headings and questions are written by hand; the role is derived from the group and the promise (tests/names.hpp),
// and category, implies and excludes come from the tag hierarchy, and a category's kinds from the kinds' request
// formats (urd/kinds.hpp), so they cannot drift from them.
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

using urd::test::json_text;

/// A category's none: what names the category.
urd::tag none_of(urd::tag of) { return { of.in, 0 }; }

/// The tags, other than `of`, that are no category's none and pass `keep`, as a JSON array.
template<typename Keep>
std::string tags_where(urd::tag of, Keep keep)
{
    auto result = std::string("[");
    urd::test::for_each_tag(
        [&](urd::tag other)
        {
            if (other == of || other.answer == 0 || !keep(other))
                return;
            if (result.size() > 1)
                result += ", ";
            result += json_text(urd::test::tag_name(other));
        });
    return result + "]";
}

/// The kinds whose request format answers `of`'s category, as a JSON array.
std::string kinds_answering(urd::tag of)
{
    auto result = std::string("[");
    [&]<typename... Kinds>(urd::detail::type_list<Kinds...> /*kinds*/)
    {
        (
            [&]<typename... Es>(urd::detail::request_format<Es...> /*format*/)
            {
                if (((urd::category<Es> == of.in) || ...))
                    result += (result.size() > 1 ? ", " : "") + json_text(Kinds::name);
            }(typename Kinds::format {}),
            ...);
    }(urd::leaf_kinds<> {});
    return result + "]";
}

/// The entry of `of`'s category.
const urd::test::category_entry& category_of(urd::tag of) { return urd::test::category_entry_of(of); }

/// A derived role is consistent with the category: kinds and guarantees are of categories containers declare, requests
/// and hints of categories only a request states; a demand may be either.
void check_role(urd::tag of, std::string_view role)
{
    const bool guaranteed = !of.in->request_only;
    const bool consistent =
        role == "demand" || ((role == "guarantee" || role == "kind") && guaranteed) || ((role == "request" || role == "hint") && !guaranteed);
    if (!consistent)
        throw std::runtime_error("the role `" + std::string(role) + "` of `" + std::string(urd::test::tag_name(of))
                                 + "` does not match its category (beside each tag, urd/tags.hpp)");
}

std::string generate()
{
    auto out = std::string("{\"generated_by\": \"vocabulary_table --write share/urd/vocabulary.json, from urd/tags.hpp\", "
                           "\"incompatible\": \"docs/language/index.rst, Incompatible combinations\", \"tags\": [\n");
    bool first = true;
    urd::test::for_each_tag(
        [&](urd::tag of)
        {
            if (of.answer == 0)
                return;
            const auto& entry = urd::test::tag_entry_of(of);
            check_role(of, entry.role);
            auto phrases = std::string("[");
            for (const auto phrase : entry.phrases)
                if (!phrase.empty())
                    phrases += (phrases.size() > 1 ? ", " : "") + json_text(phrase);
            phrases += "]";
            out += first ? "  " : ",\n  ";
            first = false;
            out += "{\"tag\": " + json_text(urd::test::tag_name(of)) + ", \"category\": " + json_text(urd::test::tag_name(none_of(of)))
                   + ", \"role\": " + json_text(entry.role) + ", \"description\": " + json_text(entry.description) + ", \"phrases\": " + phrases
                   + ", \"implies\": " + tags_where(of, [of](urd::tag other) { return at_least(of, other); }) + ", \"excludes\": "
                   + tags_where(of, [of](urd::tag other) { return other.in == of.in; }) + ", \"group\": " + json_text(category_of(of).group)
                   + (entry.promise.empty() ? std::string() : ", \"promise\": " + json_text(entry.promise)) + "}";
        });
    out += "\n], \"categories\": [\n";
    first = true;
    urd::test::for_each_tag(
        [&](urd::tag of)
        {
            if (of.answer != 0)
                return;
            out += first ? "  " : ",\n  ";
            first = false;
            out += "{\"category\": " + json_text(urd::test::tag_name(of)) + ", \"group\": " + json_text(category_of(of).group)
                   + (category_of(of).heading.empty() ? std::string() : ", \"heading\": " + json_text(category_of(of).heading))
                   + ", \"question\": " + json_text(category_of(of).question) + ", \"kinds\": " + kinds_answering(of)
                   + ", \"tags\": " + tags_where(of, [of](urd::tag other) { return other.in == of.in; }) + "}";
        });
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
