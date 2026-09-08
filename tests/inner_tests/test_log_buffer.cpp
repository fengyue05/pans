#include <pans/macros.h>
#include "../../pans/src/Logger/buffer.h"
#include "../../pans/src/Logger/buffer_config.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <cstddef>
#include <iomanip>
#include <sstream>

constexpr int RUNS = 7;
u64 target_ = 0;
// 定义一个BENCHMARK_SINK变量，收集测量数据，让它产生一个编译器不可忽略的外部副作用，避免编译器把我们的逻辑优化掉
std::atomic<u64> BENCHMARK_SINK{0};

u64 Observe(std::string_view value) noexcept
{
    return static_cast<u64>(value.size()) +
           static_cast<unsigned char>(value.front()) +
           static_cast<unsigned char>(value[value.size() / 2]) +
           static_cast<unsigned char>(value.back());
}

u64 WriteWithSmallStreamBuffer (std::string_view message)
{
    pans::detail::InlineBuffer<pans::detail::LOG_MESSAGE_INLINE_CAPACITY> buffer;
    pans::detail::SmallStreamBuffer<pans::detail::LOG_MESSAGE_INLINE_CAPACITY> stream_buffer(buffer);
    std::ostream stream(&stream_buffer);
    stream << message;
    return Observe(buffer.view());
}

u64 WriteWithStringStream (std::string_view message)
{
    std::stringstream stream;
    stream << message;
    return Observe(stream.view());
}

template <typename Operation>
std::chrono::nanoseconds BenchmarkWrites(Operation&& operation)
{
    u64 checkSum = 0;
    const auto begin = std::chrono::steady_clock::now();
    for (u64 iteration = 0; iteration < target_; iteration++)
    {
        checkSum += operation();
    }
    const auto end = std::chrono::steady_clock::now();
    // “消费掉结果”，防止编译器认为计算结果没用然后把相关代码优化掉。
    BENCHMARK_SINK.fetch_xor(checkSum, std::memory_order_relaxed);
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
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

void PrintResult(std::string_view name, double total_nanoseconds)
{
    std::cout << std::left << std::setw(30) << name << std::right
              << total_nanoseconds << " ns \ttotal, "
              << total_nanoseconds / static_cast<double>(target_) << " ns/op\n";
}

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        std::cerr << "Usage: ./test_log_buffer count\n";
        return 1;
    }
    target_ = std::stoull(argv[1]);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "operations: " << target_ << ", average of " << RUNS << " runs\n\n";
    constexpr std::array<std::size_t, 6> MESSAGE_SIZES = {50, 100, 200, 500, 1000, 2048};
    for(const std::size_t message_size : MESSAGE_SIZES)
    {
        const std::string message(message_size, 'x');
        const double small_stream_buffer_nanoseconds = Run([&]() {
            return BenchmarkWrites([&]() { return WriteWithSmallStreamBuffer(message); });
        });
        const double stringstream_nanoseconds = Run([&]() {
            return BenchmarkWrites([&]() { return WriteWithStringStream(message); });
        });

        std::cout << "message: " << message_size << " characters"  << '\n';
        PrintResult("SmallStreamBuffer", small_stream_buffer_nanoseconds);
        PrintResult("std::stringstream", stringstream_nanoseconds);
        std::cout << "speedup: " << stringstream_nanoseconds / small_stream_buffer_nanoseconds << "x\n\n";
    }

    return 0;
}

