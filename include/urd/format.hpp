#pragma once

// How urd's types are written, with fmt (urd links it with URD_ENABLE_FMT_FORMATTERS, on by default): a type as a
// request names it (`int`, `std::string`, `index<Id>`, `Store<sequence<int>, frozen>`), a spec as the request that states
// its tags (`Store<set<int>, sorted, unique>`), a cost as the docs
// write it (`O(log n)*~`), and a tag by its name. Another type is written as its urd::type_spelling says: specialize it
// for an element type of your own; otherwise its demangled name. Values are written too: an index as its number, a
// bitset span as the positions of its set bits.
//
// Types are written by functions (urd::spelling<T>(), urd::spelling_of_spec<S>()), not by formatting an object: an
// object whose type names a Store would complete that Store, and writing a request completes nothing.

#include "urd/bits.hpp"
#include "urd/cost.hpp"
#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/data.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/view.hpp"
#include "urd/spec.hpp"
#include "urd/traits.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
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

/// A view of an interned record, by its tag and its key (its context, a repository's whole type, says nothing a reader
/// needs, and the demangler may give up on it).
template<typename T, typename C, typename K>
struct type_spelling<index_view<T, C, K>>
{
    static std::string spelled() { return fmt::format("index_view<{}, {}>", spelling<T>(), spelling<K>()); }
};

/// A key of several parts, by its parts (the demangler gives up on some of them, and writes the whole key mangled).
template<typename... Ts>
struct type_spelling<key<Ts...>>
{
    static std::string spelled() { return fmt::format("key<{}>", fmt::join(std::vector<std::string> { spelling<Ts>()... }, ", ")); }
};

template<typename T, typename A>
struct type_spelling<std::vector<T, A>>
{
    static std::string spelled() { return fmt::format("std::vector<{}>", spelling<T>()); }
};

template<typename Kind, typename... Tags>
struct type_spelling<Store<Kind, Tags...>>
{
    static std::string spelled() { return spelling_of_spec<typename detail::to_spec<Kind, Tags...>::type>(); }
};

/// Type T as a request names it.
template<typename T>
std::string spelling()
{ return type_spelling<T>::spelled(); }

/// Spec S as the request that states its tags: its kind with its element types (set<int>), then the tags that say
/// more than their category's none, in the order of the categories.
template<typename S>
std::string spelling_of_spec()
{
    auto types = std::vector<std::string>();
    [&types]<typename... Types>(boost::mp11::mp_list<Types...> /*types*/) { (types.push_back(spelling<Types>()), ...); }(typename S::element_types {});
    auto parts = std::vector<std::string> { fmt::format("{}<{}>", S::kind::name, fmt::join(types, ", ")) };
    S::urd_properties.for_each([&parts](tag each) { parts.emplace_back(tag_name(each)); });
    return fmt::format("Store<{}>", fmt::join(parts, ", "));
}

/// The name of tag T: its `name` (a static constexpr std::string_view), as the tag declares it. A tag of a family that is
/// named after its template arguments says so itself (urd::joined_name: Atom<LiftedTag, FluentTag> as
/// "AtomLiftedFluent"); urd adds nothing. A record is written (fmt) and bound in Python under it.
template<typename T>
std::string name()
{ return std::string(T::name); }

namespace detail
{
/// The indentation of what is written on this thread: 4 spaces per scoped_indent open around it.
inline int& indent_level() noexcept
{
    thread_local int level = 0;
    return level;
}

/// A value as a record's field writes it: an optional as its value or `none`, a variant as its alternative, a string as
/// its characters, a range as `[`, each element written so on a line of its own one level deeper, `]` (`[]` empty), and
/// anything else as fmt writes it.
template<typename V>
std::string field_written(const V& value);
}  // namespace detail

/// A new line at the current indentation (scoped_indent): what a formatter writes to begin a line of its own.
inline std::string new_line() { return "\n" + std::string(static_cast<std::size_t>(4 * detail::indent_level()), ' '); }

/// Indentation one level deeper while it lives: what urd writes inside it (a record's fields, a list's elements) is
/// indented by 4 more spaces, on this thread.
class scoped_indent
{
public:
    scoped_indent() noexcept { ++detail::indent_level(); }

    scoped_indent(const scoped_indent&) = delete;
    scoped_indent& operator=(const scoped_indent&) = delete;
    scoped_indent(scoped_indent&&) = delete;
    scoped_indent& operator=(scoped_indent&&) = delete;

    ~scoped_indent() { --detail::indent_level(); }
};

namespace detail
{
template<typename V>
std::string field_written(const V& value)
{
    if constexpr (optional_value<V>)
        return value.has_value() ? field_written(*value) : std::string("none");
    else if constexpr (variant_value<V>)
        return visit([](const auto& alternative) { return field_written(alternative); }, value);
    else if constexpr (std::convertible_to<const V&, std::string_view>)
        return std::string(std::string_view(value));
    else if constexpr (std::ranges::input_range<const V>)
    {
        if (std::ranges::empty(value))
            return "[]";
        auto out = std::string("[");
        {
            const auto deeper = scoped_indent();
            for (const auto& element : value)
                out += new_line() + field_written(element);
        }
        return out + new_line() + "]";
    }
    else
        return fmt::format("{}", value);
}

}  // namespace detail

/// A record of tag T as urd writes one: `Name(`, each field of Record (data<T>, or builder<T, C>) on a line of its own one
/// level deeper, `name=value`, its value read by read.template operator()<F>() and written as a field's (an optional, a
/// variant, a list), then `)`; `Name()` without fields. A library calls it for a view that reads its fields otherwise (a
/// packed record).
template<typename T, typename Record, typename Read>
std::string record_written(Read read)
{
    // Without fields the per-field lambda is never called: GCC 13 under LTO then streams it so that it cannot read it
    // back (an ICE in lto_tag_to_tree_code at the link, or in its modref pass compiling).
    if constexpr (std::same_as<decltype(Record::fields()), Fields<>>)
        return name<T>() + "()";
    else
    {
        auto out = name<T>() + "(";
        {
            const auto deeper = scoped_indent();
            for_each_field<Record>([&]<typename F>() { out += new_line() + F::c_name + "=" + detail::field_written(read.template operator()<F>()); });
        }
        return out + new_line() + ")";
    }
}

namespace detail
{

/// The tag a value with fields is named after: T for a record data<T> or a builder builder<T, C>, the value itself otherwise.
template<typename V>
struct named
{
    using type = V;
};

template<typename T>
struct named<data<T>>
{
    using type = T;
};

template<typename T, typename C>
struct named<builder<T, C>>
{
    using type = T;
};

template<typename V>
using named_t = typename named<V>::type;

/// A view V written as its record (record_written), each field resolved through the view's context.
template<typename V>
struct view_formatter
{
    constexpr auto parse(fmt::format_parse_context& context) { return context.begin(); }

    auto format(const V& view, fmt::format_context& context) const
    {
        return fmt::format_to(
            context.out(),
            "{}",
            record_written<typename V::value_type, record_of<V>>([&]<typename F>() -> decltype(auto) { return view.template member<F::member>(); }));
    }
};
}  // namespace detail

}  // namespace urd

/// A value that names its fields (fields()), `Name(\n    field=value\n    ...\n)`: a record (data<T>) or a builder
/// (builder<T, C>) under the name of its tag T, any other value under its own name (urd::name), its fields in order. A
/// library that writes such a value otherwise specializes this for it.
template<typename V>
    requires urd::HasFields<V>
struct fmt::formatter<V>
{
    constexpr auto parse(fmt::format_parse_context& context) { return context.begin(); }

    auto format(const V& value, fmt::format_context& context) const
    {
        return fmt::format_to(context.out(),
                              "{}",
                              urd::record_written<urd::detail::named_t<V>, V>([&]<typename F>() -> decltype(auto) { return value.*F::member; }));
    }
};

/// A view of a record or a builder whose fields its record names, written as that record (each field resolved through
/// the view's context). A library that writes a view otherwise specializes this for its own record.
template<typename T, typename C, typename K>
    requires urd::HasFields<urd::record_of<urd::index_view<T, C, K>>>
struct fmt::formatter<urd::index_view<T, C, K>> : urd::detail::view_formatter<urd::index_view<T, C, K>>
{
};

template<typename T, typename C, typename R>
    requires urd::HasFields<R>
struct fmt::formatter<urd::borrowed_view<T, C, R>> : urd::detail::view_formatter<urd::borrowed_view<T, C, R>>
{
};

template<typename T, typename C, typename R>
    requires urd::HasFields<R>
struct fmt::formatter<urd::owning_view<T, C, R>> : urd::detail::view_formatter<urd::owning_view<T, C, R>>
{
};

template<typename T, typename C, typename R>
    requires urd::HasFields<R>
struct fmt::formatter<urd::shared_view<T, C, R>> : urd::detail::view_formatter<urd::shared_view<T, C, R>>
{
};

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

/// An index, as its number.
template<typename T>
struct fmt::formatter<urd::index<T>, char>
{
    constexpr auto parse(fmt::format_parse_context& context) { return context.begin(); }

    template<typename FormatContext>
    auto format(const urd::index<T>& at, FormatContext& context) const
    { return fmt::format_to(context.out(), "{}", at.value); }
};

/// A key, as its parts' numbers: <first,second,...>.
template<typename... Ts>
struct fmt::formatter<urd::key<Ts...>, char>
{
    constexpr auto parse(fmt::format_parse_context& context) { return context.begin(); }

    template<typename FormatContext>
    auto format(const urd::key<Ts...>& at, FormatContext& context) const
    {
        auto out = fmt::format_to(context.out(), "<");
        auto first = true;
        ((out = fmt::format_to(out, "{}{}", std::exchange(first, false) ? "" : ",", at.template get<Ts>().value)), ...);
        return fmt::format_to(out, ">");
    }
};

/// A flat pair, as (first, second): cista's pair is not tuple-like, so fmt's tuple formatter does not take it.
template<typename T1, typename T2>
struct fmt::formatter<urd::flat_pair<T1, T2>, char>
{
    constexpr auto parse(fmt::format_parse_context& context) { return context.begin(); }

    template<typename FormatContext>
    auto format(const urd::flat_pair<T1, T2>& pair, FormatContext& context) const
    { return fmt::format_to(context.out(), "({}, {})", pair.first, pair.second); }
};

/// A bitset span, as the positions of its set bits: {p, q, ...}.
template<std::unsigned_integral Block>
struct fmt::formatter<urd::bitset_span<Block>, char>
{
    constexpr auto parse(fmt::format_parse_context& context) { return context.begin(); }

    template<typename FormatContext>
    auto format(const urd::bitset_span<Block>& bits, FormatContext& context) const
    { return fmt::format_to(context.out(), "{{{}}}", fmt::join(urd::set_bit_indices(bits), ", ")); }
};
