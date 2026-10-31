#pragma once

// How urd's types are written, with fmt (urd links it with URD_ENABLE_FMT_FORMATTERS, on by default): a type as a
// request names it (`int`, `std::string`, `index<Id>`, `Store<value<int>, frozen>`), a spec as the request that states
// its tags (`Store<key<int>, sorted, unique>`), a request as written (urd/request.hpp), a cost as the docs
// write it (`O(log n)*~`), and a tag by its name. Another type is written as its urd::type_spelling says: specialize it
// for an element type of your own; otherwise its demangled name.
//
// Types are written by functions (urd::spelling<T>(), urd::spelling_of_spec<S>()), not by formatting an object: an
// object whose type names a Store would complete that Store, and writing a request completes nothing.

#include "urd/costs.hpp"
#include "urd/index.hpp"
#include "urd/properties.hpp"
#include "urd/semantics/concepts.hpp"
#include "urd/traits.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <vector>
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

namespace urd
{

/// How type T is written: `static std::string spelled()`. Specialize it for an element type of your own; the primary
/// writes the demangled name.
template<typename T>
struct type_spelling
{
    static std::string spelled()
    {
#if __has_include(<cxxabi.h>)
        int status = 0;
        const auto freed = std::unique_ptr<char, void (*)(void*)>(abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status), std::free);
        if (status == 0)
            return std::string(freed.get());
#endif
        return typeid(T).name();
    }
};

template<typename S>
std::string spelling_of_spec();

namespace detail
{

template<typename T>
inline constexpr bool is_std_vector = false;
template<typename T, typename A>
inline constexpr bool is_std_vector<std::vector<T, A>> = true;

}  // namespace detail

/// Type T as a request names it; "-" for none (void).
template<typename T>
std::string spelling()
{
    if constexpr (std::is_void_v<T>)
        return "-";
    else if constexpr (std::is_same_v<T, int>)
        return "int";
    else if constexpr (std::is_same_v<T, unsigned>)
        return "unsigned";
    else if constexpr (std::is_same_v<T, std::size_t>)
        return "std::size_t";
    else if constexpr (std::is_same_v<T, double>)
        return "double";
    else if constexpr (std::is_same_v<T, std::string>)
        return "std::string";
    else if constexpr (is_index<T>)
        return fmt::format("index<{}>", spelling<typename T::element_type>());
    else if constexpr (detail::is_std_vector<T>)
        return fmt::format("std::vector<{}>", spelling<typename T::value_type>());
    else if constexpr (is_store<T>)
        return spelling_of_spec<traits_t<T>>();
    else
        return type_spelling<T>::spelled();
}

/// Spec S as the request that states its tags: its key and value types, then the tags that say more than their
/// category's root, in the order of urd::tag.
template<typename S>
std::string spelling_of_spec()
{
    auto parts = std::vector<std::string>();
    if constexpr (!std::is_void_v<typename S::key_type>)
        parts.push_back(fmt::format("key<{}>", spelling<typename S::key_type>()));
    if constexpr (!std::is_void_v<typename S::value_type>)
        parts.push_back(fmt::format("value<{}>", spelling<typename S::value_type>()));
    for (std::size_t each = 0; each < detail::tag_count; ++each)
    {
        const auto root = static_cast<tag>(each);
        if (detail::root_of(root) == root && S::urd_properties.in(root) != root)
            parts.emplace_back(tag_name(S::urd_properties.in(root)));
    }
    if constexpr (!std::is_void_v<typename S::order_type>)
        parts.emplace_back("order_by<P>");
    return fmt::format("Store<{}>", fmt::join(parts, ", "));
}

namespace detail
{

template<typename P>
std::string spelling_of_stated()
{
    if constexpr (a_tag<P>)
        return std::string(tag_name(P::value));
    else if constexpr (is_a<key, P>)
        return fmt::format("key<{}>", spelling<typename P::type>());
    else if constexpr (is_a<value, P>)
        return fmt::format("value<{}>", spelling<typename P::type>());
    else
        return "order_by<P>";
}

template<typename List>
struct stated_spelling;
template<typename... Ps>
struct stated_spelling<type_list<Ps...>>
{
    static std::string spelled() { return fmt::format("Store<{}>", fmt::join(std::array<std::string, sizeof...(Ps)> { spelling_of_stated<Ps>()... }, ", ")); }
};

}  // namespace detail

/// Request Q (urd::request) as it is written: Store<key<int>, sorted>, its parameters in the order stated.
template<typename Q>
std::string spelling_of_request()
{ return detail::stated_spelling<typename Q::stated>::spelled(); }

}  // namespace urd

/// A cost as the docs write it: O(1), O(log n), O(n log n), O(n changes), with * for amortized and ~ for expected.
template<>
struct fmt::formatter<urd::cost> : fmt::formatter<std::string_view>
{
    auto format(const urd::cost& how, fmt::format_context& context) const
    {
        const auto of = urd::symbol(how.of);
        auto out = context.out();
        switch (how.grows)
        {
            case urd::growth::one:
                out = fmt::format_to(out, "O(1)");
                break;
            case urd::growth::log:
                out = fmt::format_to(out, "O(log {})", of);
                break;
            case urd::growth::linear:
                out = fmt::format_to(out, "O({})", of);
                break;
            case urd::growth::n_log_n:
                out = fmt::format_to(out, "O({0} log {0})", of);
                break;
            case urd::growth::n_times:
                out = fmt::format_to(out, "O(n {})", of);
                break;
        }
        return fmt::format_to(out, "{}{}", how.amortized ? "*" : "", how.expected ? "~" : "");
    }
};

/// A tag, by its name.
template<>
struct fmt::formatter<urd::tag> : fmt::formatter<std::string_view>
{
    auto format(urd::tag of, fmt::format_context& context) const { return fmt::formatter<std::string_view>::format(urd::tag_name(of), context); }
};
