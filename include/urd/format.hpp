#pragma once

// How urd's types are written, with fmt (urd links it with URD_ENABLE_FMT_FORMATTERS, on by default): a type as a
// request names it (`int`, `std::string`, `index<Id>`, `Store<sequence<int>, frozen>`), a spec as the request that states
// its tags (`Store<set<int>, sorted, unique>`), a cost as the docs
// write it (`O(log n)*~`), and a tag by its name. Another type is written as its urd::type_spelling says: specialize it
// for an element type of your own; otherwise its demangled name.
//
// Types are written by functions (urd::spelling<T>(), urd::spelling_of_spec<S>()), not by formatting an object: an
// object whose type names a Store would complete that Store, and writing a request completes nothing.

#include "urd/costs.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/index.hpp"
#include "urd/spec.hpp"
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
template<typename T>
std::string spelling();

// How urd's own types and the standard ones a request names are written, where the demangled name says more.
template<>
struct type_spelling<unsigned>
{
    static std::string spelled() { return "unsigned"; }
};
template<>
struct type_spelling<std::size_t>
{
    static std::string spelled() { return "std::size_t"; }
};
template<>
struct type_spelling<std::string>
{
    static std::string spelled() { return "std::string"; }
};
template<typename T>
struct type_spelling<index<T>>
{
    static std::string spelled() { return fmt::format("index<{}>", spelling<T>()); }
};
template<typename T, typename A>
struct type_spelling<std::vector<T, A>>
{
    static std::string spelled() { return fmt::format("std::vector<{}>", spelling<T>()); }
};
template<typename R, typename S>
struct type_spelling<detail::Store<R, S>>
{
    static std::string spelled() { return spelling_of_spec<S>(); }
};

/// Type T as a request names it; "-" for none (void).
template<typename T>
std::string spelling()
{
    if constexpr (std::is_void_v<T>)
        return "-";
    else
        return type_spelling<T>::spelled();
}

/// Spec S as the request that states its tags: its kind with its element types (set<int>), then the tags that say
/// more than their category's none, in the order of the categories.
template<typename S>
std::string spelling_of_spec()
{
    auto types = std::vector<std::string>();
    [&types]<typename... Types>(detail::type_list<Types...> /*types*/) { (types.push_back(spelling<Types>()), ...); }(typename S::element_types {});
    auto parts = std::vector<std::string> { fmt::format("{}<{}>", S::kind::name, fmt::join(types, ", ")) };
    S::urd_properties.for_each([&parts](tag each) { parts.emplace_back(tag_name(each)); });
    return fmt::format("Store<{}>", fmt::join(parts, ", "));
}

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
