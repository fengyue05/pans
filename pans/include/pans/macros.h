#ifndef _INCLUDE_PANS_MACROS_H_
#define _INCLUDE_PANS_MACROS_H_

#include <cassert>
#include <iostream>
#include <pans/Logger/Log.h>
#include <pans/utils/system_utils.h>

#define PANS_ASSERT(x) \
    if(!(x)) [[unlikely]]\
    { \
        PANS_LOG_FATAL(PANS_LOG_ROOT()) << "Assert failed: " << #x << "\nStacktrace:\n" << pans::GetBacktrace(); \
        assert(x); \
    }

// 宏2，在宏1的基础上，增加了一个info参数，用于输出额外的信息
#define PANS_ASSERT2(x, w) \
    if(!(x)) [[unlikely]]\
    { \
        PANS_LOG_FATAL(PANS_LOG_ROOT()) << "Assert " << #x << " failed. [" << w << "].\nStacktrace:\n" << pans::GetBacktrace();\
        assert(x);\
    }

#define ASSERT_RETVAL(x, val) \
    do{\
        if(x) [[likely]] break;\
        PANS_ASSERT(x);\
        return val;\
    }while(0)

#define ASSERT_RETVAL2(x, val, info) \
    do{\
        if(x) [[likely]] break;\
        PANS_ASSERT2(x, info);\
        return val;\
    }while(0)

#define ASSERT_RETNONE(x) \
    do{\
        if(x) [[likely]] break;\
        PANS_ASSERT(x);\
        return;\
    }while(0)

#define ASSERT_RETNONE2(x, info) \
    do{\
        if(x) [[likely]] break;\
        PANS_ASSERT2(x, info);\
        return;\
    }while(0)

#define ASSERT_NOEFFECT(x)\
    do{\
        if(x) [[likely]] break;\
        PANS_ASSERT(x);\
    }while(0)

#define ASSERT_NOEFFECT2(x, info)\
    do{\
        if(x) [[likely]] break;\
        PANS_ASSERT2(x, info);\
    }while(0)

#define ASSERT_CONTINUE(x)\
    if(!(x)) [[unlikely]]{\
        PANS_ASSERT(x);\
        continue;\
    }else{}

#define ASSERT_CONTINUE2(x, info)\
    if(!(x)) [[unlikely]]{\
        PANS_ASSERT2(x, info);\
        continue;\
    }else{}

#define ASSERT_BREAK(x)\
    if(!(x)) [[unlikely]]{\
        PANS_ASSERT(x);\
        break;\
    }else{}

#define ASSERT_BREAK2(x, info)\
    if(!(x)) [[unlikely]]{\
        PANS_ASSERT2(x, info);\
        break;\
    }else{}


#endif
