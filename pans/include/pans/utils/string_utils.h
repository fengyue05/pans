#ifndef PANS_INCLUDE_PANS_UTILS_STRING_UTILS_H
#define PANS_INCLUDE_PANS_UTILS_STRING_UTILS_H

#include <cstring>
#include <cstdarg>
#include <string_view>

#include <pans/export.h>

namespace pans{

/**
 * @brief StringUtils类，来承载通用字符串操作。
 */
class PANS_API StringUtils final
{
public:
    // 使用delete禁用构造函数，它是一个纯静态类，所有的方法都是静态方法
    StringUtils() = delete;

    /**
     * @brief 宽字符转为UTF-8
     * 在Windows平台上，它会将UTF-16 宽字符wchar_t转成UTF8
     * 在Linux平台上，它会将UTF-32宽字符wchar_t转成UFT8
     * 
     * @param text 
     * @return std::string 转换失败时触发断言，返回空字符串
     */
    [[nodiscard]] static std::string WStringToString(std::wstring_view text) noexcept;

    /**
     * @brief UTF8字符串往宽字符转换
     * 在Windows平台上，wchar_t宽字符是UTF-16, 在Linux平台上，wchar_t宽字符是UTF-32
     * @param text 
     * @return std::wstring 转换失败时触发断言，返回空字符串
     */
    [[nodiscard]] static std::wstring StringToWString(std::string_view text) noexcept;
    
};

}

#endif
