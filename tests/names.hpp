#pragma once

// Names of the containers a Store can resolve to, read from their synopses (urd::container_synopsis), and the
// vocabulary as the tests and generated files spell it.

#include "urd/container_synopsis.hpp"
#include "urd/store.hpp"

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
/// urd/tag_synopsis.hpp): for printing a request or a spec, and for share/urd/vocabulary.json
/// (tests/vocabulary_table.cpp).
inline constexpr std::array<std::string_view, urd::detail::tag_count> tag_names = urd::detail::tag_names;
using urd::tag_name;

/// A category to the one who writes a request: its group, its reading heading (semantics only) and the question its
/// tags answer (urd::tag_synopsis of its root). Whether a request may state its tags comes from the code
/// (urd::detail::statable).
struct category_entry
{
    std::string_view group;
    std::string_view heading;
    std::string_view question;
};
template<typename Tag>
constexpr category_entry category_of()
{
    using words = urd::tag_synopsis<Tag>;
    if constexpr (!requires { words::question; })
        return {};
    else if constexpr (requires { typename words::heading; })
        return { words::group::id, words::heading::id, words::question };
    else
        return { words::group::id, {}, words::question };
}
template<typename... Tags>
constexpr auto categories_of(urd::detail::type_list<Tags...> /*tags*/)
{ return std::array<category_entry, sizeof...(Tags)> { category_of<Tags>()... }; }
inline constexpr std::array<category_entry, urd::detail::tag_count> category_vocabulary = categories_of(urd::detail::all_tags {});

/// What a tag is to the one who writes a request: who keeps it (semantics only), its description and phrases
/// (urd::tag_synopsis), and its role, derived: kind in the kind group; demand where the caller promises it; hint in the
/// hints group; else guarantee where a container declares its category (guaranteed_roots), request where only a
/// request states it. A category's root has an empty entry: it promises nothing and is never written.
struct tag_entry
{
    std::string_view role;
    std::string_view promise;
    std::string_view description;
    std::array<std::string_view, 4> phrases;
};
constexpr std::string_view role_of(urd::tag of, std::string_view group, std::string_view promise)
{
    if (group == "kind")
        return "kind";
    if (promise == "caller")
        return "demand";
    if (group == "hints")
        return "hint";
    return urd::detail::in(urd::detail::root_of(of), urd::detail::guaranteed_roots) ? "guarantee" : "request";
}
template<typename Tag>
constexpr tag_entry entry_of()
{
    using words = urd::tag_synopsis<Tag>;
    auto result = tag_entry {};
    if constexpr (requires { words::description; })
    {
        if constexpr (requires { words::promise; })
            result.promise = words::promise;
        result.description = words::description;
        for (std::size_t each = 0; each < words::phrases.size(); ++each)
            result.phrases[each] = words::phrases[each];
        const auto& category = category_vocabulary[urd::detail::index(urd::detail::root_of(Tag::value))];
        result.role = role_of(Tag::value, category.group, result.promise);
    }
    return result;
}
template<typename... Tags>
constexpr auto entries_of(urd::detail::type_list<Tags...> /*tags*/)
{ return std::array<tag_entry, sizeof...(Tags)> { entry_of<Tags>()... }; }
inline constexpr std::array<tag_entry, urd::detail::tag_count> tag_vocabulary = entries_of(urd::detail::all_tags {});

/// The groups, by what leaving out a needed tag causes (urd::all_groups), and the reading headings of semantics.
struct group_entry
{
    std::string_view id;
    std::string_view title;
    std::string_view test;
    std::string_view summary;
    std::string_view literature;
};
inline constexpr auto group_vocabulary = []<typename... Groups>(std::type_identity<std::tuple<Groups...>> /*groups*/)
{
    return std::array<group_entry, sizeof...(Groups)> { group_entry { Groups::id, Groups::title, Groups::test, Groups::summary, Groups::literature }... };
}(std::type_identity<urd::all_groups> {});
struct heading_entry
{
    std::string_view id;
    std::string_view title;
};
inline constexpr auto heading_vocabulary = []<typename... Headings>(std::type_identity<std::tuple<Headings...>> /*headings*/)
{ return std::array<heading_entry, sizeof...(Headings)> { heading_entry { Headings::id, Headings::title }... }; }(std::type_identity<urd::all_headings> {});

// Every tag has its words: a name, and either a category's group and question (a root) or a description (any other
// tag); a heading exactly for the categories of semantics, a promise exactly for its tags. A tag added without them
// fails here.
static_assert(
    []
    {
        for (std::size_t each = 0; each < urd::detail::tag_count; ++each)
        {
            const auto of = static_cast<urd::tag>(each);
            const bool root = urd::detail::root_of(of) == of;
            const auto& category = category_vocabulary[urd::detail::index(urd::detail::root_of(of))];
            const bool semantics = category.group == "semantics";
            if (tag_names[each].empty() || tag_vocabulary[each].description.empty() != root || category_vocabulary[each].question.empty() == root
                || category.group.empty() || category.heading.empty() == semantics || (!root && tag_vocabulary[each].promise.empty() == semantics))
                return false;
        }
        return true;
    }(),
    "urd::tag_synopsis: every tag has a name; a root a group and a question, and a heading in semantics; any other tag a "
    "description and phrases, and a promise in semantics");
static_assert(tag_name(urd::tag::sorted) == "sorted" && tag_vocabulary[urd::detail::index(urd::tag::monotone_priority)].role == "demand"
              && tag_vocabulary[urd::detail::index(urd::tag::small)].role == "hint" && tag_vocabulary[urd::detail::index(urd::tag::fifo)].role == "kind"
              && tag_vocabulary[urd::detail::index(urd::tag::frozen)].role == "request"
              && category_vocabulary[urd::detail::index(urd::tag::single_threaded)].group == "semantics"
              && category_vocabulary[urd::detail::index(urd::tag::any_clear)].group == "costs");

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
