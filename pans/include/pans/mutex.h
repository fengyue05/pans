#ifndef _PANS_INCLUDE_PANS_MUTEX_H_
#define _PANS_INCLUDE_PANS_MUTEX_H_

#include <mutex>
#include <atomic>
#include <immintrin.h>

inline void cpu_relax() noexcept
{
#if defined (__x86_64__) || defined (__i386__)
    _mm_pause();
#endif
}

namespace pans {
class SpinLock {
public:
    using Lock = std::lock_guard<SpinLock>;
    
    SpinLock() noexcept = default;
    ~SpinLock() noexcept = default;

    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    void lock() noexcept
    {
        while(mutex_.test_and_set(std::memory_order_acquire)) 
        {
            while(mutex_.test(std::memory_order_relaxed))
            {
                cpu_relax();
            }
        }
    }

    // 不可以忽略掉返回值，也就是要bool a = try_lock();
    [[nodiscard]] bool try_lock() noexcept
    {
        return !mutex_.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept
    {
        mutex_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag mutex_ = ATOMIC_FLAG_INIT; // 0
};
}

#endif