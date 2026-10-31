#pragma once

// Names of the containers a Store can resolve to, read from their synopses (urd::container_synopsis), and the
// vocabulary as the tests and generated files spell it.

#include "urd/store.hpp"
#include "urd/synopsis.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace urd::test
{

/// A representation's name and description, from its synopsis (urd::container_synopsis, beside its traits); a type
/// that declares none is "other".
template<typename R>
struct name
{
    static constexpr std::string_view id = "other";
    static constexpr std::string_view value = "a container of its own";
};
template<typename R>
    requires requires { urd::container_synopsis<R>::id; }
struct name<R>
{
    static constexpr std::string_view id = urd::container_synopsis<R>::id;
    static constexpr std::string_view value = urd::container_synopsis<R>::description;
};

/// The vocabulary as arrays in the order of urd::tag, computed from each tag's words (urd::tag_synopsis, beside the
/// tag in urd/traits.hpp): for printing a request or a spec, and for share/urd/vocabulary.json
/// (tests/vocabulary_table.cpp).
template<typename... Tags>
constexpr auto names_of(urd::detail::type_list<Tags...> /*tags*/)
{ return std::array<std::string_view, sizeof...(Tags)> { urd::tag_synopsis<Tags>::name... }; }
inline constexpr std::array<std::string_view, urd::detail::tag_count> tag_names = names_of(urd::detail::all_tags {});
constexpr std::string_view tag_name(urd::tag of) { return tag_names[urd::detail::index(of)]; }

/// What a tag is to the one who writes a request: its role, description and phrases (urd::tag_synopsis). A category's
/// root has an empty entry: it promises nothing and is never written.
struct tag_entry
{
    std::string_view role;
    std::string_view description;
    std::array<std::string_view, 4> phrases;
};
template<typename Tag>
constexpr tag_entry entry_of()
{
    using words = urd::tag_synopsis<Tag>;
    auto result = tag_entry {};
    if constexpr (requires { words::role; })
    {
        result.role = words::role;
        result.description = words::description;
        for (std::size_t each = 0; each < words::phrases.size(); ++each)
            result.phrases[each] = words::phrases[each];
    }
    return result;
}
template<typename... Tags>
constexpr auto entries_of(urd::detail::type_list<Tags...> /*tags*/)
{ return std::array<tag_entry, sizeof...(Tags)> { entry_of<Tags>()... }; }
inline constexpr std::array<tag_entry, urd::detail::tag_count> tag_vocabulary = entries_of(urd::detail::all_tags {});

/// The themes, the areas a reader may care about (urd::all_themes): they group the categories and have no rules of
/// their own.
struct theme_entry
{
    std::string_view id;
    std::string_view title;
    std::string_view summary;
};
inline constexpr auto theme_vocabulary = []<typename... Themes>(std::type_identity<std::tuple<Themes...>> /*themes*/)
{
    return std::array<theme_entry, sizeof...(Themes)> { theme_entry { Themes::id, Themes::title, Themes::summary }... };
}(std::type_identity<urd::all_themes> {});

/// A category to the one who writes a request: its theme and the question its tags answer (urd::tag_synopsis of its
/// root). Whether a request may state its tags comes from the code (urd::detail::statable).
struct category_entry
{
    std::string_view theme;
    std::string_view question;
};
template<typename Tag>
constexpr category_entry category_of()
{
    using words = urd::tag_synopsis<Tag>;
    if constexpr (requires { words::question; })
        return { words::theme::id, words::question };
    else
        return {};
}
template<typename... Tags>
constexpr auto categories_of(urd::detail::type_list<Tags...> /*tags*/)
{ return std::array<category_entry, sizeof...(Tags)> { category_of<Tags>()... }; }
inline constexpr std::array<category_entry, urd::detail::tag_count> category_vocabulary = categories_of(urd::detail::all_tags {});

// Every tag has its words: a name, and either a category's question (a root) or a role (any other tag); a tag added
// without them fails here.
static_assert(
    []
    {
        for (std::size_t each = 0; each < urd::detail::tag_count; ++each)
        {
            const auto of = static_cast<urd::tag>(each);
            const bool root = urd::detail::root_of(of) == of;
            if (tag_names[each].empty() || tag_vocabulary[each].role.empty() != root || category_vocabulary[each].question.empty() == root)
                return false;
        }
        return true;
    }(),
    "urd::tag_synopsis: every tag has a name; a root a theme and a question, any other tag a role, description and phrases");
static_assert(tag_name(urd::tag::sorted) == "sorted" && tag_vocabulary[urd::detail::index(urd::tag::monotone_priority)].role == "demand"
              && category_vocabulary[urd::detail::index(urd::tag::single_threaded)].theme == "threads");

/// Names typed tests by container: a Store as "Store_" and its representation, with the index for uniqueness.
struct Names
{
    template<typename T>
    static std::string GetName(int index)
    {
        auto result = std::string();
        if constexpr (urd::is_store<T>)
            result = "Store_" + std::string(name<typename T::representation>::id);
        else
            result = std::string(name<T>::id);
        return result + "_" + std::to_string(index);
    }
};

}  // namespace urd::test
