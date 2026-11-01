#pragma once

// Names of the containers a Store can resolve to, read from their synopses (urd::container_synopsis), and the
// vocabulary as the tests and generated files spell it.

#include "urd/registry/derived.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace urd::test
{

/// An empty S as its builder gives it: numbered (urd::layer) where S hands out indices for views, else default-built.
template<typename S>
S make_empty(std::size_t number = 0)
{
    if constexpr (std::constructible_from<S, urd::layer>)
        return S(urd::layer { number });
    else
        return S();
}

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

/// The vocabulary as arrays by category, then by answer, computed from each tag's words (beside the tag, urd/tags.hpp):
/// for printing a request or a spec, and for share/urd/vocabulary.json (tests/vocabulary_table.cpp).
using urd::tag_name;
using urd::detail::most_answers;

/// urd's categories, in the order its documentation and share/urd/vocabulary.json list them; every kind's request
/// format (urd/kinds.hpp) answers only these.
using vocabulary_categories = urd::detail::request_format<urd::order,
                                                          urd::uniqueness,
                                                          urd::keys,
                                                          urd::referencing,
                                                          urd::layout,
                                                          urd::representation,
                                                          urd::threading,
                                                          urd::memory,
                                                          urd::clearing,
                                                          urd::access,
                                                          urd::scoping,
                                                          urd::layering,
                                                          urd::handling,
                                                          urd::sharing,
                                                          urd::direction,
                                                          urd::shape,
                                                          urd::packing,
                                                          urd::grouping,
                                                          urd::tracking,
                                                          urd::change,
                                                          urd::emptiness,
                                                          urd::multiplicity,
                                                          urd::position,
                                                          urd::workload,
                                                          urd::priority>;
template<typename... Es>
constexpr auto tables_of(urd::detail::request_format<Es...> /*categories*/)
{ return std::array<const urd::detail::category_table*, sizeof...(Es)> { urd::category<Es>... }; }
inline constexpr auto category_tables = tables_of(vocabulary_categories {});
inline constexpr std::size_t category_count = category_tables.size();
/// The position of a category among vocabulary_categories.
constexpr std::size_t category_index(const urd::detail::category_table* of)
{
    for (std::size_t each = 0; each < category_count; ++each)
        if (category_tables[each] == of)
            return each;
    throw "urd: a category outside the vocabulary (tests/names.hpp, vocabulary_categories)";
}
/// Every tag of the vocabulary, each category's none first.
template<typename F>
constexpr void for_each_tag(F f)
{
    for (const auto* each : category_tables)
        for (std::uint8_t answer = 0; answer < each->answers; ++answer)
            f(urd::tag(each, answer));
}
/// urd's type traits (urd::trait), in the order share/urd/complexity.json lists them.
inline constexpr std::array<const urd::trait_info*, 13> vocabulary_traits {
    urd::trait_v<urd::trait::can_hash>,           urd::trait_v<urd::trait::an_index>,        urd::trait_v<urd::trait::copies_as_bytes>,
    urd::trait_v<urd::trait::destroys_trivially>, urd::trait_v<urd::trait::names_members>,   urd::trait_v<urd::trait::whole_value>,
    urd::trait_v<urd::trait::own_order>,          urd::trait_v<urd::trait::natural_ordered>, urd::trait_v<urd::trait::by_entry>,
    urd::trait_v<urd::trait::integral_priority>,  urd::trait_v<urd::trait::rows_flatten>,    urd::trait_v<urd::trait::rows_in_place>,
    urd::trait_v<urd::trait::rows_reused>
};

/// urd's roles (urd::role), in the order share/urd/complexity.json lists them.
inline constexpr std::array<const urd::role_info*, 6> vocabulary_roles { urd::role_v<urd::role::element>, urd::role_v<urd::role::order>,
                                                                         urd::role_v<urd::role::rows>,    urd::role_v<urd::role::mapped>,
                                                                         urd::role_v<urd::role::row>,     urd::role_v<urd::role::row_element> };

/// Every kind's format answers only the vocabulary's categories.
template<typename... Es>
constexpr bool within_vocabulary(urd::detail::request_format<Es...> /*format*/)
{ return ((category_index(urd::category<Es>) < category_count) && ...); }
static_assert([]<typename... Kinds>(urd::detail::type_list<Kinds...> /*kinds*/)
              { return (within_vocabulary(urd::detail::format_of<Kinds> {}) && ...); }(urd::leaf_kinds<> {}));

/// A category to the one who writes a request: its group, its reading heading (semantics only) and the question its
/// tags answer (the words of its none).
struct category_entry
{
    std::string_view group;
    std::string_view heading;
    std::string_view question;
};
template<typename Root, typename... Tags>
constexpr category_entry category_of(urd::detail::type_list<Root, Tags...> /*tags*/)
{ return { Root::group::id, Root::heading::id, Root::question }; }
template<typename... Es>
constexpr auto categories_of(urd::detail::request_format<Es...> /*categories*/)
{ return std::array<category_entry, sizeof...(Es)> { category_of(typename urd::tags_of<Es>::type {})... }; }
inline constexpr std::array<category_entry, category_count> category_vocabulary = categories_of(vocabulary_categories {});
constexpr const category_entry& category_entry_of(urd::tag of) { return category_vocabulary[category_index(of.in)]; }

/// What a tag is to the one who writes a request: who keeps it (semantics only), its description and phrases
/// (urd/tags.hpp), and its role, derived: kind in the kind group; demand where the caller promises it; hint in the
/// hints group; else guarantee where a container declares its category, request where only a request states it
/// (request_only). A category's none has an empty entry: it promises nothing and is never written.
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
    return of.in->request_only ? "request" : "guarantee";
}
template<typename Tag>
constexpr tag_entry entry_of()
{
    auto result = tag_entry {};
    if constexpr (!std::is_same_v<typename Tag::root, Tag>)
    {
        result.promise = Tag::promise;
        result.description = Tag::description;
        for (std::size_t each = 0; each < Tag::phrases.size(); ++each)
            result.phrases[each] = Tag::phrases[each];
        result.role = role_of(Tag::value, category_entry_of(Tag::value).group, result.promise);
    }
    return result;
}
template<typename... Tags>
constexpr std::array<tag_entry, most_answers> entries_of(urd::detail::type_list<Tags...> /*tags*/)
{ return { entry_of<Tags>()... }; }
template<typename... Es>
constexpr auto all_entries_of(urd::detail::request_format<Es...> /*categories*/)
{ return std::array<std::array<tag_entry, most_answers>, sizeof...(Es)> { entries_of(typename urd::tags_of<Es>::type {})... }; }
inline constexpr auto tag_vocabulary = all_entries_of(vocabulary_categories {});
constexpr const tag_entry& tag_entry_of(urd::tag of) { return tag_vocabulary[category_index(of.in)][of.answer]; }

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

// Every tag has its words: a name, and either a category's group and question (a none) or a description (any other
// tag); a heading exactly for the categories of semantics, a promise exactly for its tags. A tag added without them
// fails here.
static_assert(
    []
    {
        bool fine = true;
        for_each_tag(
            [&fine](urd::tag of)
            {
                const bool root = of.answer == 0;
                const auto& category = category_entry_of(of);
                const bool semantics = category.group == "semantics";
                if (tag_name(of).empty() || tag_entry_of(of).description.empty() != root || category.question.empty() || category.group.empty()
                    || category.heading.empty() == semantics || (!root && tag_entry_of(of).promise.empty() == semantics))
                    fine = false;
            });
        return fine;
    }(),
    "urd/tags.hpp: every tag has a name; a category's none a group and a question, and a heading in semantics; any "
    "other tag a description and phrases, and a promise in semantics");
static_assert(tag_name(urd::order::sorted) == "sorted" && tag_entry_of(urd::priority::monotone_priority).role == "demand"
              && tag_entry_of(urd::workload::small).role == "hint" && tag_entry_of(urd::access::fifo).role == "kind"
              && tag_entry_of(urd::change::frozen).role == "request" && category_entry_of(urd::threading::none).group == "semantics"
              && category_entry_of(urd::clearing::none).group == "costs");

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
