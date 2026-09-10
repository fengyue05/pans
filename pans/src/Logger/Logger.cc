#include "pans/Logger/Logger.h"
#include "Logger/formatter.h"
#include "Logger/logger_impl.h"
#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <pans/macros.h>
#include "Logger/appender_impl.h"
#include "pans/Logger/Log_level.h"
#include "pans/Logger/appender.h"

namespace pans {

constexpr std::string_view DEFAULT_LOG_PATTERN = "%d{%Y-%m-%d %H:%M:%S}.%u%Tthread=%t%Tfiber=%F%T[%p]%T%f:%l%T%m%n";

Logger::Logger(std::string name) 
    : impl_(std::make_unique<Impl>(std::move(name)))
{}

Logger::~Logger() = default;

bool Logger::shouldLog(LogLevel::Level level) const noexcept
{
    return impl_->shouldLog(level);
}

void Logger::setLevel(LogLevel::Level level) noexcept
{
    impl_->setLevel(level);
}

LogLevel::Level Logger::getLevel() const noexcept
{
    return impl_->getLevel();
}

std::string_view Logger::getName() const noexcept
{
    return impl_->getName();
}

void Logger::setFormatter(std::string_view pattern)
{
    auto formatter = std::make_shared<const detail::Formatter>(pattern);
    impl_->setFormatter(std::move(formatter));
}

std::string Logger::getFormatterPattern() const 
{
    const auto formatter = impl_->getFormatter();
    return formatter == nullptr ? std::string() : formatter->getPattern();
}

void Logger::addAppender(AppenderPtr appender)
{
    impl_->addAppender(appender);
}

void Logger::removeAppender(const AppenderPtr& appender)
{
    impl_->removeAppender(appender);
}

void Logger::clearAppenders()
{
    impl_->clearAppenders();
}

void Logger::flush()
{
    impl_->flush();
}

void Logger::sync()
{
    impl_->sync();
}

Logger::Impl::Impl(std::string name)
    : name_(name)
{
    if (name.empty())
    {
        throw std::invalid_argument("logger name cannot be empty");
    }
    formatter_ = std::make_shared<const detail::Formatter>(DEFAULT_LOG_PATTERN);
}

bool Logger::Impl::shouldLog(LogLevel::Level level) const noexcept
{
    if (static_cast<u8>(level) < static_cast<u8>(getLevel()))
    {
        return false;
    }
    {
        std::shared_lock<std::shared_mutex> lock(mutex_); // 读锁
        if (!appenders_.empty())
        {
            return true;
        }
    }
    return root_ != nullptr && root_->shouldLog(level);
}

void Logger::Impl::submit(const detail::LogRecordView& record) noexcept
{
    if (static_cast<u8>(record.level_) < static_cast<u8>(getLevel()))
    {
        return;
    }
    std::shared_lock<std::shared_mutex> lock(mutex_); // 读锁
    if (appenders_.empty()) 
    {
        lock.unlock();
        if (root_ != nullptr)
        {
            root_->impl_->submit(record);
        }
        return;
    }
    // InlineBuffer，它遇到长日志的时候，可能会使用堆内存
    // vector开辟堆内存的时候，会有极小概率抛bad_alloc异常，这里不能让异常逃逸出去
    try 
    {
        detail::FormattedRecordBuffer formatted_record;
        formatter_->format(record, formatted_record);
        for (const AppenderPtr& appender : appenders_)
        {
            detail::AppenderAccess::Append(appender, record.level_, formatted_record.view());
        }
    }
    catch(...)
    {
        constexpr std::string_view message = "pans logger : record formatting failed\n";
        std::fwrite(message.data(), 1, message.size(), stderr);
    }
}

void Logger::Impl::setLevel(LogLevel::Level level) noexcept
{
    level_.store(level, std::memory_order_release);
}

void Logger::Impl::setFormatter(std::shared_ptr<const detail::Formatter> formatter)
{
    ASSERT_RETNONE2(formatter != nullptr, "logger formatter cannot be null");
    std::unique_lock<std::shared_mutex> lock(mutex_); // 写锁
    formatter_ = std::move(formatter);
}

std::shared_ptr<const detail::Formatter> Logger::Impl::getFormatter() const noexcept
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return formatter_;
}

void Logger::Impl::addAppender(AppenderPtr appender)
{
    ASSERT_RETNONE2(appender != nullptr, "logger appender cannot be null");
    std::unique_lock<std::shared_mutex> lock(mutex_);
    appenders_.push_back(std::move(appender));
}

void Logger::Impl::removeAppender(const AppenderPtr& appender)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::erase(appenders_, appender);
}

void Logger::Impl::clearAppenders() 
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    appenders_.clear();
}

void Logger::Impl::flush()
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (appenders_.empty())
    {
        lock.unlock();
        if (root_ != nullptr)
        {
            root_->flush();
        }
        return;
    }
    for (const AppenderPtr& appender : appenders_)
    {
        appender->flush();
    }
}

void Logger::Impl::sync()
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (appenders_.empty())
    {
        lock.unlock();
        if (root_ != nullptr)
        {
            root_->sync();
        }
        return;
    }

    for (const AppenderPtr& appender : appenders_) 
    {
        appender->sync();
    }
}

void Logger::Impl::setRoot(const LoggerPtr& root) noexcept
{
    root_ = root;
}

void detail::LoggerAccess::Submit(Logger& logger, const LogRecordView& record) noexcept
{
    logger.impl_->submit(record);
}

void detail::LoggerAccess::SetRoot(Logger& logger, const LoggerPtr& root) noexcept
{
    logger.impl_->setRoot(root);
}

class LoggerManager final
{
public:
    LoggerManager()
        : root_(std::make_shared<Logger>("root"))
    {
        root_->addAppender(MakeStdoutAppender());
        loggers_.emplace("root", root_);
    }

    [[nodiscard]] LoggerPtr getRoot() const noexcept
    {
        return root_;
    }

    [[nodiscard]] LoggerPtr getLogger(std::string_view name)
    {
        ASSERT_RETVAL2(!name.empty(), nullptr, "logger name cannot be empty");
        std::lock_guard<std::mutex> lock(mutex_);
        const auto iterator = loggers_.find(std::string(name));
        if (iterator != loggers_.end())
        {
            return iterator->second;
        }

        auto logger = std::make_shared<Logger>(std::string(name));
        // 给新来的logger绑定根logger，以防止如果新的logger没有设置appender，那么我们就可以用根logger
        detail::LoggerAccess::SetRoot(*logger, root_);
        loggers_.emplace(logger->getName(), logger);
        return logger;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, LoggerPtr> loggers_;
    LoggerPtr root_;
};

LoggerManager& GetLoggerManager()
{
    static LoggerManager manager;
    return manager;
}

LoggerPtr GetRootLogger()
{
    return GetLoggerManager().getRoot();
}

LoggerPtr GetLogger(std::string_view name)
{
    return GetLoggerManager().getLogger(name);
}

}