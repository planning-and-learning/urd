#pragma once

#include "urd/cost.hpp"
#include "urd/store/types.hpp"

#include <concepts>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <utility>

namespace urd::detail::facet
{

/// A pool of slots: each insert gives a new handle, by which the element is reached, changed and erased. Slots never
/// move, and erased ones are reused, so a handle is valid until its element is erased.
template<typename Derived, typename R, typename S>
class slots
{
    using value_type = typename S::element_type;

public:
    using handle_type = typename R::handle_type;

    /// What gives a slot back when its owner goes: the element's on_pool_release() where it has one (to let go of what
    /// it refers to, keeping its storage), then erase. The element is not destroyed.
    struct releaser
    {
        Derived* store = nullptr;
        handle_type handle;

        void operator()(value_type* element) const noexcept
        {
            if constexpr (requires { element->on_pool_release(); })
            {
                static_assert(noexcept(element->on_pool_release()), "urd::Store: on_pool_release() must be noexcept");
                element->on_pool_release();
            }
            store->erase(handle);
        }
    };
    /// A releaser that also holds `keep` while the slot is in use (an owner of the Store, say), and drops it after the
    /// slot is back: so the Store is still there to take it.
    struct keeping_releaser : releaser
    {
        std::shared_ptr<const void> keep;

        void operator()(value_type* element) noexcept
        {
            releaser::operator()(element);
            keep.reset();
        }
    };
    /// The control block's allocator where a shared owner holds `keep`: the control block deallocates through a copy of
    /// its allocator and destroys that copy after (libstdc++, libc++), so `keep`, and with it the pool, outlives the block.
    template<typename T>
    struct keeping_allocator
    {
        using value_type = T;
        template<typename U>
        struct rebind
        {
            using other = keeping_allocator<U>;
        };

        std::pmr::polymorphic_allocator<T> blocks;
        std::shared_ptr<const void> keep;

        template<typename U>
        keeping_allocator(const keeping_allocator<U>& other) noexcept : blocks(other.blocks), keep(other.keep)  // NOLINT(google-explicit-constructor)
        {
        }
        keeping_allocator(std::pmr::memory_resource* resource, std::shared_ptr<const void> held) noexcept : blocks(resource), keep(std::move(held)) {}

        [[nodiscard]] T* allocate(std::size_t count) { return blocks.allocate(count); }
        void deallocate(T* block, std::size_t count) noexcept { blocks.deallocate(block, count); }

        template<typename U>
        friend bool operator==(const keeping_allocator& lhs, const keeping_allocator<U>& rhs) noexcept
        { return lhs.blocks == rhs.blocks; }
    };

    handle_type insert(const value_type& element) { return representation().insert(element); }
    handle_type insert(value_type&& element) { return representation().insert(std::move(element)); }
    template<typename... Args>
    handle_type emplace(Args&&... args)
    { return representation().emplace(std::forward<Args>(args)...); }
    /// A slot to use as it is: an erased element with what it held and the storage it kept, else a new one.
    handle_type acquire()
        requires std::default_initializable<value_type>
    { return representation().acquire(); }

    /// An acquired slot owned as std::unique_ptr does: it goes back when the owner goes. No allocation once the
    /// slots in use have been so many. The Store must outlive its owners and stay where it is.
    [[nodiscard]] std::unique_ptr<value_type, releaser> acquire_unique()
        requires std::default_initializable<value_type> && shrinks<S>
    {
        const auto handle = acquire();
        return std::unique_ptr<value_type, releaser>(&representation()[handle], releaser { &self(), handle });
    }
    /// As acquire_unique(), holding `keep` while the slot is in use and dropping it once the slot is back: pass the
    /// owner of the Store (std::enable_shared_from_this) and the Store outlives the owner of its slot. A free slot
    /// holds nothing.
    [[nodiscard]] std::unique_ptr<value_type, keeping_releaser> acquire_unique(std::shared_ptr<const void> keep)
        requires std::default_initializable<value_type> && shrinks<S>
    {
        const auto handle = acquire();
        return std::unique_ptr<value_type, keeping_releaser>(&representation()[handle], keeping_releaser { { &self(), handle }, std::move(keep) });
    }
    /// An acquired slot owned as std::shared_ptr does: it goes back when the last owner goes. Every owner (unique or
    /// shared, weak ones too) returns its slot or its control block to this Store, so all must be back before the Store
    /// is moved or destroyed, unless `keep` holds what owns it. The control block comes from a pool the Store keeps
    /// (std::pmr), so no allocation either once as many owners have lived at once. The Store must outlive the slot's
    /// owners and stay where it is.
    [[nodiscard]] std::shared_ptr<value_type> acquire_shared()
        requires std::default_initializable<value_type> && shrinks<S>
    {
        const auto handle = acquire();
        return std::shared_ptr<value_type>(&representation()[handle],
                                           releaser { &self(), handle },
                                           std::pmr::polymorphic_allocator<value_type>(representation().blocks()));
    }
    /// As acquire_shared(), holding `keep` while the slot is in use and dropping it once the slot and its control block
    /// are back: pass the owner of the Store (std::enable_shared_from_this) and the Store outlives the slot's last owner.
    /// A free slot holds nothing.
    [[nodiscard]] std::shared_ptr<value_type> acquire_shared(std::shared_ptr<const void> keep)
        requires std::default_initializable<value_type> && shrinks<S>
    {
        const auto handle = acquire();
        return std::shared_ptr<value_type>(&representation()[handle],
                                           releaser { &self(), handle },
                                           keeping_allocator<value_type> { representation().blocks(), std::move(keep) });
    }

    [[nodiscard]] const value_type& operator[](handle_type handle) const { return constant()[handle]; }
    [[nodiscard]] value_type& operator[](handle_type handle) { return representation()[handle]; }

    /// Frees the slot for reuse; the handle and references to the element mean nothing after.
    void erase(handle_type handle)
        requires shrinks<S>
    { representation().erase(handle); }
    void clear()
        requires shrinks<S>
    { representation().clear(); }

private:
    [[nodiscard]] const R& constant() const { return store_access::of<Derived>(*this); }
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
    [[nodiscard]] Derived& self() { return static_cast<Derived&>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

template<>
struct facet_operations<facet::slots>
{
    using offered = operation_list<operation::insert_row, operation::access>;
};

}  // namespace urd::detail
