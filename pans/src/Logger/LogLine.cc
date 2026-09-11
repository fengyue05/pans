#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <new>
#include <ostream>
#include <thread>
#include <vector>

#include <pans/Logger/Log.h>
#include <pans/utils/system_utils.h>
#include <pans/utils/thread_utils.h>

#include "Logger/buffer.h"
#include "Logger/buffer_config.h"
#include "Logger/logger_impl.h"
#include "Logger/log_record.h"

namespace pans::detail {

struct LogLine::Impl final
{
    Impl(Logger& logger, LogLevel::Level level, u32 line, std::string_view filename)
        : logger_(logger)
        , level_(level)
        , line_(line)
        , filename_(filename)
        , timestamp_(std::chrono::system_clock::now())
        , elapsed_(GetElapsedTime())
        , threadId_(GetThreadId())
        , fiberId_(GetFiberId())
        , threadName_(GetThreadName())
        , streamBuffer_(inlineBuffer_)
        , stream_(&streamBuffer_)
    {}

    [[nodiscard]] LogRecordView getRecord() const noexcept
    {
        return {
            level_,
            logger_.getName(),
            inlineBuffer_.view(),
            timestamp_,
            elapsed_,
            threadId_,
            fiberId_,
            threadName_,
            filename_,
            line_,
        };
    }

    Logger& logger_;
    LogLevel::Level level_;
    u32 line_ = 0;
    std::string_view filename_;
    std::chrono::system_clock::time_point timestamp_;
    std::chrono::steady_clock::duration elapsed_;
    u64 threadId_ = 0;
    u64 fiberId_ = 0;
    std::string_view threadName_;
    InlineBuffer<LOG_MESSAGE_INLINE_CAPACITY> inlineBuffer_;
    SmallStreamBuffer<LOG_MESSAGE_INLINE_CAPACITY> streamBuffer_;
    std::ostream stream_;
};

LogLine::LogLine(Logger& logger, LogLevel::Level level, u32 line, std::string_view file_name)
{
    static_assert(sizeof(Impl) <= LOG_LINE_IMPL_SIZE, "LogLine inline implementation storage is too small");
    static_assert(alignof(Impl) <= alignof(std::max_align_t), "LogLine implementation requires excessive alignment");
    // 按照固定的内存地址创建对象，这里是在Impl的起始地址上创建implStorage，把struct Impl的数据全部放在这个数组里面
    std::construct_at(reinterpret_cast<Impl*>(implStorage_), logger, level, line, file_name);
}

LogLine::Impl& LogLine::getImpl() noexcept
{
    return *std::launder(reinterpret_cast<Impl*>(implStorage_));
}

LogLine::~LogLine() noexcept
{
    Impl& impl = getImpl();
    LoggerAccess::Submit(impl.logger_, impl.getRecord());
    std::destroy_at(&impl);
}

std::ostream& LogLine::stream() noexcept
{
    return getImpl().stream_;
}

void LogPrintf(Logger &logger, LogLevel::Level level, u32 line, std::string_view file_name, const char *format, ...)
{
    LogLine log_line(logger, level, line, file_name);
    if (format == nullptr)
    {
        log_line.stream() << "<null-format>";
        return;
    }

    std::array<char, PRINTF_FORMAT_INLINE_CAPACITY> inline_buffer;

    va_list arguments;
    va_list arguments_copy;
    va_start(arguments, format);
    va_copy(arguments_copy, arguments);
    const int required_size = std::vsnprintf(inline_buffer.data(), inline_buffer.size(), format, arguments);

    if (required_size < 0)
    {
        va_end(arguments_copy);
        log_line.stream() << "<format-error>";
        return;
    }

    const std::size_t message_size = static_cast<std::size_t>(required_size);
    if (message_size < inline_buffer.size())
    {
        va_end(arguments_copy);
        log_line.stream().write(inline_buffer.data(), static_cast<std::streamsize>(message_size));
        return;
    }

    std::vector<char> overflow_buffer(message_size + 1);
    const int second_result = std::vsnprintf(overflow_buffer.data(), overflow_buffer.size(), format, arguments_copy);
    va_end(arguments_copy);
    if (second_result < 0)
    {
        log_line.stream() << "<format-error>";
        return;
    }

    log_line.stream().write(overflow_buffer.data(), static_cast<std::streamsize>(message_size));
}

}