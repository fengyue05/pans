#ifndef _PANS_SRC_LOGGER_LOGGER_IMPL_H_
#define _PANS_SRC_LOGGER_LOGGER_IMPL_H_

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

#include <pans/Logger/Logger.h>
#include "Logger/formatter.h"
#include "Logger/log_record.h"

namespace pans {
class Logger::Impl
{
public:
    explicit Impl(std::string name);
    [[nodiscard]] bool shouldLog(LogLevel::Level level) const noexcept;
    void submit(const detail::LogRecordView& record) noexcept;
    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;
    [[nodiscard]] std::string_view getName() const noexcept;

    void setFormatter(std::shared_ptr<const detail::Formatter> formatter);
    [[nodiscard]] std::shared_ptr<const detail::Formatter> getFormatter() const noexcept;
    void addAppender(AppenderPtr appender);
    void removeAppender(const AppenderPtr& appender);
    void clearAppenders();

    void flush();
    void sync();

    void setRoot(const LoggerPtr& root) noexcept;

private:
    std::string name_;
    std::atomic<LogLevel::Level> level_{LogLevel::Level::LOG_DEBUG};

    mutable std::shared_mutex mutex_;
    std::shared_ptr<const detail::Formatter> formatter_;
    std::vector<AppenderPtr> appenders_;

    LoggerPtr root_;
};

namespace detail {
/**
 * LoggerAccess本质上是一个内部权限访问的桥梁，
 * 用于在不扩大Logger公共API的情况下，访问其私有实现
 */
class LoggerAccess final 
{
public:
    static void Submit(Logger& logger, const LogRecordView& record) noexcept;
    static void SetRoot(Logger& logger, const LoggerPtr& root) noexcept;
};
}
}

#endif