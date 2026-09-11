#include <pans/Logger/Log_level.h>
#include <pans/Logger/Logger.h>
#include <pans/macros.h>

#include <cstddef>
#include <iosfwd>
#include <string_view>
#include <pans/Logger/appender.h>

namespace pans::detail {

class PANS_API LogLine final
{
public:
    LogLine(Logger& logger, LogLevel::Level level, u32 line, std::string_view file_name);
    ~LogLine() noexcept;

    LogLine(const LogLine&) = delete;
    LogLine& operator=(const LogLine&) = delete;
    LogLine(LogLine&& ) = delete;
    LogLine& operator=(LogLine&&) = delete;

    [[nodiscard]] std::ostream& stream() noexcept;
private:
    // 这个地方不使用指针是因为每行日志都要构造一个LogLine，构造频率很高
    // 这个Impl最后是放在我们自定义的缓冲区里面的，如果都是指针的话，那么我们做栈的优化的努力就白费了
    struct Impl;
    static constexpr std::size_t LOG_LINE_IMPL_SIZE = 1024;
    // 内存起始地址对其，原因就是我们使用的是byte，存的是Impl
    alignas(std::max_align_t) std::byte implStorage_[LOG_LINE_IMPL_SIZE];

    [[nodiscard]] Impl& getImpl() noexcept;
};

// LogPrintf("....");
PANS_API void LogPrintf(Logger& logger, LogLevel::Level level, u32 line, std::string_view file_name, const char* format, ...);

}