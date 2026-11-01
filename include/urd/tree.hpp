#pragma once

// The tree urd sorts Stores by, written once: the walk from a node to the first child that claims a request, down to
// a leaf, and what lies below a node. Nothing here names a kind or a tag: the kinds are data it walks (urd/kinds.hpp),
// so adding a kind or a child is adding data, not another copy of the tree.

#include "urd/detail/type_list.hpp"

#include <cstddef>
#include <type_traits>

namespace urd::detail
{

/// A node of the tree: a name, and the children it splits into or what claims a request (or both).
template<typename Node>
concept tree_node = requires { Node::name; } && (requires { typename Node::children; } || requires { &Node::claims; });

// The walk.

/// Whether Node claims a request: by its own claims, else where one of its children does.
template<typename Node, typename Facts>
constexpr bool claims_of(const Facts& facts)
{
    if constexpr (requires { Node::claims(facts); })
        return Node::claims(facts);
    else
        return []<typename... Children>(type_list<Children...> /*children*/, const Facts& f)
        { return (claims_of<Children>(f) || ...); }(typename Node::children {}, facts);
}
/// The first of Children that claims a request.
template<typename Facts, typename... Children>
constexpr std::size_t first_claiming(type_list<Children...> /*children*/, const Facts& facts)
{
    static_assert((tree_node<Children> && ...), "urd: a child of a node is no node: it needs a name, and children or what it says (urd/tree.hpp)");
    std::size_t at = 0;
    const bool found = ((claims_of<Children>(facts) || (++at, false)) || ...);
    if (!found)
        throw "urd: no child of a node claims the request; the last should claim any";
    return at;
}

/// The path from Node down to the leaf a request reaches, Node first. Structs and std::conditional_t, not partial
/// specializations over the request: GCC 13 fails on those internally.
template<typename Node, auto Facts>
struct path_from;
template<typename Node, auto Facts>
struct path_below
{
    using child = typename nth<first_claiming(typename Node::children {}, Facts), typename Node::children>::type;
    using type = typename join<type_list<Node>, typename path_from<child, Facts>::type>::type;
};
template<typename Node, auto Facts>
struct path_from
{
    using type = typename std::conditional_t<requires { typename Node::children; }, path_below<Node, Facts>, std::type_identity<type_list<Node>>>::type;
};

/// Whether Node is Parent or below it.
template<typename Node, typename Parent>
inline constexpr bool below = std::is_same_v<Node, Parent>;
template<typename Node, typename Parent>
    requires requires { typename Parent::children; }
inline constexpr bool below<Node, Parent> = std::is_same_v<Node, Parent> || []<typename... Children>(type_list<Children...> /*children*/)
{ return (below<Node, Children> || ...); }(typename Parent::children {});

/// The leaves below a node, in order (itself where it has no children).
template<typename Node>
struct leaves_below
{
    using type = type_list<Node>;
};
template<typename Node>
    requires requires { typename Node::children; }
struct leaves_below<Node>
{
    template<typename... Children>
    static auto of(type_list<Children...> /*children*/) -> typename join<typename leaves_below<Children>::type...>::type;
    using type = decltype(of(typename Node::children {}));
};

}  // namespace urd::detail
