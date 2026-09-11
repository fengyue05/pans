#include <cstdint>
#include <cstdio>

#include <pans/utils/string_utils.h>
#include <pans/macros.h>

namespace pans{

namespace{

// Unicode码点的最大值，合法范围是0x0~0x10FFFF,超过范围非法
constexpr u32 MAX_UNICODE_CODE_POINT = 0x10FFFF; 
constexpr u32 HIGH_SURROGATE_FIRST = 0xD800;
constexpr u32 HIGH_SURROGATE_LAST = 0xDBFF;
constexpr u32 LOW_SURROGATE_FIRST = 0xDC00;
constexpr u32 LOW_SURROGATE_LAST = 0xDFFF;

static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4, "StringUtils requires a UTF-16 or UTF-32 wchar_t");

// 检查是否是高代理项
[[nodiscard]] constexpr bool IsHighSurrogate(u32 value) noexcept
{
    return value >= HIGH_SURROGATE_FIRST && value <= HIGH_SURROGATE_LAST;
}

// 检查是否是低代理项
[[nodiscard]] constexpr bool IsLowSurrogate(u32 value) noexcept
{
    return value >= LOW_SURROGATE_FIRST && value <= LOW_SURROGATE_LAST;
}

void AppendUtf8(std::string& output, u32 code_point)
{
    if (code_point <= 0x7F)
    {
        output.push_back(static_cast<char>(code_point));
    }
    else if (code_point <= 0x7FF)
    {
        output.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    }
    else if (code_point <= 0xFFFF)
    {
        output.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    }
    else
    {
        output.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    }
}

[[nodiscard]] bool DecodeUtf8CodePoint(std::string_view text, std::size_t& offset, u32& code_point) noexcept
{
    const auto first = static_cast<unsigned char>(text[offset++]);
    if (first <= 0x7F)
    {
        code_point = first;
        return true;
    }

    u32 minimum_code_point = 0;
    std::size_t continuation_count = 0;
    if (first >= 0xC2 && first <= 0xDF)
    {
        code_point = first & 0x1FU;
        minimum_code_point = 0x80;
        continuation_count = 1;
    }
    else if (first >= 0xE0 && first <= 0xEF)
    {
        code_point = first & 0x0FU;
        minimum_code_point = 0x800;
        continuation_count = 2;
    }
    else if (first >= 0xF0 && first <= 0xF4)
    {
        code_point = first & 0x07U;
        minimum_code_point = 0x10000;
        continuation_count = 3;
    }
    else
    {
        return false;
    }

    if (text.size() - offset < continuation_count)
    {
        return false;
    }
    for (std::size_t i = 0; i < continuation_count; ++i)
    {
        const auto continuation = static_cast<unsigned char>(text[offset++]);
        if ((continuation & 0xC0U) != 0x80U)
        {
            return false;
        }
        code_point = (code_point << 6U) | (continuation & 0x3FU);
    }

    if (code_point < minimum_code_point || code_point > MAX_UNICODE_CODE_POINT ||
        IsHighSurrogate(code_point) || IsLowSurrogate(code_point))
    {
        return false;
    }
    return true;
}

}

std::string StringUtils::WStringToString(std::wstring_view text) noexcept
{
    try
    {
        std::string result;
        result.reserve(text.size());

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            u32 code_point = static_cast<u32>(text[i]);
            if constexpr (sizeof(wchar_t) == 2)
            {
                if (IsHighSurrogate(code_point))
                {
                    ++i;
                    ASSERT_RETVAL2(i < text.size(), {}, "宽字符串包含不完整的 UTF-16 代理对");
                    const u32 low_surrogate = static_cast<u32>(text[i]);
                    ASSERT_RETVAL2(IsLowSurrogate(low_surrogate), {}, "宽字符串包含非法 UTF-16 代理对");
                    code_point = 0x10000U + ((code_point - HIGH_SURROGATE_FIRST) << 10U) + (low_surrogate - LOW_SURROGATE_FIRST);
                }
                else
                {
                    ASSERT_RETVAL2(!IsLowSurrogate(code_point), {}, "宽字符串包含孤立的 UTF-16 低代理项");
                }
            }
            else
            {
                ASSERT_RETVAL2(code_point <= MAX_UNICODE_CODE_POINT && !IsHighSurrogate(code_point) && !IsLowSurrogate(code_point),
                               {}, "宽字符串包含非法 UTF-32 码点");
            }

            AppendUtf8(result, code_point);
        }
        return result;
    }
    catch (...)
    {
        ASSERT_NOEFFECT2(false, "宽字符串转换为 UTF-8 时发生异常");
        return {};
    }
}

std::wstring StringUtils::StringToWString(std::string_view text) noexcept
{
    try
    {
        std::wstring result;
        result.reserve(text.size());

        std::size_t offset = 0;
        while (offset < text.size())
        {
            u32 code_point = 0;
            const bool decoded = DecodeUtf8CodePoint(text, offset, code_point);
            ASSERT_RETVAL2(decoded, {}, "字符串包含非法 UTF-8 编码");
            if constexpr (sizeof(wchar_t) == 2)
            {
                if (code_point <= 0xFFFF)
                {
                    result.push_back(static_cast<wchar_t>(code_point));
                }
                else
                {
                    code_point -= 0x10000;
                    result.push_back(static_cast<wchar_t>(HIGH_SURROGATE_FIRST + (code_point >> 10U)));
                    result.push_back(static_cast<wchar_t>(LOW_SURROGATE_FIRST + (code_point & 0x3FFU)));
                }
            }
            else
            {
                result.push_back(static_cast<wchar_t>(code_point));
            }
        }
        return result;
    }
    catch (...)
    {
        ASSERT_NOEFFECT2(false, "UTF-8 转换为宽字符串时发生异常");
        return {};
    }
}


}

