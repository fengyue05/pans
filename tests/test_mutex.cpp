#include <pans/mutex.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <latch>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <pans/macros.h>

using namespace pans;

constexpr int RUNS = 7;
u64 Counter_ = 0;
u64 target_ = 0;

class AtomicWaitLock 
{
public:
    using Lock = std::lock_guard<AtomicWaitLock>;

    AtomicWaitLock() noexcept = default;
    ~AtomicWaitLock() noexcept = default;

    AtomicWaitLock(const AtomicWaitLock&) = delete;
    AtomicWaitLock& operator=(const AtomicWaitLock&) = delete;

    void lock() noexcept 
    {
        bool expected = false;
        // 如果是没有占用就返回true，如果被占用了就返回false
        while(!mutex_.compare_exchange_weak(expected, true, std::memory_order_acquire, std::memory_order_relaxed))
        {
            // 如果 mutex_ 当前还是 true，那当前线程就等待；等到 mutex_ 的值发生变化后再醒来检查
            std::atomic_wait_explicit(&mutex_, true, std::memory_order_relaxed);
            expected = false;
        }
    }

    [[nodiscard]] bool try_lock ()
    {
        bool expected = false;
        // 这里因为只尝试一次，所以是strong，不可以出错
        return mutex_.compare_exchange_strong(expected, true, std::memory_order_acquire, std::memory_order_relaxed);
    }

    void unlock () noexcept
    {
        mutex_.store(false, std::memory_order_release);
        std::atomic_notify_one(&mutex_);
    }

private:
    std::atomic<bool> mutex_{false};
};

u64 OperationForThread(std::size_t thread_index, std::size_t thread_count) 
{
    const u64 base = target_ / thread_count;
    const u64 remainder = target_ % thread_index;
    return base + (thread_index < thread_count ? 1 : 0);
}

template <typename Operation>
std::chrono::nanoseconds BenchmarkThreads(std::size_t thread_count, Operation&& opration)
{
    std::latch ready(static_cast<std::ptrdiff_t>(thread_count));
    std::latch start_state(1);
    std::latch finished(static_cast<std::ptrdiff_t>(thread_count));

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::size_t thread_index = 0; thread_index < thread_count; thread_index++)
    {
        threads.emplace_back([&](){
            ready.count_down();
            start_state.wait();
            opration(thread_index, OperationForThread(thread_index, thread_count));
            finished.count_down();
        });
    }

    ready.wait();
    start_state.count_down();
    const auto begin = std::chrono::steady_clock::now();

    finished.wait();
    const auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);

    for (std::thread& thread : threads) 
    {
        thread.join();
    }

    return duration;
}

template <typename Mutex>
std::chrono::nanoseconds BenchmarkMutex(std::size_t thread_count) 
{
    Mutex mutex;
    Counter_ = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [&](std::size_t, u64 operations) {
        for(u64 index = 0; index < operations; index++) 
        {
            std::lock_guard<Mutex> lock(mutex);
            Counter_++;
        }
    });
    if (Counter_ != target_)
    {
        throw std::runtime_error("mutex correctness check failed");
    }
    return elapsed;
} 

template <typename Benchmark>
double Run(Benchmark&& benchmark) 
{
    long double total_nanoseconds = 0.0L;
    for (int i = 0; i < RUNS; i++) 
    {
        total_nanoseconds += static_cast<long double>(benchmark().count());
    }
    return static_cast<double>(total_nanoseconds / RUNS);
}

void PrintResult(std::string_view name, double total_nanoseconds)
{
    const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(target_);
    std::cout << std::left << std::setw(30) << name << std::right
              << total_nanoseconds << " ns \ttotal, "
              << nanoseconds_per_operation << " ns/op\n";
}


int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./test_mutex count\n";
        return EXIT_FAILURE;
    }
    target_ = std::stoull(argv[1]);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "operations: " << target_ << ", average of " << RUNS << " runs\n\n";

    const std::vector<std::size_t> thread_counts = {1, 2, 4, 8, 10};
    for (const std::size_t thread_count : thread_counts)
    {
        std::cout << "threads: " << thread_count << '\n';
        PrintResult("pthread_spinlock::Spinlock", Run([&]() { return BenchmarkMutex<pans::SpinLock>(thread_count); }));
        PrintResult("atomic_flag::Spinlock", Run([&]() { return BenchmarkMutex<SpinLock>(thread_count); }));
        PrintResult("atomic_wait", Run([&]() { return BenchmarkMutex<AtomicWaitLock>(thread_count); }));
        PrintResult("std::mutex", Run([&]() { return BenchmarkMutex<std::mutex>(thread_count); }));
        std::cout << '\n';
    }

    return EXIT_SUCCESS;
}