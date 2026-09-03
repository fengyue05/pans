#include <atomic>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <latch>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <vector>
#include <pans/macros.h>

// 一个测试跑多次，然后取平均值
const int RUNS = 7;

u64 target_ = 0; // 一次执行的次数
u64 Counter_ = 0; // 每一次用来累加计数
std::atomic<u64> atomicCounter_{0};
std::mutex mutex_;
std::shared_mutex sharedMutex_;


// 把总任务数 target 尽量平均地分配给 thread_count 个线程，并返回 thread_index（线程的编号） 这个线程应该执行多少次操作。
u64 OperationsForThread(std::size_t thread_index, std::size_t thread_count) 
{
    const u64 base = target_ / thread_count;
    const u64 remainder = target_ % thread_count;
    return base + (thread_index < remainder ? 1 : 0);
}

// 把一个线程跑很多次
template <typename Operation>
std::chrono::nanoseconds BenchmarkThreads(std::size_t thread_count, Operation&& operation)
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
            start_state.wait(); // 等待所有的线程都准备好，以防有的线程都没有运行起来
            operation(thread_index, OperationsForThread(thread_index, thread_count));
            finished.count_down();
        });
    }

    ready.wait();
    const auto begin = std::chrono::steady_clock::now();
    start_state.count_down();
    finished.wait();
    const auto end = std::chrono::steady_clock::now();

    for (std::thread& thread : threads) 
    {
        thread.join();
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}

std::chrono::nanoseconds BenchmarkMutex(std::size_t thread_count)
{
    Counter_ = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, u64 operations) {
        for(u64 index = 0; index < operations; index++) 
        {
            std::lock_guard<std::mutex> lock(mutex_);
            Counter_++;
        }
    });
    return elapsed;
}

std::chrono::nanoseconds BenchmarkSharedMutex(std::size_t thread_count)
{
    Counter_ = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, u64 operations){
        for(u64 index = 0; index < operations; index++) 
        {
            std::unique_lock<std::shared_mutex> lock(sharedMutex_);
            Counter_++;
        }
    });
    return elapsed;
}

std::chrono::nanoseconds BenchmarkAtomic(std::size_t thread_count)
{
    atomicCounter_.store(0, std::memory_order_relaxed); // 不保证同步
    const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, u64 operations){
        for(u64 index = 0; index < operations; index++)
        {
            atomicCounter_.fetch_add(1, std::memory_order_relaxed);
        }
    });
    return elapsed;
}

std::chrono::nanoseconds BenchmarkAtomicCas(std::size_t thread_count)
{
    atomicCounter_.store(0, std::memory_order_relaxed);
    const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, u64 operations) {
            for(u64 index = 0; index < operations; ++index)
            {
                u64 expected = atomicCounter_.load(std::memory_order_relaxed);
                while(!atomicCounter_.compare_exchange_weak(expected, expected + 1, std::memory_order_relaxed, std::memory_order_relaxed))
                {
                }
            }
        });
    return elapsed;
}

template <typename Benchmark>
double Run(Benchmark&& benchmark)
{
    long double total_nanoseconds = 0.0L;
    for (int run = 0; run < RUNS; run++) 
    {
        total_nanoseconds += static_cast<long double>(benchmark().count());
    }
    return static_cast<double>(total_nanoseconds / RUNS);
}


// string_view之查看字符串，但是不拥有
void PrintResult(std::string_view name, double total_nanoseconds) 
{
    const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(target_);
    std::cout << std::left << std::setw(14) << name << std::right 
            << total_nanoseconds << " ns \ttotal, " << nanoseconds_per_operation << " ns/op\n";
}

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        std::cerr << "Usage: ./test_lock count\n";
        return -1;
    }
    target_ = std::atoi(argv[1]);
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "operations: " << target_ << ", average of " << RUNS << " runs\n\n";
    const std::vector<std::size_t> thread_counts = {1, 2, 4, 8, 10};
    for(const std::size_t thread_count : thread_counts)
    {
        std::cout << "threads: " << thread_count << '\n';
        PrintResult("mutex",            Run([&]() { return BenchmarkMutex(thread_count); }));
        PrintResult("shared mutex",     Run([&]() { return BenchmarkSharedMutex(thread_count); }));
        PrintResult("atomic",           Run([&]() { return BenchmarkAtomic(thread_count); }));
        PrintResult("atomic CAS",       Run([&]() { return BenchmarkAtomicCas(thread_count); }));
        std::cout << '\n';
    }

    return 0;
}
