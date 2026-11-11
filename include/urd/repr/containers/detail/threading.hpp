/*
 * Copyright (C) 2026 Dominik Drexler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef URD_REPR_CONTAINERS_DETAIL_THREADING_HPP_
#define URD_REPR_CONTAINERS_DETAIL_THREADING_HPP_

#include <atomic>
#include <cstddef>
#include <mutex>
#include <type_traits>
#include <utility>

namespace urd::repr::detail
{

template<bool ThreadSafe>
class Mutex;

/// No lock: where one thread uses the container.
template<>
class Mutex<false>
{
public:
    void lock() noexcept {}
    void unlock() noexcept {}
};

template<>
class Mutex<true>
{
public:
    Mutex() = default;
    ~Mutex() = default;
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    // Moving requires quiescence; the destination keeps an independent lock.
    Mutex(Mutex&&) noexcept {}
    Mutex& operator=(Mutex&&) noexcept { return *this; }

    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }

private:
    std::mutex m_mutex;
};

template<bool ThreadSafe, typename Fn>
decltype(auto) with_lock(Mutex<ThreadSafe>& mutex, Fn&& fn)
{
    const std::lock_guard lock(mutex);
    return std::forward<Fn>(fn)();
}

template<bool ThreadSafe>
using Size = std::conditional_t<ThreadSafe, std::atomic_size_t, size_t>;

/// What a Size or a Pointer holds, by how it is held: an atomic is loaded with acquire, anything else read as it is.
template<typename T>
T load(const std::atomic<T>& value) noexcept
{ return value.load(std::memory_order_acquire); }
template<typename T>
T load(const T& value) noexcept
{ return value; }

/// Written likewise: an atomic stored with release, after what it publishes.
template<typename T>
void store(std::atomic<T>& target, std::type_identity_t<T> value) noexcept
{ target.store(value, std::memory_order_release); }
template<typename T>
void store(T& target, std::type_identity_t<T> value) noexcept
{ target = value; }

/// A plain object read where threads may write it: loaded with acquire through std::atomic_ref, else as it is.
template<bool ThreadSafe, typename T>
T load(const T& value) noexcept
{
    if constexpr (ThreadSafe)
        return std::atomic_ref(const_cast<T&>(value))
            .load(std::memory_order_acquire);  // NOLINT(cppcoreguidelines-pro-type-const-cast): C++20 has atomic_ref of non-const objects only
    else
        return value;
}

/// Written where threads may read it: stored with release, after what it publishes.
template<bool ThreadSafe, typename T>
void store(T& target, std::type_identity_t<T> value) noexcept
{
    if constexpr (ThreadSafe)
        std::atomic_ref(target).store(value, std::memory_order_release);
    else
        target = value;
}

template<bool ThreadSafe, typename T>
using Pointer = std::conditional_t<ThreadSafe, std::atomic<T*>, T*>;

}  // namespace urd::repr::detail

#endif
