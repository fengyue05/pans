#include "Logger/formatter.h"
#include "Logger/log_record.h"
#include <array>
#include <charconv>
#include <ctime>
#include <unordered_map>

#include <pans/macros.h>

namespace pans::detail {

constexpr std::string_view DEFAULT_DATE_FORMAT = "%Y-%m-%d %H:%M:%S";

Formatter::Formatter(std::string_view pattern)
    : pattern_(pattern)
{
    if (!pause()) 
    {
        throw std::invalid_argument("invalid logger format pattern");
    }
}

void Formatter::format(const LogRecordView& record, FormattedRecordBuffer& output) const 
{
    for (const auto& item : items_)
    {
        item->format(record, output); // 这个函数调用的是FormatItem里面的format，也就是派生类的函数 
    }
}

// 我们这里不使用to_string是因为这个转换会在堆区开辟一个内存，但是如果内存不够，那么就会抛出异常，导致我们根本使用不了日志器
// 所以我们这里自己写一个转化函数，把整数变成字符串
template <typename T>
void AppendInteger(FormattedRecordBuffer& output, T value)
{
    std::array<char, 24> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.size() + buffer.data(), value);
    ASSERT_RETNONE2(result.ec == std::errc(), "trans " << value << "to chars failed");
    // result.ptr 最后一个成功写入字符的下一个位置
    output.append(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
}

class LiteralFormatItem final : public Formatter::FormatItem
{
public:
    explicit LiteralFormatItem(std::string value)
        : value_(std::move(value))
    {}

    void format (const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append(value_);
    }
private:
    std::string value_;
};

class LevelFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append(LogLevel::ToString(record.level_));
    }
};

class ElapsedFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        AppendInteger(output, std::chrono::duration_cast<std::chrono::milliseconds>(record.elapsed_).count());
    }
};

class LoggerNameFormatItem final : public Formatter::FormatItem
{
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append(record.loggerName_);
    }
};

class ThreadIdFormatItem final : public Formatter::FormatItem
{
public:
    void format (const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        AppendInteger(output, record.threadId_);
    }
};

class NewLineFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append('\n');
    }
};

class DateTimeFormatItem final : public Formatter::FormatItem
{
public:
    explicit DateTimeFormatItem(std::string_view format)
        : format_(format)
    {}

    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        // 同一个进程中的datetime format要一致，否则会串
        static thread_local time_t last_second = 0;
        static thread_local char cached_date_time[20] = {'\0'};

        const auto duration = record.timestamp_.time_since_epoch();
        const time_t current_second = static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());

        // 这里做优化，只要是在同一个min内，就直接用之前保存的时间，不去系统里读取时间
        if (current_second != last_second) 
        {
            std::tm buffer{}; // 保存得到目前时间的结构体
            const std::tm* result = localtime_r(&current_second, &buffer);
            ASSERT_RETNONE2(result == nullptr, "failed to convert log time to local time");

            /** 
             * @brief localtime_r是线程安全的，但是消耗也大：
             * 1. 全局有锁可能排队
             * 2. 需要进行复杂的时区计算(尤其是有过夏令时变更历史的时区)
             * 3. 如果系统没有加载时区信息或者要求响应时区变更, 每次调用这个接口还要去发起磁盘IO，读系统文件/etc/localtime
            */
            // 把格式化的时间转化成字符串
            const std::size_t size = std::strftime(cached_date_time, sizeof(cached_date_time), format_.c_str(), &buffer);
            ASSERT_RETNONE2(size != 0, "failed to format log time");
            last_second = current_second;
        }

        output.append(cached_date_time);
    }

private:
    std::string format_;
};

class MicrosecondsFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        const auto total_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(record.timestamp_.time_since_epoch()).count();
        /*
        总微秒数 = 秒数 * 1000000 + 剩余微秒
        */
        s64 microseconds = total_microseconds % 1000000; // 剩余的微秒数
        if (microseconds < 0)
        {
            microseconds += 1000000;
        }

        // 秒内微秒必须补齐成固定六位，否则 12:00:00.1234 这样的时间戳无法解析，也无法按字典序排序
        std::array<char, MICROSECONDS_WIDTH> digits{};
        auto remaining = static_cast<u32>(microseconds);
        for (std::size_t index = digits.size() - 1; index >= 0; ) 
        {
            digits[index] = static_cast<char>('0' + remaining % 10);
            remaining /= 10;
        }
        output.append(digits.data(), digits.size());
    }

private:
    static constexpr std::size_t MICROSECONDS_WIDTH = 6;
};

class FileNameFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append(record.fileName_);
    }
};

class LineFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        AppendInteger(output, record.line_);
    }
};

class TabFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append('\t');
    }
};

class FiberIdFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        AppendInteger(output, record.fiberId_);
    }
};

class ThreadNameFormatItem final : public Formatter::FormatItem
{
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override 
    {
        output.append(record.threadName_);
    }
};

class MessageFormatItem final : public Formatter::FormatItem
{
public:
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
    {
        output.append(record.message_);
    }
};

using FormatItemFactory = std::unique_ptr<Formatter::FormatItem> (*)(std::string_view);

template <typename Item>
[[nodiscard]] std::unique_ptr<Formatter::FormatItem> CreateSimpleFormatItem(std::string_view)
{
    return std::make_unique<Item>();
}

template <typename Item>
[[nodiscard]] std::unique_ptr<Formatter::FormatItem> CreateConfiguredFormatItem(std::string_view format)
{
    return std::make_unique<Item>(format);
}

[[nodiscard]] const std::unordered_map<char, FormatItemFactory>& GetFormatItemFactories()
{
    static const std::unordered_map<char, FormatItemFactory> FORMAT_ITEM_FACTORIES{
        {'m', &CreateSimpleFormatItem<MessageFormatItem>},          // 日志内容
        {'p', &CreateSimpleFormatItem<LevelFormatItem>},            // LogLevel
        {'r', &CreateSimpleFormatItem<ElapsedFormatItem>},          // 起服已过时间
        {'c', &CreateSimpleFormatItem<LoggerNameFormatItem>},       // 日志器名称
        {'t', &CreateSimpleFormatItem<ThreadIdFormatItem>},         // 线程id
        {'n', &CreateSimpleFormatItem<NewLineFormatItem>},          // 换行
        {'d', &CreateConfiguredFormatItem<DateTimeFormatItem>},     // datetime
        {'u', &CreateSimpleFormatItem<MicrosecondsFormatItem>},     // 毫秒数
        {'f', &CreateSimpleFormatItem<FileNameFormatItem>},         // 文件名
        {'l', &CreateSimpleFormatItem<LineFormatItem>},             // 行号
        {'T', &CreateSimpleFormatItem<TabFormatItem>},              // 制表符
        {'F', &CreateSimpleFormatItem<FiberIdFormatItem>},          // 协程ID
        {'N', &CreateSimpleFormatItem<ThreadNameFormatItem>},       // 大N线程名
    };
    return FORMAT_ITEM_FACTORIES;
}

 
[[nodiscard]] std::unique_ptr<Formatter::FormatItem> CreateFormatItem(char directive, std::string_view format)
{
    const auto& factories = GetFormatItemFactories();
    const auto it = factories.find(directive);
    ASSERT_RETVAL2(it != factories.end(), nullptr, "unknown logger format directive: " << directive);
    return it->second(format);
}

void Formatter::addLiteral(std::string& literal)
{
    if (literal.empty())
    {
        return;
    }
    items_.push_back(std::make_unique<LiteralFormatItem>(std::move(literal)));
    literal.clear();
}

// %d{%Y-%m-%d %H:%M:%S}.%u%Tthread=%t%Tfiber=%F%T[%p]%T%f:%l%T%m%n
int Formatter::parse()
{
    std::string literal;
    for (std::size_t index = 0; index < pattern_.size(); index++)
    {
        if (pattern_[index] != '%')
        {
            literal.push_back(pattern_[index]);
            continue;
        }
        ASSERT_RETVAL2(index + 1 < pattern_.size(), -1, "logger format pattern ends with an incomplete directive");
        const char directive = pattern_[++index];
        if (directive == '%')
        {
            literal.push_back('%');
            continue;
        }
        addLiteral(literal);
        std::string_view item_format;
        if (index + 1 < pattern_.size() && pattern_[index + 1] == '{')
        {
            const std::size_t closing_brace = pattern_.find('}', index + 2);
            ASSERT_RETVAL2(closing_brace != std::string::npos && closing_brace != (index + 2), -2, "missing a close brace or empty");
            item_format = std::string_view(pattern_).substr(index + 2, closing_brace - index - 2);
            index = closing_brace;
        }
        items_.push_back(CreateFormatItem(directive, item_format));
    }
    addLiteral(literal);

    for(const auto& item : items_)
    {
        ASSERT_RETVAL2(item != nullptr, -3, "logger formatter contains a null format item");
    }

    return 0;
}

}