#pragma once
// urd::clone: a deep copy of a builder (or a record), its children with it. A built or held child is owned by its
// handle (a std::unique_ptr), so a builder that holds one is move-only; clone copies it into a slot of its own of the
// same pool, and the builder's other members as they are. Copies are explicit: a builder is never copied implicitly.
#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/handle.hpp"
#include "urd/elements/view.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace urd
{

namespace detail
{
/// Whether a V is copied as it is: it is copyable, and so is everything it holds (std::vector reports itself copyable
/// whatever its elements; a builder or record is copied as its fields are).
template<typename V>
struct copied
{
    static constexpr bool value = std::copy_constructible<V>;
};

template<typename E, typename A>
struct copied<std::vector<E, A>>
{
    static constexpr bool value = copied<E>::value;
};

template<typename E>
struct copied<std::optional<E>>
{
    static constexpr bool value = copied<E>::value;
};

template<typename A, typename B>
struct copied<std::pair<A, B>>
{
    static constexpr bool value = copied<A>::value && copied<B>::value;
};

template<typename... Vs>
struct copied<std::variant<Vs...>>
{
    static constexpr bool value = (copied<Vs>::value && ...);
};

/// A record's flat containers, as their std counterparts (a flat vector, too, reports itself copyable whatever its
/// elements).
template<typename E>
struct copied<flat_vector<E>>
{
    static constexpr bool value = copied<E>::value;
};

template<typename E>
struct copied<flat_optional<E>>
{
    static constexpr bool value = copied<E>::value;
};

template<typename A, typename B>
struct copied<flat_pair<A, B>>
{
    static constexpr bool value = copied<A>::value && copied<B>::value;
};

template<typename... Vs>
struct copied<flat_variant<Vs...>>
{
    static constexpr bool value = (copied<Vs>::value && ...);
};

template<typename R>
consteval bool fields_copied()
{
    return []<typename... Fs>(Fields<Fs...> /*fields*/)
    { return (copied<std::remove_cvref_t<decltype(std::declval<const R&>().*Fs::member)>>::value && ...); }(R::fields());
}

template<typename T, typename C>
struct copied<builder<T, C>>
{
    static constexpr bool value = std::copy_constructible<builder<T, C>> && fields_copied<builder<T, C>>();
};

template<typename T>
struct copied<data<T>>
{
    static constexpr bool value = std::copy_constructible<data<T>> && fields_copied<data<T>>();
};
}  // namespace detail

/// Whether a V is copied as it is (detail::copied): what clone copies; anything else it clones member by member.
template<typename V>
concept CopiedAsIs = detail::copied<V>::value;

/// A copyable value: copied (an interned child, a key, a builder or record of copyable members).
template<CopiedAsIs V>
V clone(const V& value);

/// A builder that holds a child of its own: each field cloned, its index as it is.
template<typename T, typename C>
    requires(!CopiedAsIs<builder<T, C>>)
builder<T, C> clone(const builder<T, C>& built);

/// A move-only record: each field cloned, its index as it is.
template<typename T>
    requires(!CopiedAsIs<data<T>>)
data<T> clone(const data<T>& record);

/// std's containers of what is not copyable, element by element.
template<typename E>
    requires(!CopiedAsIs<E>)
std::vector<E> clone(const std::vector<E>& values);

template<typename E>
    requires(!CopiedAsIs<E>)
std::optional<E> clone(const std::optional<E>& value);

template<typename A, typename B>
    requires(!CopiedAsIs<std::pair<A, B>>)
std::pair<A, B> clone(const std::pair<A, B>& value);

/// A variant (a child: its form kept), its alternative cloned.
template<typename... Vs>
    requires(!CopiedAsIs<std::variant<Vs...>>)
std::variant<Vs...> clone(const std::variant<Vs...>& value);

/// A record's flat containers of what is not copyable (move-only records), element by element, as std's.
template<typename E>
    requires(!CopiedAsIs<E>)
flat_vector<E> clone(const flat_vector<E>& values);

template<typename E>
    requires(!CopiedAsIs<E>)
flat_optional<E> clone(const flat_optional<E>& value);

template<typename A, typename B>
    requires(!CopiedAsIs<flat_pair<A, B>>)
flat_pair<A, B> clone(const flat_pair<A, B>& value);

template<typename... Vs>
    requires(!CopiedAsIs<flat_variant<Vs...>>)
flat_variant<Vs...> clone(const flat_variant<Vs...>& value);

/// A view of a record held outside it (a borrowed view: valid while its record is), as a view owning a clone of that
/// record: kept beyond it, read through the same context.
template<typename T, typename C, typename R>
owning_view<T, C, R> clone(const borrowed_view<T, C, R>& view);

namespace detail
{
/// Each field of `source` (a record or a builder of T) cloned into `target`, its index as it is where T's records hold
/// their key (SelfIndexed).
template<typename T, typename R>
void clone_fields(R& target, const R& source)
{
    if constexpr (SelfIndexed<T>)
        target.index = source.index;
    for_each_field<R>([&]<typename F>() { target.*F::member = urd::clone(source.*F::member); });
}
}  // namespace detail

template<CopiedAsIs V>
V clone(const V& value)
{ return value; }

template<typename T, typename C>
    requires(!CopiedAsIs<builder<T, C>>)
builder<T, C> clone(const builder<T, C>& built)
{
    auto result = builder<T, C>();
    detail::clone_fields<T>(result, built);
    return result;
}

template<typename T>
    requires(!CopiedAsIs<data<T>>)
data<T> clone(const data<T>& record)
{
    auto result = data<T>();
    detail::clone_fields<T>(result, record);
    return result;
}

template<typename E>
    requires(!CopiedAsIs<E>)
std::vector<E> clone(const std::vector<E>& values)
{
    auto result = std::vector<E>();
    result.reserve(values.size());
    for (const auto& value : values)
        result.push_back(urd::clone(value));
    return result;
}

template<typename E>
    requires(!CopiedAsIs<E>)
std::optional<E> clone(const std::optional<E>& value)
{
    if (!value.has_value())
        return std::nullopt;
    return std::optional<E>(urd::clone(*value));
}

template<typename A, typename B>
    requires(!CopiedAsIs<std::pair<A, B>>)
std::pair<A, B> clone(const std::pair<A, B>& value)
{ return std::pair<A, B>(urd::clone(value.first), urd::clone(value.second)); }

template<typename... Vs>
    requires(!CopiedAsIs<std::variant<Vs...>>)
std::variant<Vs...> clone(const std::variant<Vs...>& value)
{
    return std::visit([]<typename V>(const V& alternative) { return std::variant<Vs...>(std::in_place_type<V>, urd::clone(alternative)); }, value);
}

template<typename E>
    requires(!CopiedAsIs<E>)
flat_vector<E> clone(const flat_vector<E>& values)
{
    auto result = flat_vector<E>();
    result.reserve(values.size());
    for (const auto& value : values)
        result.emplace_back(urd::clone(value));
    return result;
}

template<typename E>
    requires(!CopiedAsIs<E>)
flat_optional<E> clone(const flat_optional<E>& value)
{
    if (!value.has_value())
        return flat_optional<E>();
    return flat_optional<E>(urd::clone(*value));
}

template<typename A, typename B>
    requires(!CopiedAsIs<flat_pair<A, B>>)
flat_pair<A, B> clone(const flat_pair<A, B>& value)
{ return flat_pair<A, B> { urd::clone(value.first), urd::clone(value.second) }; }

template<typename... Vs>
    requires(!CopiedAsIs<flat_variant<Vs...>>)
flat_variant<Vs...> clone(const flat_variant<Vs...>& value)
{ return value.apply([](const auto& alternative) { return flat_variant<Vs...>(urd::clone(alternative)); }); }

template<typename R>
handle<R> clone(const handle<R>& held)
{
    if (held == nullptr)
        return handle<R>();
    const auto& release = held.get_deleter();
    if (release.acquire == nullptr)
        throw std::logic_error("urd::clone: a handle of a Store of keyed slots is not cloned (its slot is drawn under a key)");
    auto slot = std::uint64_t { 0 };
    auto* record = static_cast<R*>(release.acquire(release.store, slot));
    auto result = handle<R>(record, urd::release { release.store, slot, release.give_back, release.acquire });
    *result = urd::clone(*held);
    return result;
}

template<typename T, typename C, typename R>
owning_view<T, C, R> clone(const borrowed_view<T, C, R>& view)
{ return owning_view<T, C, R>(urd::clone(detail::view_access::record(view)), &view.context()); }

}  // namespace urd
