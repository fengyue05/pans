#ifndef PANS_SRC_LOGGER_BUFFER_CONFIG_H
#define PANS_SRC_LOGGER_BUFFER_CONFIG_H

#include <cstddef>

namespace pans::detail {

// 一条日志消息，先预留 128 字节的内联空间。
inline constexpr std::size_t LOG_MESSAGE_INLINE_CAPACITY = 128;

// 用于 printf 风格格式化时，先准备 128 字节的内联缓冲区。
inline constexpr std::size_t PRINTF_FORMAT_INLINE_CAPACITY = 128;

// 最终格式化出来的完整日志记录，先准备 256 字节内联空间。
inline constexpr std::size_t FORMATTED_RECORD_INLINE_CAPACITY = 256;

}

#endif 
