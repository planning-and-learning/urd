#pragma once

// A container with the number of the Store it serves (urd::layer): what a Store that hands out indices for views is
// built on (urd/registry/select.hpp), so that a view's identity is its index and its Store's number. The container
// itself does not change; its declaration and costs are its own, and it compares and hashes as its container.

#include "urd/elements/index.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"

#include <concepts>
#include <cstddef>
#include <tuple>
#include <utility>

namespace urd::repr
{

/// Container R, numbered: built from its number and whatever R is built from, never without the number.
template<typename R>
class Numbered : public R
{
public:
    template<typename... Args>
        requires std::constructible_from<R, Args...>
    explicit Numbered(urd::layer number, Args&&... args) : R(std::forward<Args>(args)...), m_number(number)
    {
    }

    /// Its number, as it was given.
    [[nodiscard]] std::size_t layer() const noexcept { return m_number.number; }
    /// Identified by its container alone (urd::EqualTo, Hash, Less): the number names the Store, not its contents.
    static constexpr auto identifying_members() noexcept { return Members<&Numbered::container> {}; }

private:
    /// Its container, as the base it is: what identifying_members names.
    [[nodiscard]] const R& container() const noexcept { return *this; }

    urd::layer m_number;
};

}  // namespace urd::repr

namespace urd
{

template<typename R>
struct detail::declaration_of<repr::Numbered<R>> : detail::declaration_of<R>
{
};

/// A numbered container holds what its container holds.
template<typename R>
struct memory_of<repr::Numbered<R>>
{
    static std::size_t bytes(const repr::Numbered<R>& held) { return memory_usage(static_cast<const R&>(held)); }
};

}  // namespace urd
