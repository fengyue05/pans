#ifndef _PANS_INCLUDE_PANS_LOGGER_LOG_LEVEL_H_
#define _PANS_INCLUDE_PANS_LOGGER_LOG_LEVEL_H_

#include <string_view>
#include <pans/export.h>
#include <pans/macros.h>

namespace pans {

class PANS_API LogLevel final 
{
public:
    enum class Level : std::uint8_t
    {
        LOG_LV_DEBUG, // 调试细节
        LOG_LV_INFO,  // 记录正常运行的信息
        LOG_LV_WARN,  // 可能存在的问题
        LOG_LV_ERROR, // 操作失败，需要检查
        LOG_LV_FATAL, // 程序如果继续运行会造成损失
        LOG_LV_OFF,   // 关闭日志
    };

    [[nodiscard]] static std::string_view ToString(Level level) noexcept;
    [[nodiscard]] static LogLevel::Level FromString(std::string_view value) noexcept;
};

}

#endif