#ifndef _PANS_SRC_LOGGER_LOG_RECORD_H_
#define _PANS_SRC_LOGGER_LOG_RECORD_H_

#include <chrono>
#include <string_view>
#include <pans/Logger/Log_level.h>
#include <pans/macros.h>

namespace pans::detail {

struct LogRecordView
{
    LogLevel::Level level_ = LogLevel::Level::LOG_DEBUG;
    std::string_view loggerName_; // 日志器名称
    std::string_view message_; // 日志内容
    std::chrono::system_clock::time_point timestamp_; // 时间戳
    std::chrono::system_clock::duration elapsed_;  // 起服已过时间
    u64 threadId_ = 0; 
    u64 fiberId_ = 0; 
    std::string_view threadName_; // 线程名
    std::string_view fileName_; // 文件名
    u32 line_ = 0; // 行号
};

}


#endif