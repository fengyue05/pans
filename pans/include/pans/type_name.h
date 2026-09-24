#ifndef _PANS_INCLUDE_PANS_NAME_H_
#define _PANS_INCLUDE_PANS_NAME_H_

#include <cstddef>
#include <source_location>
#include <string_view>

namespace pans::detail 
{
template<typename T>
[[nodiscard]] consteval std::string_view TypeNameProbe() noexcept
{
    return std::source_location::current().function_name();
}

template <typename T>
[[nodiscard]] consteval std::string_view ExtractTypeName() noexcept
{
    constexpr std::string_view signature = TypeNameProbe<T>();
    // GCC: consteval std::string_view pans::detail::TypeNameProbe() [with T = int;...]
    // Clang: std::string_view pans::detail::TypeNameProbe() [T = int]

    constexpr std::string_view marker = "T = ";
    constexpr std::size_t marker_pos = signature.find(marker);

    if constexpr (marker_pos == std::string_view::npos)
    {
        return signature;
    }
    else 
    {
        constexpr std::size_t begin = marker_pos + marker.size();
        constexpr std::size_t semicolon = signature.find(';', begin);
        constexpr std::size_t bracket = signature.find(']');
        constexpr std::size_t end = semicolon != std::string_view::npos ? semicolon : bracket;
        if constexpr (end == std::string_view::npos || end <= begin)
        {
            return signature.substr(begin);
        }
        else
        {
            return signature.substr(begin, end - begin);
        }
    }
}

template<typename T>
inline constexpr std::string_view TYPE_NAME = ExtractTypeName<T>();
}

namespace pans 
{
template <typename T>
[[nodiscard]] consteval std::string_view TypeName() noexcept
{
    return detail::TYPE_NAME<T>;
}
}

#endif