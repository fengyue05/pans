#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string_view>
#include <vector>
#include <pans/types.h>

#include <cxxabi.h>
#include <elfutils/libdwfl.h> // 主要用于读取和分析 ELF 文件、DWARF 调试信息、进程中的模块和地址信息
#include <execinfo.h>
#include <unistd.h>

#include <pans/utils/system_utils.h>

namespace pans {

std::chrono::steady_clock::duration GetElapsedTime() noexcept
{
    static const auto START_TIME = std::chrono::steady_clock::now();
    return std::chrono::steady_clock::now() - START_TIME;
}

u64 GetFiberId() noexcept
{
    return 0;
}

namespace {

// 这里需要的不是“释放内存的代码”本身，而是一个可作为类型传给模板的删除器
struct FreeDeleter final 
{
    void operator()(void* ptr) const noexcept
    {
        std::free(ptr);
    }
};

// 地址 → 某个共享库 → 某个函数 → foo.cpp:42  Dwfl 就是组织这次查询的入口
struct DwflDeleter final
{
    void operator()(Dwfl* dwfl) const noexcept
    {
        dwfl_end(dwfl);
    }
};

/*
这里的 ELF 模块，可以先理解为：程序运行时加载的一份可执行代码或数据。
ELF 是 Linux 上常见的二进制文件格式。一个进程里可能有多个 ELF 文件参与运行，例如：
my_app          主程序
libc.so.6       C 标准库
libstdc++.so.6  C++ 标准库
libfoo.so       你的共享库
在 Dwfl 里，它们各自可以对应一个 Dwfl_Module。模块记录了“这份文件在所分析的地址空间里位于哪里”。所以拿到一个地址时，Dwfl 会先找它落在哪个模块中，再用该模块的符号和调试信息查函数名、源码行号。
可以把关系记成：
Dwfl（整个会话）
 ├─ Dwfl_Module（主程序）
 ├─ Dwfl_Module（libc.so.6）
 └─ Dwfl_Module（libfoo.so）
**ELF 文件是磁盘上的文件；Dwfl_Module 是 Dwfl 对这个模块的描述句柄。**两者有关，但不是同一个东西。
*/

const Dwfl_Callbacks DWFL_CALLBACKS{
    dwfl_linux_proc_find_elf,  // 作用是帮助它通过当前 Linux 进程的信息找到 ELF 模块
    dwfl_standard_find_debuginfo, // 用来寻找 DWARF 调试信息
    nullptr,
    nullptr,
};

// 输入一个程序地址，尽量返回它对应的源代码位置
/*
backtrace()
    ↓
拿到 void* 地址数组

backtrace_symbols()
    ↓
地址 -> 模块名 + mangled symbol

DemangleBacktraceSymbol()
    ↓
mangled symbol -> C++可读函数名


同时：

void* 地址
    ↓
SourceLocationParser::parse()
    ↓
libdwfl + DWARF
    ↓
源文件名 + 行号
*/
class SourceLocationParser final
{
public:
    SourceLocationParser()
        : dwfl_(dwfl_begin(&DWFL_CALLBACKS))
    {
        if (!dwfl_ || dwfl_linux_proc_report(dwfl_.get(), getpid()) || dwfl_report_end(dwfl_.get(), nullptr, nullptr))
        {
            dwfl_.reset();
        }
    }

    [[nodiscard]] std::string parse(void* frame, bool is_return_address) const 
    {
        if (!dwfl_)
        {
            return {};
        }
        // std::uintptr_t能够无损容纳指针值的无符号整数类型
        auto address = static_cast<Dwarf_Addr>(reinterpret_cast<std::uintptr_t>(frame));
        if (is_return_address && address > 0)
        {
            // 因为函数调用时，栈上保存的地址通常是：call 指令执行完以后，下一条指令的位置
            address--;
        }
        
        Dwfl_Line* line = dwfl_getsrc(dwfl_.get(), address);
        if (line == nullptr)
        {
            return {};
        }

        int line_number = 0;
        const char* file_name = dwfl_lineinfo(line, nullptr, &line_number, nullptr, nullptr, nullptr);

        if (file_name == nullptr || line_number <= 0)
        {
            return {};
        }

        return std::string(file_name) + ":" + std::to_string(line_number);
    }

private:
    std::unique_ptr<Dwfl, DwflDeleter> dwfl_;
};

// 模块名（符号名+偏移）[地址]
// ./a.out(_ZN4pans6Logger3LogEv+0x25)[0x401234]
/*
普通 ELF 的符号表通常能告诉你：
函数 Logger::log 从哪个地址开始
但不一定能告诉你：
0x401025 对应 Logger.cc 第 73 行
后者需要更详细的 DWARF 调试信息。
所以：
符号表
地址 → 函数名 + 偏移

DWARF
地址 → 源文件 + 行号

backtrace_symbols()
和：
dwfl_getsrc()
dwfl_lineinfo()
前者主要得到类似：
Logger::log()+0x25
后者进一步得到：
Logger.cc:73
*/
std::string DemangleBacktraceSymbol (const char* symbol)
{
    const std::string_view text(symbol == nullptr ? "" : symbol);
    const auto name_begin = text.find('(');
    if (name_begin == std::string_view::npos) // 格式不对
    {
        return std::string(text);
    }

    const auto mangled_begin = name_begin + 1;
    // ./a.out(_ZN4pans6Logger3logEv+0x25) [0x401234]
    auto name_end = text.find('+', mangled_begin);
    if (name_end == std::string::npos)
    {
        // 有些格式是./a.out(function)
        name_end = text.find(')', mangled_begin);
    }
    // 结束位置找不到或者函数名是空
    if (name_end == std::string_view::npos || name_end == mangled_begin)
    {
        return std::string(text);
    }

    const std::string mangled_name(text.substr(mangled_begin, name_end - mangled_begin));
    int status = 0;
    // abi::__cxa_demangle() 属于 GCC/Clang 的 C++ ABI 接口   _ZN4pans6Logger3logEv 还原成pans::Logger::log()
    // __cxa_demangle()返回的 char* 是动态分配的，而且要求调用：free()
    // 两个 nullptr 不用我自己提供缓冲区   让 __cxa_demangle 自己分配
    std::unique_ptr<char, FreeDeleter> demangled_name(abi::__cxa_demangle(mangled_name.c_str(), nullptr, nullptr, &status));
    if (status == 0 && demangled_name)
    {
        return std::string(demangled_name.get());
    }
    return mangled_name;
}

}

std::string GetBacktrace(int size, int skip, const std::string& prefix)
{
    if (size <= 0)
    {
        return {};
    }
    // 获取当前线程的函数调用栈，把各层的返回地址存到 frames 中
    std::vector<void*> frames(static_cast<std::size_t>(size));
    const int frame_count = ::backtrace(frames.data(), size);
    if (frame_count <= 0)
    {
        return {};
    }

    // 将栈帧地址转换为可读的符号描述，并用智能指针管理返回的内存
    /*
    backtrace_symbols(...) 返回 char**，指向一个字符串指针数组，每个字符串描述一个栈帧，可能包含函数名、偏移量和地址。
    unique_ptr<T> 默认管理 T*，这里 T 是 char*，因此管理的是 char**。
    FreeDeleter 是自定义删除器，应该调用 free()，因为这块内存需要用 free() 释放，而不是 delete。
    */
    std::unique_ptr<char*, FreeDeleter> symbols(::backtrace_symbols(frames.data(), frame_count));
    if (!symbols)
    {
        return {};
    }

    std::ostringstream stream;
    const SourceLocationParser source_location_parser;
    // 在调用栈处理中，skip 表示跳过最前面的多少个栈帧，通常用来略过打印调用栈的辅助函数自身
    const int first_frame = std::clamp(skip, 0, frame_count);
    for (int i = first_frame; i < frame_count; i++) 
    {
        stream << prefix << "#" << i - first_frame << " " << DemangleBacktraceSymbol(symbols.get()[i]);
        const std::string source_location = source_location_parser.parse(frames[i], i > 0);
        if (!source_location.empty())
        {
            stream << "at" << source_location;
        }
        stream << '\n';
    }
    return stream.str();
}

}