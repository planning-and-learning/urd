#pragma once

#include "urd/elements/builder.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/flat.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/view.hpp"
#include "urd/elements/walk.hpp"
#include "urd/traits.hpp"

#include <boost/mp11/algorithm.hpp>
#include <concepts>
#include <cstddef>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

/// The children of a record of element type T: every key<U> with U in the type list Types (a boost::mp11 list),
/// found anywhere in the record's cista members: directly, in (nested) flat vectors, as a variant's alternative, in an
/// optional or a pair (std's or cista's, as urd::resolution reads them), or inside a member that names cista members
/// itself (urd::Serializable). U may be T: an key<T> nested in any of these shapes is a child (an (and (and ...))).
/// Only a direct member of type key<T> is not: that is the record's own handle. A direct key<T> member that links
/// to another record of type T (a parent) is therefore not a child either; nest it (an optional, say) to make it one.
/// They are handed out as the views their context makes, context.view(index) (urd::resolves), so callers never handle
/// raw indices. urd::map_variant_children instead hands out each variant slot whole (urd::resolve of it), so that a
/// callback may replace it by another alternative.
///
/// A builder's children are its urd::child members (an index, a built builder, or a record not interned yet), found in the
/// same shapes as std's containers and builders held in place: urd::map_children of a builder writes its record with
/// each child replaced by an index, and urd::intern interns a builder so (its built children first).
namespace urd
{

/// How the elements of a record's member Member are read where each is a child or a plain value by its position, as the
/// record and its context say (a row's values: by its relation's column types). Specialized beside the record, with
/// `child_type` (the key a child element holds), `is_child(stored, context, position)` (stored: the record in any of its
/// stored forms, urd::stored_field), `child(element)` (the key an element holds) and `element(child_type)` (an element
/// holding a key). urd's builders read such a member element by element (build_from: a child as the view it names).
template<auto Member>
struct element_children
{
};

/// Whether the elements of member Member are children or plain values by their position (element_children).
template<auto Member>
concept HasElementChildren = requires { typename element_children<Member>::child_type; };

/// Whether T is a record of named fields (data<T>::fields()), which urd's walkers read field by field; else a plain
/// value of its own, read member by member.
template<typename T>
concept RecordFields = requires { data<T>::fields(); };

/// Whether a context makes the views of keys K (context.view(key)): an element child of key K is read through it.
template<typename C, typename K>
concept ViewsKeys = requires(const C& context, K at) { context.view(at); };

/// The built form of a record's member of type M, read through C: the type a builder holds for it, every builder
/// field being the built form of its record's (urd::builder), the mirror of how it is written back (assign_member): a
/// record's key (key<T> of a stored record) a child, a record held in place (data<T>) its builder, a flat container its
/// std counterpart of built forms (flat_vector, flat_optional, flat_pair, flat_variant: std::vector, std::optional,
/// std::pair, std::variant), anything else as it is (a value: a number, a flat_string, a plan, a key of no record). C is
/// the repository that holds the member's records. Each form also says how a record's member is written into it, in
/// place (assign: into a builder's own value, a pooled one, whose buffers are kept: nothing is allocated once they have
/// grown; a key its interned child, a record its builder; the bind_builder setters' conversion), which needs no context,
/// as no child holds one; a library that specializes built_form for a member of its own gives both, or its type alone
/// where the member is made only through its context: then nothing that holds it is made from a record either
/// (made_from_record).
template<typename M, typename C>
struct built_form
{
    using type = M;

    static void assign(const M& value, M& into) { detail::assign_value(value, into); }
};

template<typename M, typename C>
using built_t = typename built_form<M, C>::type;

/// Whether a member of type M is made from a record's (built_form<M, C>::assign): not where its built form, or one of
/// its parts', is made only through its context.
template<typename M, typename C>
concept made_from_record = requires(const M& value, built_t<M, C>& into) { built_form<M, C>::assign(value, into); };

namespace detail
{
/// Whether each field of data<T> is made from a record's.
template<typename T, typename C>
consteval bool fields_made_from_record()
{
    return []<typename... Ds>(Fields<Ds...> /*fields*/) { return (made_from_record<member_type_t<Ds::member>, C> && ...); }(data<T>::fields());
}
}  // namespace detail

template<typename T, typename C>
    requires stored_record<T>
struct built_form<key<T>, C>
{
    using type = child<T, C>;

    static void assign(key<T> value, type& into) { into.template emplace<0>(value); }
};

template<typename T, typename C>
    requires(!PlainValue<T>)
struct built_form<data<T>, C>
{
    using type = builder<T, C>;

    /// Its index as it is, each field from the record's of the same position (a builder's fields are its record's).
    static void assign(const data<T>& value, type& into)
        requires(detail::fields_made_from_record<T, C>())
    {
        if constexpr (SelfIndexed<T>)
            into.index = value.index;
        [&]<typename... Ds, typename... Bs>(Fields<Ds...> /*record*/, Fields<Bs...> /*builder*/)
        { (built_form<member_type_t<Ds::member>, C>::assign(value.*Ds::member, into.*Bs::member), ...); }(data<T>::fields(), type::fields());
    }
};

template<typename M, typename C>
struct built_form<flat_vector<M>, C>
{
    using type = std::vector<built_t<M, C>>;

    static void assign(const flat_vector<M>& value, type& into)
        requires made_from_record<M, C>
    {
        into.resize(value.size());
        for (std::size_t i = 0; i < value.size(); ++i)
            built_form<M, C>::assign(value[i], into[i]);
    }
};

template<typename M, typename C>
struct built_form<flat_optional<M>, C>
{
    using type = std::optional<built_t<M, C>>;

    static void assign(const flat_optional<M>& value, type& into)
        requires made_from_record<M, C>
    {
        if (!value.has_value())
        {
            into.reset();
            return;
        }
        if (!into.has_value())
            into.emplace();
        built_form<M, C>::assign(*value, *into);
    }
};

template<typename M, typename N, typename C>
struct built_form<flat_pair<M, N>, C>
{
    using type = std::pair<built_t<M, C>, built_t<N, C>>;

    static void assign(const flat_pair<M, N>& value, type& into)
        requires made_from_record<M, C> && made_from_record<N, C>
    {
        built_form<M, C>::assign(value.first, into.first);
        built_form<N, C>::assign(value.second, into.second);
    }
};

template<typename... Ms, typename C>
struct built_form<flat_variant<Ms...>, C>
{
    using type = std::variant<built_t<Ms, C>...>;

    /// Its alternative's form, at the alternative's position (kept where it holds that alternative already).
    static void assign(const flat_variant<Ms...>& value, type& into)
        requires(made_from_record<Ms, C> && ...)
    {
        value.apply(
            [&]<typename A>(const A& alternative)
            {
                constexpr auto at = boost::mp11::mp_find<boost::mp11::mp_list<Ms...>, A>::value;
                if (into.index() != at)
                    into.template emplace<at>();
                built_form<A, C>::assign(alternative, std::get<at>(into));
            });
    }
};

/// The index an insert gives: of the pair (view or index, created) a repository's insert returns, of the interned
/// record's view (an index_view: its key), or the index itself.
template<typename U>
key<U> inserted_index(key<U> at) noexcept
{ return at; }

template<typename T, typename C, typename K>
K inserted_index(const index_view<T, C, K>& view) noexcept
{ return view.index(); }

/// An insert's result (std::set's convention): the interned record, as its view or its index (the overloads above),
/// and whether it was created.
template<typename A>
auto inserted_index(const std::pair<A, bool>& inserted) noexcept
{ return urd::inserted_index(inserted.first); }

/// What a key function gives for a child whose record it did not find (a lookup that interns nothing): its key is none.
struct missing_key
{
};

namespace detail
{
/// A slot of type S that is a child: an index of a type in the list Types.
template<typename S, typename Types>
concept child_of = Key<std::remove_const_t<S>> && boost::mp11::mp_contains<Types, typename std::remove_const_t<S>::element_type>::value;
template<typename Types, typename S>
inline constexpr bool is_child = child_of<S, Types>;

/// walk over record's members, but for its own handle: a direct member of type key<T>.
template<typename T, typename R, typename F>
void walk_members(R& record, F f)
{
    for_each_cista_member(record,
                          [&]<typename M>(M& member)
                          {
                              if constexpr (!std::same_as<std::remove_const_t<M>, key<T>>)
                                  walk(member, f);
                          });
}

/// Assigns source's members to scratch's, all but scratch's own handle (a direct member of type key<T>, which stays as
/// it is: none in a checked-out scratch). A flat vector keeps the memory it has (set), so a scratch reused across calls
/// allocates only where a member grows beyond what it held before; every other member is assigned.
template<typename T, typename R>
void assign_members(R& scratch, const R& source)
{
    [&]<auto... Ms>(Members<Ms...>)
    {
        (
            [&]
            {
                using M = std::remove_cvref_t<decltype(scratch.*Ms)>;
                if constexpr (std::same_as<M, key<T>>)
                    return;
                else if constexpr (FlatVector<M>)
                    (scratch.*Ms).set((source.*Ms).begin(), (source.*Ms).end());
                else
                    scratch.*Ms = source.*Ms;
            }(),
            ...);
    }(R::cista_members());
}

/// A builder's member written into its record's member of the same position, converted: a child (urd::child) as the
/// index f gives it, a builder held in place as its record, std's containers as the flat ones, a value as it is.
// Every overload declared first: the qualified calls between them (a vector's elements, a variant's alternative) find
// only what is declared before them.
template<typename M, typename B, typename F>
void assign_member(flat_vector<M>& target, const std::vector<B>& source, F& f);
template<typename M, typename Kind, typename... Tags, typename F>
void assign_member(flat_vector<M>& target, const Store<Kind, Tags...>& source, F& f);
template<typename M, typename B, typename F>
void assign_member(flat_optional<M>& target, const std::optional<B>& source, F& f);
template<typename M, typename N, typename A, typename B, typename F>
void assign_member(flat_pair<M, N>& target, const std::pair<A, B>& source, F& f);
template<typename... Ms, typename... Bs, typename F>
void assign_member(flat_variant<Ms...>& target, const std::variant<Bs...>& source, F& f);

template<typename M, typename F>
void assign_member(M& target, const M& source, F& /*f*/)
{ detail::assign_value(source, target); }

template<typename R>
inline constexpr bool lookup_result_type = false;
template<typename V>
inline constexpr bool lookup_result_type<std::optional<V>> = true;

/// Whether R is a lookup's result: the optional of what it found.
template<typename R>
concept LookupResult = lookup_result_type<R>;

/// What an insert or a lookup gave, as the key K: the key itself, or the index an insert's result names
/// (urd::inserted_index); none for a key a lookup did not find (urd::missing_key, or an empty lookup result).
template<typename K, typename R>
K as_key(const R& inserted)
{
    using Result = std::remove_cvref_t<R>;
    if constexpr (std::same_as<Result, K>)
        return inserted;
    else if constexpr (std::same_as<Result, missing_key>)
        return K {};
    else if constexpr (LookupResult<Result>)
        return inserted.has_value() ? detail::as_key<K>(*inserted) : K {};
    else
        return urd::inserted_index(inserted);
}

/// A child as the key f gives it in the form it holds: its key (interned), its builder (built), or its record not
/// interned yet (held, writable: interning may prepare it).
template<typename K, typename U, typename C, typename F>
void assign_member(K& target, const child<U, C, K>& source, F& f)
{
    if (const auto* interned = std::get_if<0>(&source))
        target = detail::as_key<K>(f(std::as_const(*interned)));
    else if (const auto* built = std::get_if<1>(&source))
        target = detail::as_key<K>(f(std::as_const(**built)));
    else
        target = detail::as_key<K>(f(*std::get<2>(source)));
}

template<typename U, typename C, typename F>
void assign_built(data<U>& target, const builder<U, C>& source, F& f);

/// A builder held in place written as its record, field by field.
template<typename U, typename C, typename F>
void assign_member(data<U>& target, const builder<U, C>& source, F& f)
{ detail::assign_built(target, source, f); }

/// A builder held in place where its record holds a key of it (its built form a builder: urd::built_form): written as
/// the key f gives, which interns the builder in the repository that holds its kind.
template<typename U, typename C, typename F>
void assign_member(key<U>& target, const builder<U, C>& source, F& f)
{ target = detail::as_key<key<U>>(f(source)); }

/// Whether a record's member of type M takes a built value of another type B as it is (a library's own built form that
/// assigns itself into its record's form).
template<typename M, typename B>
concept AssignsBuilt = !std::same_as<M, B> && requires(M& target, const B& source) { target = source; };

/// A built value the record's member takes by assignment.
template<typename M, typename B, typename F>
    requires AssignsBuilt<M, B>
void assign_member(M& target, const B& source, F& /*f*/)
{ target = source; }

template<typename M, typename B, typename F>
void assign_member(flat_vector<M>& target, const std::vector<B>& source, F& f)
{
    using size_type = typename flat_vector<M>::size_type;
    target.resize(static_cast<size_type>(source.size()));
    for (size_type i = 0; i < target.size(); ++i)
        detail::assign_member(target[i], source[i], f);
}

/// A Store of children (a builder's set of them, say) as the record's flat vector, in the Store's order.
template<typename M, typename Kind, typename... Tags, typename F>
void assign_member(flat_vector<M>& target, const Store<Kind, Tags...>& source, F& f)
{
    using size_type = typename flat_vector<M>::size_type;
    target.resize(static_cast<size_type>(source.size()));
    auto at = size_type { 0 };
    for (const auto& element : source)
        detail::assign_member(target[at++], element, f);
}

template<typename M, typename B, typename F>
void assign_member(flat_optional<M>& target, const std::optional<B>& source, F& f)
{
    if (!source.has_value())
    {
        target.reset();
        return;
    }
    auto value = M();
    detail::assign_member(value, *source, f);
    target = std::move(value);
}

template<typename M, typename N, typename A, typename B, typename F>
void assign_member(flat_pair<M, N>& target, const std::pair<A, B>& source, F& f)
{
    detail::assign_member(target.first, source.first, f);
    detail::assign_member(target.second, source.second, f);
}

/// A variant member: the alternative it holds, at the same position.
template<std::size_t I, typename... Ms, typename... Bs, typename F>
void assign_alternative(flat_variant<Ms...>& target, const std::variant<Bs...>& source, F& f)
{
    if constexpr (I < sizeof...(Bs))
    {
        if (source.index() != I)
            return detail::assign_alternative<I + 1>(target, source, f);
        auto value = std::variant_alternative_t<I, std::variant<Ms...>>();
        detail::assign_member(value, std::get<I>(source), f);
        target = std::move(value);
    }
}

template<typename... Ms, typename... Bs, typename F>
void assign_member(flat_variant<Ms...>& target, const std::variant<Bs...>& source, F& f)
{
    static_assert(sizeof...(Ms) == sizeof...(Bs), "urd::map_children: a builder's variant member has its record's alternatives");
    detail::assign_alternative<0>(target, source, f);
}

/// A builder's fields written into its record's, in step (the same names, in the same order).
template<typename... Ds, typename... Bs, typename D, typename B, typename F>
void assign_fields(Fields<Ds...> /*record*/, Fields<Bs...> /*built*/, D& target, const B& source, F& f)
{
    static_assert(sizeof...(Ds) == sizeof...(Bs), "urd::map_children: a builder has its record's fields");
    static_assert(((Ds::name == Bs::name) && ...), "urd::map_children: a builder's fields are its record's, in order");
    (detail::assign_member(target.*Ds::member, source.*Bs::member, f), ...);
}

template<typename U, typename C, typename F>
void assign_built(data<U>& target, const builder<U, C>& source, F& f)
{ detail::assign_fields(data<U>::fields(), builder<U, C>::fields(), target, source, f); }
}  // namespace detail

namespace detail
{
/// f(context.view(child)) for each child of the field Member of a record in its stored form S (urd::stored_field: in
/// place for a record held whole, by value from a record laid out by its store): its children of Types, or, for a
/// member whose elements are children by their position (element_children), each element that is one where the context
/// resolves its kind.
template<typename Types, auto Member, typename S, typename C, typename F>
void visit_field_children(const S& stored, const C& context, F& f)
{
    decltype(auto) member = stored_field<Member, S>::read(stored);
    if constexpr (HasElementChildren<Member>)
    {
        using Children = element_children<Member>;
        if constexpr (ViewsKeys<C, typename Children::child_type>)
        {
            auto position = std::size_t { 0 };
            for (const auto element : member)
            {
                if (Children::is_child(stored, context, position))
                    f(context.view(Children::child(element)));
                ++position;
            }
        }
        else
            return;  // a context that resolves no child of the kind names none
    }
    else
    {
        auto visit = [&]<typename X>(X& child)
            requires detail::is_child<Types, std::remove_const_t<X>> { f(context.view(child)); };
        detail::walk(member, visit);
    }
}

/// The field Member of scratch, its children replaced by what f(context.view(child)) reads (urd::store); an element
/// that is a child by its position (element_children) by the key f gives for its view.
template<typename Types, auto Member, typename R, typename S, typename C, typename F>
void map_field_children(R& scratch, const S& source, const C& context, F& f)
{
    auto& member = scratch.*Member;
    if constexpr (HasElementChildren<Member>)
    {
        using Children = element_children<Member>;
        using Child = typename Children::child_type;
        if constexpr (ViewsKeys<C, Child>)
        {
            for (std::size_t position = 0; position < member.size(); ++position)
                if (Children::is_child(source, context, position))
                    member[position] = Children::element(resolution<Child>::stored(f(context.view(Children::child(member[position])))));
        }
        else
            return;  // a context that resolves no child of the kind names none
    }
    else
    {
        auto replace = [&]<typename X>(X& child)
            requires detail::is_child<Types, X> { urd::store(child, f(context.view(child))); };
        detail::walk(member, replace);
    }
}

/// scratch's fields from a record of T in its stored form S (urd::stored_field), all but its own handle: a flat vector
/// keeps the memory it has (set), so a scratch reused across calls allocates only where a field grows.
template<typename T, typename R, typename S>
void assign_fields_from(R& scratch, const S& source)
{
    [&]<typename... Ds>(Fields<Ds...> /*record*/)
    {
        (
            [&]
            {
                decltype(auto) value = stored_field<Ds::member, S>::read(source);
                auto& target = scratch.*Ds::member;
                if constexpr (FlatVector<std::remove_cvref_t<decltype(target)>>)
                    target.set(std::ranges::begin(value), std::ranges::end(value));
                else if constexpr (std::same_as<std::remove_cvref_t<decltype(value)>, std::remove_cvref_t<decltype(target)>>)
                    detail::assign_value(value, target);
                else
                    target = value;
            }(),
            ...);
    }(data<T>::fields());
}
}  // namespace detail

/// Calls f(context.view(child)) for every child of a record of T (in any stored form: data<T>, its stored_ref), field by
/// field in order: its children of Types, and the elements of a member that are children by their position.
template<typename Types, typename T, typename R, typename C, typename F>
void for_each_child(const R& record, const C& context, F f)
{
    if constexpr (RecordFields<T>)
        [&]<typename... Ds>(Fields<Ds...> /*record*/) { (detail::visit_field_children<Types, Ds::member>(record, context, f), ...); }(data<T>::fields());
    else
        detail::walk_members<T>(record,
                                [&]<typename S>(S& child)
                                    requires detail::is_child<Types, S> { f(context.view(child)); });  // a plain value of its own: its members
}

/// for_each_child of the record a view reads, in its stored form, through its own context: the way to visit a view's
/// children without handing its stored form out of urd (detail::view_access).
template<typename Types, typename T, typename V, typename F>
void for_each_child(const V& source, F f)
{ urd::for_each_child<Types, T>(detail::view_access::stored(source), source.context(), std::move(f)); }

/// source (a record of T in any stored form) with every child c replaced by what f(context.view(c)) reads, stored as c
/// (urd::store), field by field, written into scratch, which it returns: f returns an index of c's type, or a view of
/// one; an element of a member that is a child by its position likewise. source is read, not copied (a record is
/// move-only): scratch's fields are assigned from it (detail::assign_fields_from), so a scratch reused across calls (a
/// checked-out builder) allocates nothing once its members have grown; scratch's own handle stays as it is.
template<typename Types, typename T, typename S, typename R, typename C, typename F>
R& map_children(const S& source, R& scratch, const C& context, F f)
{
    if constexpr (RecordFields<T>)
    {
        detail::assign_fields_from<T>(scratch, source);
        [&]<typename... Ds>(Fields<Ds...> /*record*/) { (detail::map_field_children<Types, Ds::member>(scratch, source, context, f), ...); }(data<T>::fields());
    }
    else
    {
        // a plain value of its own (no record): its members, as they are held
        detail::assign_members<T>(scratch, source);
        detail::walk_members<T>(scratch,
                                [&]<typename X>(X& child)
                                    requires detail::is_child<Types, X> { urd::store(child, f(context.view(child))); });
    }
    return scratch;
}

/// The record of a builder, written into scratch (a reused record: its buffers kept, its own handle as it is): each
/// field in step with the builder's (urd::builder: the same names, in the same order), each child (urd::child: interned,
/// built, or a record not interned yet; a vector's elements one by one, each in its own form) replaced by the key f
/// gives for it in its form: f(key) for an interned one, f(const builder&) for a built one, f(data&) for a
/// record not interned yet; f returns a key, a view of one, the pair a repository's insert returns, or missing_key.
/// Builders held in place are written as their records, std's containers as the flat ones.
template<typename T, typename C, typename F>
data<T>& map_children(const builder<T, C>& source, data<T>& scratch, F f)
{
    detail::assign_built(scratch, source, f);
    return scratch;
}

/// The parts Ps of its key a record is known by before it is stored (its key(), all but the last part, which its store
/// writes): what it is inserted under. None for a key of one part.
template<typename... Ps, typename T>
key<Ps...> built_key(const data<T>& record)
{
    if constexpr (sizeof...(Ps) == 0)
        return key<>();
    else
    {
        const auto whole = record.key();
        return key<Ps...>(whole.template get<Ps>()...);
    }
}

/// Interns a builder: its record written into scratch (map_children), each child as the key `insert` gives it, then the
/// record inserted. `insert` takes every form a child holds: an interned child's key (kept as it is), a built one's builder (interned again, with a scratch
/// record of its own) and a record not interned yet, of every kind the builder holds, and inserts it into the repository that holds its kind; so built children
/// first. A child of a kind another context holds must be interned there: insert takes only its key (a built one or a record of that kind does not compile).
/// Returns what insert(scratch) returns.
template<typename T, typename C, typename Insert>
decltype(auto) intern(const builder<T, C>& built, data<T>& scratch, Insert insert)
{
    urd::map_children(built, scratch, [&](auto& form) -> decltype(auto) { return insert(form); });
    return insert(scratch);
}

namespace detail
{
/// A builder's member from its record's, read through `context` (build_from): a child from an index, as the view f
/// gives for the element it names (a view in the builder's context); a record held in place as its builder; the flat
/// containers as std's, element by element; anything else as it is.
/// Whether T is a std container a builder member is built element by element (a vector, an optional): never copied
/// whole, since its elements may be children (move-only), which std's copyability does not tell.
template<typename T>
inline constexpr bool is_built_element_wise = false;
template<typename T, typename A>
inline constexpr bool is_built_element_wise<std::vector<T, A>> = true;
template<typename T>
inline constexpr bool is_built_element_wise<std::optional<T>> = true;

/// Whether a builder holds a member of type K as its record does although it names a record (a key of its own, as a
/// row's: it resolves through context Cx to a view with a key, urd::Indexed): build_from builds it as a child.
template<typename K, typename Cx>
concept held_key = requires(const K& key, const Cx& context) {
    { urd::resolve(key, context) } -> Indexed;
};

// Every overload declared first, as assign_member's.
template<typename B, typename M, typename Cx, typename F>
void build_member(std::vector<B>& target, const flat_vector<M>& source, const Cx& context, F& f);
template<typename B, typename M, typename Cx, typename F>
void build_member(std::optional<B>& target, const flat_optional<M>& source, const Cx& context, F& f);
template<typename B, typename M, typename Cx, typename F>
void build_member(std::vector<B>& target, const std::vector<M>& source, const Cx& context, F& f);
template<typename B, typename M, typename Cx, typename F>
void build_member(std::optional<B>& target, const std::optional<M>& source, const Cx& context, F& f);
template<typename A, typename B, typename M, typename N, typename Cx, typename F>
void build_member(std::pair<A, B>& target, const flat_pair<M, N>& source, const Cx& context, F& f);
template<typename... Bs, typename... Ms, typename Cx, typename F>
void build_member(std::variant<Bs...>& target, const flat_variant<Ms...>& source, const Cx& context, F& f);
template<typename T, typename Cx, typename F>
    requires(!is_built_element_wise<T> && !held_key<T, Cx>)
void build_member(T& target, const T& source, const Cx& context, F& f);
template<typename K, typename Cx, typename F>
    requires held_key<K, Cx>
void build_member(K& target, const K& source, const Cx& context, F& f);

/// A child from another builder's child: its key where it is interned, as a record's key is built (f's view of it: the
/// key in another repository where the builder is copied there); a built or held child is owned by its builder, not
/// copied (std::bad_variant_access).
template<typename U, typename C, typename K, typename Cx, typename F>
void build_member(child<U, C, K>& target, const child<U, C, K>& source, const Cx& context, F& f);

/// A child from a view (a writer that has the view already): its key.
template<typename U, typename C, typename K, typename Cv, typename Cx, typename F>
void build_member(child<U, C, K>& target, const index_view<U, Cv, K>& source, const Cx& /*context*/, F& /*f*/)
{ target.template emplace<0>(source.index()); }

/// A child from a record's key: the key of the view f gives for the element it names (f(view): as it is, or the view in
/// another repository that copying gives).
template<typename U, typename C, typename K, typename Cx, typename F>
void build_member(child<U, C, K>& target, const K& source, const Cx& context, F& f)
{
    if (source == K {})
        target = child<U, C, K> {};  // none: the empty child
    else
        urd::store(target, f(context.view(source)));
}

template<typename U, typename C, typename K, typename Cx, typename F>
void build_member(child<U, C, K>& target, const child<U, C, K>& source, const Cx& context, F& f)
{ detail::build_member(target, std::get<0>(source), context, f); }

template<typename U, typename C, typename D, typename Cx, typename F>
    requires(!is_builder<D>)
void build_fields_of(builder<U, C>& target, const D& source, const Cx& context, F& f);

template<typename U, typename C, typename Cx, typename F>
void build_member(builder<U, C>& target, const data<U>& source, const Cx& context, F& f)
{ detail::build_fields_of(target, source, context, f); }

template<typename B, typename M, typename Cx, typename F>
void build_member(std::vector<B>& target, const flat_vector<M>& source, const Cx& context, F& f)
{
    target.resize(source.size());
    for (typename flat_vector<M>::size_type i = 0; i < source.size(); ++i)
        detail::build_member(target[i], source[i], context, f);
}

template<typename B, typename M, typename Cx, typename F>
void build_member(std::optional<B>& target, const flat_optional<M>& source, const Cx& context, F& f)
{
    if (!source.has_value())
    {
        target.reset();
        return;
    }
    detail::build_member(target.emplace(), *source, context, f);
}

/// A record-form value held in a std container (a writer's own list or optional of indices, say): as the flat one.
template<typename B, typename M, typename Cx, typename F>
void build_member(std::vector<B>& target, const std::vector<M>& source, const Cx& context, F& f)
{
    target.resize(source.size());
    for (std::size_t i = 0; i < source.size(); ++i)
        detail::build_member(target[i], source[i], context, f);
}

template<typename B, typename M, typename Cx, typename F>
void build_member(std::optional<B>& target, const std::optional<M>& source, const Cx& context, F& f)
{
    if (!source.has_value())
    {
        target.reset();
        return;
    }
    detail::build_member(target.emplace(), *source, context, f);
}

template<typename A, typename B, typename M, typename N, typename Cx, typename F>
void build_member(std::pair<A, B>& target, const flat_pair<M, N>& source, const Cx& context, F& f)
{
    detail::build_member(target.first, source.first, context, f);
    detail::build_member(target.second, source.second, context, f);
}

/// A variant member: the alternative its record's holds, at the same position.
template<std::size_t I, typename... Bs, typename... Ms, typename Cx, typename F>
void build_alternative(std::variant<Bs...>& target, const flat_variant<Ms...>& source, const Cx& context, F& f)
{
    if constexpr (I < sizeof...(Ms))
    {
        if (source.index() != I)
            return detail::build_alternative<I + 1>(target, source, context, f);
        detail::build_member(target.template emplace<I>(), cista::get<I>(source), context, f);
    }
}

template<typename... Bs, typename... Ms, typename Cx, typename F>
void build_member(std::variant<Bs...>& target, const flat_variant<Ms...>& source, const Cx& context, F& f)
{
    static_assert(sizeof...(Bs) == sizeof...(Ms), "urd::build_from: a builder's variant member has its record's alternatives");
    detail::build_alternative<0>(target, source, context, f);
}

template<typename T, typename Cx, typename F>
    requires(!is_built_element_wise<T> && !held_key<T, Cx>)
void build_member(T& target, const T& source, const Cx& /*context*/, F& /*f*/)
{ detail::assign_value(source, target); }

/// A key held as it is (a builder holds no child of it): the key of the view f gives for what it names, as a child's;
/// the none key as it is.
template<typename K, typename Cx, typename F>
    requires held_key<K, Cx>
void build_member(K& target, const K& source, const Cx& context, F& f)
{
    if (source == K {})
        target = source;
    else
        urd::store(target, f(urd::resolve(source, context)));
}

/// A builder's member from a record's member Member, read through the source's member SourceMember of its form D
/// (urd::stored_field: in place for a record held whole or a builder, by value from a record laid out by its store;
/// a builder's member is the record's of the same position): a member whose elements are children
/// or plain values by their position (HasElementChildren) element by element, a child as f gives for its view, a plain
/// value as it is; any other member as build_member builds it.
template<auto Member, auto SourceMember, typename BM, typename D, typename Cx, typename F>
void build_field(BM& target, const D& source, const Cx& context, F& f)
{
    decltype(auto) member = stored_field<SourceMember, D>::read(source);
    if constexpr (HasElementChildren<Member>)
    {
        using Children = element_children<Member>;
        using Child = typename Children::child_type;
        target.clear();
        auto position = std::size_t { 0 };
        for (const auto element : member)
        {
            if constexpr (ViewsKeys<Cx, Child>)
            {
                if (Children::is_child(source, context, position))
                    target.push_back(Children::element(resolution<Child>::stored(f(context.view(Children::child(element))))));
                else
                    target.push_back(element);
            }
            else
                target.push_back(element);  // a context that resolves no child of the kind names none: every element is plain
            ++position;
        }
    }
    else
        detail::build_member(target, member, context, f);
}

template<typename... Bs, typename... Ds, typename... Ss, typename B, typename D, typename Cx, typename F>
void build_fields(Fields<Bs...> /*built*/, Fields<Ds...> /*record*/, Fields<Ss...> /*source*/, B& target, const D& source, const Cx& context, F& f)
{
    static_assert(sizeof...(Bs) == sizeof...(Ds) && sizeof...(Ss) == sizeof...(Ds), "urd::build_from: a builder has its record's fields");
    static_assert(((Bs::name == Ds::name) && ...) && ((Ss::name == Ds::name) && ...), "urd::build_from: a builder's fields are its record's, in order");
    (detail::build_field<Ds::member, Ss::member>(target.*Bs::member, source, context, f), ...);
}

/// A builder's fields from a record of U in any stored form (data<U>, its stored_ref: a record_ref or a reader of a
/// record laid out by its store).
template<typename U, typename C, typename D, typename Cx, typename F>
    requires(!is_builder<D>)
void build_fields_of(builder<U, C>& target, const D& source, const Cx& context, F& f)
{ detail::build_fields(builder<U, C>::fields(), data<U>::fields(), data<U>::fields(), target, source, context, f); }

/// A builder's fields from another builder's of the same record (of any context: a builder read through its view, as
/// copying it into another repository does).
template<typename U, typename C, typename D, typename Cx, typename F>
void build_fields_of(builder<U, C>& target, const builder<U, D>& source, const Cx& context, F& f)
{ detail::build_fields(builder<U, C>::fields(), data<U>::fields(), builder<U, D>::fields(), target, source, context, f); }
}  // namespace detail

/// A builder's member from its record's, read through `context`: each child as the view the context gives for it, a
/// record held in place as its builder, the flat containers as std's (a term written by a function that gives a
/// record's, say).
template<typename B, typename M, typename Cx>
void build_member(B& target, const M& source, const Cx& context)
{
    const auto as_it_is = [](const auto& view) { return view; };
    detail::build_member(target, source, context, as_it_is);
}

/// Whether V is an interned view (index_view): the form of a reading that has a key.
template<typename V>
struct is_index_view : std::false_type
{
};

template<typename T, typename C, typename K>
struct is_index_view<index_view<T, C, K>> : std::true_type
{
};

/// The interned view a reading of a T reads (a view of it, or a resolved child: the variant of the views of its forms):
/// an interned one as it is; a built or held record has no key, its holder's (std::bad_variant_access).
template<typename T, typename C, typename K>
index_view<T, C, K> interned_view(const index_view<T, C, K>& interned)
{ return interned; }

template<typename... Vs>
auto interned_view(const std::variant<Vs...>& reading)
{
    using Interned = boost::mp11::mp_at<boost::mp11::mp_list<Vs...>, boost::mp11::mp_find_if<boost::mp11::mp_list<Vs...>, is_index_view>>;
    return std::get<Interned>(reading);
}

/// A child written from a reading of its kind, as build_member copies a child from another builder's: the key of the
/// record it reads where that is interned (interned_view; a built or held one is its holder's, not copied).
template<typename U, typename C, typename K, typename R>
void copy_child(child<U, C, K>& target, const R& reading)
{ target.template emplace<0>(urd::interned_view(reading).index()); }

/// Whether a reading of a T (a view of it, or a resolved child: the variant of the views of its forms) reads the
/// interned record `interned` (the same context, the same key): a built or held record is another record, even of the
/// same values.
template<typename R, typename T, typename C, typename K>
bool is_interned_as(const R& reading, const index_view<T, C, K>& interned)
{
    return urd::visit_view(reading,
                           [&interned](const auto& view)
                           {
                               if constexpr (std::same_as<std::remove_cvref_t<decltype(view)>, index_view<T, C, K>>)
                                   return &view.context() == &interned.context() && view.index() == interned.index();
                               else
                                   return false;
                           });
}

/// A builder filled from the record a view reads, through the view's own context: each field in step with the record's
/// (urd::builder: the same names, in the same order), each child (a child member, or a key held as it is:
/// held_key) as the view f gives for the view of the element it names (f(view of the child): a view in the builder's context, as copying into another
/// repository gives it). The mirror of map_children(builder, scratch, f); target's buffers are reused, its own index stays.
template<typename V, typename T, typename C, typename F>
builder<T, C>& build_from(const V& source, builder<T, C>& target, F f)
{
    detail::build_fields_of(target, detail::view_access::stored(source), source.context(), f);
    return target;
}

/// map_children of the record a view reads, through its own context: the way to rebuild a record from a view without
/// handing its stored form out of urd (detail::view_access).
template<typename Types, typename T, typename V, typename R, typename F>
R& map_children(const V& source, R& scratch, F f)
{ return urd::map_children<Types, T>(detail::view_access::stored(source), scratch, source.context(), std::move(f)); }

/// map_children, but every flat_variant slot (one not inside another variant; any alternatives, children or not) is
/// replaced whole, in scratch: by f(urd::resolve(slot, context)) stored as the slot (urd::store), f called with the std::variant of the slot's
/// alternatives resolved (an empty slot throws std::bad_variant_access). f returns that variant, another of its
/// alternatives (a view or index of another alternative type), or anything else the slot's type stores or
/// one of its alternatives. The walk does not descend into a slot: the children (and nested variants) inside it are
/// f's to map. Children outside variants are replaced as map_children replaces them, so f is an overload set (or a
/// generic callback) taking both the views and the resolved variants.
template<typename Types, typename T, typename R, typename C, typename F>
R& map_variant_children(const R& source, R& scratch, const C& context, F f)
{
    detail::assign_members<T>(scratch, source);
    detail::walk_members<T>(scratch,
                            [&]<typename S>(S& slot)
                                requires(detail::is_child<Types, S> || FlatVariant<S>) { urd::store(slot, f(urd::resolve(std::as_const(slot), context))); });
    return scratch;
}

namespace detail
{
/// A builder's member from its record's, every variant slot replaced whole (build_variants_from): a slot by what f gives
/// for it resolved through `from` (a record's variant, written into the builder's through `to`), a list of slots slot by
/// slot, anything else as build_from builds it (its children through f).
template<typename B, typename M, typename From, typename To, typename F>
void build_variant_member(B& target, const M& source, const From& from, const To& to, F& f)
{
    if constexpr (FlatVariant<M>)
        urd::build_member(target, f(urd::resolve(source, from)), to);
    else if constexpr (FlatVector<M>)
    {
        // a list of slots slot by slot; of anything else, as build_from builds it
        target.resize(source.size());
        for (typename M::size_type i = 0; i < source.size(); ++i)
            detail::build_variant_member(target[i], source[i], from, to, f);
    }
    else
        detail::build_member(target, source, from, f);
}

template<typename... Bs, typename... Ds, typename B, typename D, typename From, typename To, typename F>
void build_variant_fields(Fields<Bs...> /*built*/, Fields<Ds...> /*record*/, B& target, const D& source, const From& from, const To& to, F& f)
{
    static_assert(sizeof...(Bs) == sizeof...(Ds), "urd::build_variants_from: a builder has its record's fields");
    static_assert(((Bs::name == Ds::name) && ...), "urd::build_variants_from: a builder's fields are its record's, in order");
    (detail::build_variant_member(target.*Bs::member, source.*Ds::member, from, to, f), ...);
}
}  // namespace detail

/// build_from, but every flat_variant slot of the record (one not inside another variant) is replaced whole: by what f
/// gives for it resolved (the std::variant of its alternatives' views), a record's variant written into the builder's
/// through `destination` (the context the builder's children are read through). The builder counterpart of
/// map_variant_children: an alternative may change on the way (a function term of another kind, say). Children outside
/// variants are built as build_from builds them, so f takes both the views and the resolved variants.
template<typename V, typename T, typename C, typename F>
builder<T, C>& build_variants_from(const V& source, builder<T, C>& target, const C& destination, F f)
{
    detail::build_variant_fields(builder<T, C>::fields(), data<T>::fields(), target, detail::view_access::record(source), source.context(), destination, f);
    return target;
}

/// map_variant_children of the record a view reads, through its own context (detail::view_access).
template<typename Types, typename T, typename V, typename R, typename F>
R& map_variant_children(const V& source, R& scratch, F f)
{ return urd::map_variant_children<Types, T>(detail::view_access::record(source), scratch, source.context(), std::move(f)); }

}  // namespace urd
