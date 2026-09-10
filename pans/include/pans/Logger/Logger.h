#ifndef _PANS_INCLUDE_PANS_LOGGER_LOGGER_H_
#define _PANS_INCLUDE_PANS_LOGGER_LOGGER_H_

#include <memory>
#include <string>
#include <string_view>
#include <pans/export.h>
#include <pans/Logger/appender.h>
#include <pans/Logger/Log_level.h>

namespace pans {

// 用来代理我们的内部权限，严格控制谁可以访问我们的Logger私有实现
namespace detail {
class LoggerAccess;
} // namespace detail

class PANS_API Logger final 
{
public:
    class Impl;
    explicit Logger(std::string name = "root");
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    [[nodiscard]] bool shouldLog(LogLevel::Level level) const noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;
    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] std::string_view getName() const noexcept;

    void setFormatter(std::string_view pattern);
    [[nodiscard]] std::string getFormatterPattern() const;
    void addAppender(AppenderPtr appender);
    void removeAppender(const AppenderPtr& appender);
    void clearAppenders();

    void flush();
    void sync();
private:
    std::unique_ptr<Impl> impl_;
    friend class detail::LoggerAccess;
};

using LoggerPtr = std::shared_ptr<Logger>;

[[nodiscard]] PANS_API LoggerPtr GetRootLogger();
[[nodiscard]] PANS_API LoggerPtr GetLogger(std::string_view name);


}

#endif