#ifndef _PANS_INCLUDE_PANS_TYPES_H_
#define _PANS_INCLUDE_PANS_TYPES_H_

#include <cstdint>

#define INVALID64 (~0ULL)
#define INVALID32 0xFFFFFFFF
#define INVALID16 0xFFFF
#define INVALID8  0xFF

#define MAX_U8   0xFF
#define MAX_U16  0xFFFF
#define MAX_U32  0xFFFFFFFF
#define MAX_U64  (~0ULL)

using u8 = std::uint8_t;
using s8 = std::int8_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u64 = std::uint64_t;
using s64 = std::int64_t;


#endif