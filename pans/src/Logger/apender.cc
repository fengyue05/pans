#include "Logger/appender_impl.h"
#include "pans/Logger/Log_level.h"
#include "pans/Logger/appender.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>
#include <unistd.h>
#include <utility>

#include <pans/macros.h>

namespace pans {

Appender::Appender(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{}

void Appender::setLevel(LogLevel::Level level) noexcept
{
    impl_->setLevel(level);
}

LogLevel::Level Appender::getLevel() const noexcept
{
    return impl_->getLevel();
}

void Appender::flush() 
{
    impl_->flush();
}

void Appender::sync()
{
    impl_->sync();
}

void Appender::Impl::append(LogLevel::Level level, std::string_view formatted_record) noexcept
{
    if (static_cast<u8>(level) < static_cast<u8>(this->getLevel())) 
    {
        return;   
    }
    // std::mutex::lock()函数，在极端情况下会抛出std::system_error异常, lock_guard的构造函数也不是noexcept的
    try
    {
        std::lock_guard<std::mutex> lock(mutex_);
        writeUnlocked(formatted_record);
        if (level == LogLevel::Level::LOG_LV_FATAL)
        {
            flushUnlocked();
        }
    }
    catch(...)
    {
        constexpr std::string_view message = "pans logger: appender operation failed\n";
        std::fwrite(message.data(), 1, sizeof(message), stderr);
    }
}

void Appender::Impl::setLevel(LogLevel::Level level) noexcept
{
    level_.store(level, std::memory_order_release);
}

LogLevel::Level Appender::Impl::getLevel() const noexcept
{
    return level_.load(std::memory_order_acquire);
}

// 把数据从程序的内存刷到操作系统缓冲区
void Appender::Impl::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    flushUnlocked();
}

void Appender::Impl::sync()
{
    std::lock_guard<std::mutex> lock(mutex_);
    syncUnlocked();
}

class StdoutAppenderImpl final : public Appender::Impl
{
protected:
    void writeUnlocked(std::string_view formatted_record) noexcept override
    {
        std::cout.write(formatted_record.data(), static_cast<std::streamsize>(formatted_record.size()));
    }

    void flushUnlocked() noexcept override
    {
        std::cout.flush();
    }

    // 这个是输出到终端，不需要同步到磁盘，所以还是flush（也可以不写）
    void syncUnlocked() noexcept override
    {
        flushUnlocked();
    }
};

class FileAppenderImpl final : public Appender::Impl
{
public:
    explicit FileAppenderImpl(const std::string& file_name)
    {
        if (file_name.empty())
        {
            throw std::invalid_argument("logger file name cannot be empty");
        }
        /*
        a（append）：追加写入，保留文件原来的内容，写入的数据放到文件末尾。文件不存在时会创建。
        b（binary）：二进制模式，不进行文本转换，例如 Windows 下不会把 \n 自动转换成 \r\n。Linux 下文本模式和二进制模式通常没有区别。
        */
        file_ = std::fopen(file_name.c_str(), "ab");
        if (file_ == nullptr) 
        {
            // 通用错误类别，用来说明一个错误码应该按照什么规则解释，常用于解释 errno
            // 错误码只是数字，需要搭配错误类别才能确定含义
            throw std::system_error(errno, std::generic_category(), "failed to open logger file: " + file_name);
        }
    }

    ~FileAppenderImpl() override 
    {
        if (file_ != nullptr)
        {
            std::fflush(file_);
            std::fclose(file_);
        }
    }

protected:
    void writeUnlocked(std::string_view formatted_record) noexcept override
    {
        std::fwrite(formatted_record.data(), 1, formatted_record.size(), file_);
    }

    void flushUnlocked() noexcept override
    {
        std::fflush(file_);
    }

    void syncUnlocked() noexcept override
    {
        flushUnlocked();
        ::fdatasync(::fileno(file_));
    }

private:
    std::FILE* file_ = nullptr;
};

AppenderPtr detail::AppenderAccess::MakeStdoutAppender()
{
    return AppenderPtr(new Appender(std::make_unique<StdoutAppenderImpl>()));
}

AppenderPtr detail::AppenderAccess::MakeFileAppender(std::string file_name)
{
    return AppenderPtr(new Appender(std::make_unique<FileAppenderImpl>(file_name)));
}

void detail::AppenderAccess::Append(const AppenderPtr& appender, LogLevel::Level level, std::string_view formatted_record) 
{
    if (appender != nullptr) 
    {
        appender->impl_->append(level, formatted_record);
    }
}

AppenderPtr MakeStdoutAppender()
{
    return detail::AppenderAccess::MakeStdoutAppender();
}

AppenderPtr MakeFileAppender(std::string file_name) 
{
    return detail::AppenderAccess::MakeFileAppender(std::move(file_name));
}

}