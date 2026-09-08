#ifndef _PANS_INCLUDE_PANS_LOGGER_APPENDER_H_
#define _PANS_INCLUDE_PANS_LOGGER_APPENDER_H_

#include <memory>
#include <string>
#include <pans/export.h>
#include <pans/Logger/Log_level.h>

namespace pans {

// 用来代理我们的内部权限，严格控制谁可以访问我们的Appender私有实现
namespace detail {
class AppenderAccess;
}

class PANS_API Appender final 
{
public:
    // 声明一个内部实现内，类外定义(我们的输出地方可能是网络，可能是终端。。。不同的输出地方需要不同的处理，所以全部在impl的派生类里面实现)
    class Impl;

    ~Appender() = default;
    Appender(const Appender&) = delete;
    Appender(Appender&& ) = delete;
    Appender& operator=(const Appender&) = delete;
    Appender& operator=(Appender&&) = delete;

    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;

    // 把数据写入脏页，我们的std::endl就是换行加上flush
    void flush();
    // 把脏页的数据写入磁盘，达到数据同步的效果
    void sync();

private:
    // 而我们通过构造函数传进来对应的指针，就可以输出到对应的地方
    explicit Appender(std::unique_ptr<Impl> impl) noexcept; 
    std::unique_ptr<Impl> impl_;
    friend class detail::AppenderAccess;
};

using AppenderPtr = std::shared_ptr<Appender>;

[[nodiscard]] PANS_API AppenderPtr MakeStdoutAppender();
[[nodiscard]] PANS_API AppenderPtr MakeFileAppender(std::string file_name);

}

#endif