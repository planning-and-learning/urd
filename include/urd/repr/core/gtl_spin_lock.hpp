#pragma once

// gtl's parallel tables lock their shards through LockableImpl<Mutex>; one it does not know it takes as exclusive-only,
// so a shared spin mutex needs this to give its readers a shared lock.

#include "urd/repr/containers/spin_mutex.hpp"

#include <gtl/phmap.hpp>
#include <mutex>
#include <shared_mutex>

namespace gtl
{

template<>
class LockableImpl<urd::repr::SpinSharedMutex> : public urd::repr::SpinSharedMutex
{
public:
    using mutex_type = urd::repr::SpinSharedMutex;
    using Base = LockableBaseImpl<mutex_type>;
    using SharedLock = std::shared_lock<mutex_type>;
    using UniqueLock = std::unique_lock<mutex_type>;
    using ReadWriteLock = typename Base::ReadWriteLock;
    using SharedLocks = typename Base::ReadLocks;
    using UniqueLocks = typename Base::WriteLocks;
};

}  // namespace gtl
