#pragma once

// A map over a parent: its own values, and lookups that fall through to the parent's.

#include "urd/elements/index.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/wrappers/layered_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace urd::repr
{

/// A map with dense keys (an IndexMap) that answers lookups from its own values, then from a parent's, which it does
/// not own and never changes: a layer over a repository built earlier. It holds, iterates and erases only its own
/// values; inserting a key the parent has inserts nothing and finds the parent's. A key's own value shadows the
/// parent's: at(), update() and try_emplace() make an own value where this layer has none, once, under the slot's lock
/// (IndexMap::update): where values layer (a map of layered pools layers through both levels) one built from
/// try_emplace's arguments (a bit width), or default, layered over the parent's value (the slot's earlier value,
/// cleared, where its Store offers clear() and it was built alike: without arguments, any earlier value); else one built from the arguments, or
/// else a copy of the parent's.
/// insert_or_assign writes its own. owner(key) is the layer whose own values hold `key`.
///
/// Over a thread-safe map (layered, concurrent: the thread-safe generation map), at, update, read, try_emplace,
/// contains and owner run from threads at once; clear, erase, insert_or_assign and iteration need no other operation
/// running. The parent must outlive the layer and not change while layered over: frozen, it is read without locks.
template<typename R>
class Layered
{
public:
    using iterator = typename R::const_iterator;
    using const_iterator = iterator;
    using key_type = typename R::key_type;
    using value_type = typename R::value_type;
    using mapped_type = typename R::mapped_type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    Layered() = default;

    template<std::input_iterator InputIt>
    Layered(InputIt first, InputIt last) : m_own(first, last)
    {
    }

    /// Answers from `parent` too, from now on; nullptr ends it.
    void layer_over(const Layered* parent) { m_parent = parent; }
    [[nodiscard]] const Layered* parent() const { return m_parent; }

    [[nodiscard]] iterator begin() const { return m_own.begin(); }
    [[nodiscard]] iterator end() const { return m_own.end(); }
    [[nodiscard]] size_type size() const { return m_own.size(); }
    [[nodiscard]] bool empty() const { return m_own.empty(); }

    /// Whether this layer's own values hold `key`.
    [[nodiscard]] bool owns(const key_type& key) const
    { return read_own(key, [](const mapped_type& /*value*/) { return true; }, false); }
    /// The nearest layer, this one or a parent, whose own values hold `key`; nullptr where none does.
    [[nodiscard]] const Layered* owner(const key_type& key) const
    {
        const auto* layer = this;
        while (layer != nullptr && !layer->owns(key))
            layer = layer->m_parent;
        return layer;
    }

    /// The owner's element, else end().
    [[nodiscard]] iterator find(const key_type& key) const
    {
        const auto* layer = owner(key);
        return layer == nullptr ? end() : layer->m_own.find(key);
    }
    [[nodiscard]] bool contains(const key_type& key) const { return owner(key) != nullptr; }
    [[nodiscard]] size_type count(const key_type& key) const { return contains(key) ? 1 : 0; }

    /// A key and its value, as any pair: inserts nothing where a parent holds the key.
    template<typename Pair>
    std::pair<iterator, bool> insert(const Pair& pair)
    {
        const auto key = key_type(pair.first);
        if (const auto* layer = owner(key); layer != nullptr && layer != this)
            return { layer->m_own.find(key), false };
        return m_own.insert(pair);
    }
    template<typename Pair>
    iterator insert(const_iterator /*hint*/, const Pair& pair)
    { return insert(pair).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        for (; first != last; ++first)
            insert(*first);
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(std::pair<key_type, mapped_type>(std::forward<Args>(args)...)); }

    /// Own values only: the parent's stay.
    size_type erase(const key_type& key) { return m_own.erase(key); }
    iterator erase(const_iterator pos) { return m_own.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return m_own.erase(first, last); }
    void clear() { m_own.clear(); }

    /// The value of the owner of `key`.
    [[nodiscard]] const mapped_type& at(const key_type& key) const
    {
        if (const auto* value = value_of(key))
            return *value;
        throw std::out_of_range("urd::repr::Layered::at: no layer holds the key");
    }
    /// The own value, made where there is none.
    [[nodiscard]] mapped_type& at(const key_type& key)
    { return own(key, [](mapped_type& value, bool /*held*/) -> mapped_type& { return value; }); }
    /// read(value) with the owner's value of `key`, else `missing`: one walk up the layers.
    template<typename Read, typename Result>
        requires std::invocable<Read&, const mapped_type&>
    [[nodiscard]] Result read(const key_type& key, Read read, Result missing) const
    {
        const auto* value = value_of(key);
        return value == nullptr ? missing : Result(read(*value));
    }
    /// read(value) with this layer's own value of `key`, else `missing`: the parent's are not looked at.
    template<typename Read, typename Result>
        requires std::invocable<Read&, const mapped_type&>
    [[nodiscard]] Result read_own(const key_type& key, Read read, Result missing) const
    { return m_own.read(key, read, missing); }
    /// update(value, held) with the own value of `key`, made where there is none (held is then false).
    template<typename Update>
        requires std::invocable<Update&, mapped_type&, bool>
    decltype(auto) update(const key_type& key, Update update)
    { return own(key, update); }
    /// The own value of `key`, made from `args` where there is none: whether it was made now.
    template<typename... Args>
    std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args)
    {
        const bool made = own(key, [](mapped_type& /*value*/, bool held) { return !held; }, std::forward<Args>(args)...);
        return { m_own.find(key), made };
    }
    template<typename M>
    std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& value)
    { return m_own.insert_or_assign(key, std::forward<M>(value)); }

private:
    /// Whether values layer (are layered Stores).
    static constexpr bool layers_values = requires(mapped_type& own, const mapped_type* above) { own.layer_over(above); };

    /// The owner's value of `key`, else nullptr.
    [[nodiscard]] const mapped_type* value_of(const key_type& key) const
    {
        for (const auto* layer = this; layer != nullptr; layer = layer->m_parent)
            if (const auto* value = layer->read_own(key, [](const mapped_type& own) { return &own; }, static_cast<const mapped_type*>(nullptr)))
                return value;
        return nullptr;
    }

    /// update(value, held) with the own value of `key`, made from `args` once, under the slot's lock, where new.
    template<typename Update, typename... Args>
    decltype(auto) own(const key_type& key, Update update, Args&&... args)
    {
        return m_own.update(key,
                            [&](mapped_type& value, bool held) -> decltype(auto)
                            {
                                if (!held)
                                    make(value, m_parent == nullptr ? nullptr : m_parent->value_of(key), std::forward<Args>(args)...);
                                return update(value, held);
                            });
    }
    /// The slot's value made own (see the class). A value that layers reuses what the slot kept (a generation map's
    /// earlier value), storage and all, where its Store offers clear() and it was built alike: cleared and layered anew.
    template<typename... Args>
    static void make(mapped_type& value, const mapped_type* above, Args&&... args)
    {
        if constexpr (layers_values)
        {
            if (!reused(value, args...))
                value = mapped_type(std::forward<Args>(args)...);
            value.layer_over(above);
        }
        else if (above != nullptr && sizeof...(Args) == 0)
            value = *above;
        else
            value = mapped_type(std::forward<Args>(args)...);
    }
    /// Whether `value` was cleared for reuse: where its Store offers clear() (monotone_until_clear, or one that
    /// shrinks) and it was built alike.
    template<typename... Args>
        requires requires(mapped_type& value) { value.clear(); }
    static bool reused(mapped_type& value, const Args&... args)
    {
        if (!built_alike(value, args...))
            return false;
        value.clear();
        return true;
    }
    template<typename... Args>
    static bool reused(mapped_type& /*value*/, const Args&... /*args*/)
    { return false; }
    /// Whether `value` is what `args` would build, but for its contents: no arguments, or the one it reports it was
    /// built with (a bit width, column widths).
    static bool built_alike(const mapped_type& /*value*/) { return true; }
    template<typename Arg>
        requires requires(const mapped_type& value, const Arg& arg) {
            { value.width() == arg } -> std::convertible_to<bool>;
        }
    static bool built_alike(const mapped_type& value, const Arg& arg)
    { return value.width() == arg; }
    template<typename Arg>
        requires requires(const mapped_type& value, const Arg& arg) {
            { value.column_widths() == arg } -> std::convertible_to<bool>;
        }
    static bool built_alike(const mapped_type& value, const Arg& arg)
    { return value.column_widths() == arg; }
    template<typename... Args>
    static bool built_alike(const mapped_type& /*value*/, const Args&... /*args*/)
    { return false; }

    friend struct urd::memory_of<Layered>;

    R m_own;
    const Layered* m_parent = nullptr;
};

}  // namespace urd::repr

namespace urd
{

/// Its own layer's: the parent's is the parent's.
template<typename R>
struct memory_of<repr::Layered<R>>
{
    static std::size_t bytes(const repr::Layered<R>& held) { return memory_usage(held.m_own); }
};

template<>
struct include<"urd/repr/wrappers/layer.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

/// Over its part's container for the request.
template<typename Part, typename S>
struct representation_of<layered_decl<Part, false>, S>
{
    using type = repr::Layered<typename representation_of<Part, S>::type>;
};

}  // namespace urd::detail
