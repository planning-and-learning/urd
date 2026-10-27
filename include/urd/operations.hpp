#pragma once

#include <utility>

namespace urd
{

// The operations a container's properties require (interface.hpp says which), as Boost.Graph's are for graphs:
// called as free functions, `urd::insert(c, element)`, so generic code works on any conforming container.
// Each calls the container's member of the standard name when it has one, so standard containers and
// containers written to the standard conform as they are. A container with another interface conforms
// through an adapter: free functions of the same name in its namespace, found by argument-dependent lookup.
//
//     namespace lib { auto insert(Bag& bag, int element) { return bag.add(element); } }

namespace detail::operations
{

// Declared deleted so that only a member or an adapter found by argument-dependent lookup is called.
void insert() = delete;
void emplace() = delete;
void push_back() = delete;
void emplace_back() = delete;
void pop_back() = delete;
void erase() = delete;
void clear() = delete;
void find() = delete;
void count() = delete;
void contains() = delete;
void lower_bound() = delete;
void upper_bound() = delete;
void equal_range() = delete;

struct insert_fn
{
    template<typename R, typename... Args>
        requires requires(R& range, Args&&... args) { range.insert(std::forward<Args>(args)...); }
                 || requires(R& range, Args&&... args) { insert(range, std::forward<Args>(args)...); }
    constexpr decltype(auto) operator()(R& range, Args&&... args) const
    {
        if constexpr (requires { range.insert(std::forward<Args>(args)...); })
            return range.insert(std::forward<Args>(args)...);
        else
            return insert(range, std::forward<Args>(args)...);
    }
};

struct emplace_fn
{
    template<typename R, typename... Args>
        requires requires(R& range, Args&&... args) { range.emplace(std::forward<Args>(args)...); }
                 || requires(R& range, Args&&... args) { emplace(range, std::forward<Args>(args)...); }
    constexpr decltype(auto) operator()(R& range, Args&&... args) const
    {
        if constexpr (requires { range.emplace(std::forward<Args>(args)...); })
            return range.emplace(std::forward<Args>(args)...);
        else
            return emplace(range, std::forward<Args>(args)...);
    }
};

struct push_back_fn
{
    template<typename R, typename... Args>
        requires requires(R& range, Args&&... args) { range.push_back(std::forward<Args>(args)...); }
                 || requires(R& range, Args&&... args) { push_back(range, std::forward<Args>(args)...); }
    constexpr decltype(auto) operator()(R& range, Args&&... args) const
    {
        if constexpr (requires { range.push_back(std::forward<Args>(args)...); })
            return range.push_back(std::forward<Args>(args)...);
        else
            return push_back(range, std::forward<Args>(args)...);
    }
};

struct emplace_back_fn
{
    template<typename R, typename... Args>
        requires requires(R& range, Args&&... args) { range.emplace_back(std::forward<Args>(args)...); }
                 || requires(R& range, Args&&... args) { emplace_back(range, std::forward<Args>(args)...); }
    constexpr decltype(auto) operator()(R& range, Args&&... args) const
    {
        if constexpr (requires { range.emplace_back(std::forward<Args>(args)...); })
            return range.emplace_back(std::forward<Args>(args)...);
        else
            return emplace_back(range, std::forward<Args>(args)...);
    }
};

struct pop_back_fn
{
    template<typename R>
        requires requires(R& range) { range.pop_back(); } || requires(R& range) { pop_back(range); }
    constexpr decltype(auto) operator()(R& range) const
    {
        if constexpr (requires { range.pop_back(); })
            return range.pop_back();
        else
            return pop_back(range);
    }
};

// For a sequence, erase(c, value) is the standard's std::erase, found by argument-dependent lookup.
struct erase_fn
{
    template<typename R, typename... Args>
        requires requires(R& range, Args&&... args) { range.erase(std::forward<Args>(args)...); }
                 || requires(R& range, Args&&... args) { erase(range, std::forward<Args>(args)...); }
    constexpr decltype(auto) operator()(R& range, Args&&... args) const
    {
        if constexpr (requires { range.erase(std::forward<Args>(args)...); })
            return range.erase(std::forward<Args>(args)...);
        else
            return erase(range, std::forward<Args>(args)...);
    }
};

struct clear_fn
{
    template<typename R>
        requires requires(R& range) { range.clear(); } || requires(R& range) { clear(range); }
    constexpr decltype(auto) operator()(R& range) const
    {
        if constexpr (requires { range.clear(); })
            return range.clear();
        else
            return clear(range);
    }
};

struct find_fn
{
    template<typename R, typename E>
        requires requires(R& range, const E& element) { range.find(element); }
                 || requires(R& range, const E& element) { find(range, element); }
    constexpr decltype(auto) operator()(R& range, const E& element) const
    {
        if constexpr (requires { range.find(element); })
            return range.find(element);
        else
            return find(range, element);
    }
};

struct count_fn
{
    template<typename R, typename E>
        requires requires(R& range, const E& element) { range.count(element); }
                 || requires(R& range, const E& element) { count(range, element); }
    constexpr decltype(auto) operator()(R& range, const E& element) const
    {
        if constexpr (requires { range.count(element); })
            return range.count(element);
        else
            return count(range, element);
    }
};

struct contains_fn
{
    template<typename R, typename E>
        requires requires(R& range, const E& element) { range.contains(element); }
                 || requires(R& range, const E& element) { contains(range, element); }
    constexpr decltype(auto) operator()(R& range, const E& element) const
    {
        if constexpr (requires { range.contains(element); })
            return range.contains(element);
        else
            return contains(range, element);
    }
};

struct lower_bound_fn
{
    template<typename R, typename E>
        requires requires(R& range, const E& element) { range.lower_bound(element); }
                 || requires(R& range, const E& element) { lower_bound(range, element); }
    constexpr decltype(auto) operator()(R& range, const E& element) const
    {
        if constexpr (requires { range.lower_bound(element); })
            return range.lower_bound(element);
        else
            return lower_bound(range, element);
    }
};

struct upper_bound_fn
{
    template<typename R, typename E>
        requires requires(R& range, const E& element) { range.upper_bound(element); }
                 || requires(R& range, const E& element) { upper_bound(range, element); }
    constexpr decltype(auto) operator()(R& range, const E& element) const
    {
        if constexpr (requires { range.upper_bound(element); })
            return range.upper_bound(element);
        else
            return upper_bound(range, element);
    }
};

struct equal_range_fn
{
    template<typename R, typename E>
        requires requires(R& range, const E& element) { range.equal_range(element); }
                 || requires(R& range, const E& element) { equal_range(range, element); }
    constexpr decltype(auto) operator()(R& range, const E& element) const
    {
        if constexpr (requires { range.equal_range(element); })
            return range.equal_range(element);
        else
            return equal_range(range, element);
    }
};

}  // namespace detail::operations

inline constexpr detail::operations::insert_fn insert {};
inline constexpr detail::operations::emplace_fn emplace {};
inline constexpr detail::operations::push_back_fn push_back {};
inline constexpr detail::operations::emplace_back_fn emplace_back {};
inline constexpr detail::operations::pop_back_fn pop_back {};
inline constexpr detail::operations::erase_fn erase {};
inline constexpr detail::operations::clear_fn clear {};
inline constexpr detail::operations::find_fn find {};
inline constexpr detail::operations::count_fn count {};
inline constexpr detail::operations::contains_fn contains {};
inline constexpr detail::operations::lower_bound_fn lower_bound {};
inline constexpr detail::operations::upper_bound_fn upper_bound {};
inline constexpr detail::operations::equal_range_fn equal_range {};

}  // namespace urd
