#ifndef _PANS_SRC_BUFFER_H_
#define _PANS_SRC_BUFFER_H_

#include <cstring>
#include <vector>
#include <array>
#include <string_view>
#include <algorithm>

namespace pans::detail {

// 自定义一个日志缓冲区，针对高频，小数据量的日志场景，在栈上分配日志缓冲区，与超长日志，再在堆上分配内存
template <std::size_t INLINE_CAPACITY>
class InlineBuffer
{
public:
    void append(const char* data, std::size_t size)
    {
        if (size == 0)
        {
            return;
        }
        if (overflow_.empty() && size + size_ <= INLINE_CAPACITY)
        {
            std::memcpy(inline_.data() + size_, data, size);
            size_ += size;
            return;
        }

        if (overflow_.empty())
        {
            const std::size_t required_capacity = size_ + size;
            overflow_.reserve(std::max(required_capacity, 2 * INLINE_CAPACITY));
            overflow_.insert(overflow_.end(), inline_.data(), inline_.data() + size_);
        }
        overflow_.insert(overflow_.end(), data, data + size);
        size_ += size;
    }

    void append(std::string_view value)
    {
        append(value.data(), value.size());
    }

    void append(char value)
    {
        append(&value, 1);
    }

    [[nodiscard]] const char* data() const noexcept
    {
        return overflow_.empty() ? inline_.data() : overflow_.data();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return size_;
    }

    [[nodiscard]] std::string_view view() const noexcept
    {
        return {data(), size()};
    }

private:
    std::array<char, INLINE_CAPACITY> inline_{};
    std::vector<char> overflow_;
    std::size_t size_ = 0;
};

// 继承std::streambuf是为了能够能够继续使用流式输入接口，兼容ostream生态
template <std::size_t INLINE_CAPACITY>
class SmallStreamBuffer : public std::streambuf
{
public:
    explicit SmallStreamBuffer(InlineBuffer<INLINE_CAPACITY>& buffer) noexcept
        : buffer_(buffer)
    {}

protected:
    std::streamsize xsputn(const char* data, std::streamsize size) override
    {
        if (size <= 0)
        {
            return 0;
        }

        buffer_.append(data, static_cast<std::size_t>(size));
        return size;
    }

    int_type overflow(int_type character) override
    {
        // overflow，除了可以接受一般字符，还可能会接收流终止符EOF
        // 如果我们遇到了EOF, 把它转成非EOF值向上报告写入成功，实际上不向Buffer里写入任何值。
        if (traits_type::eq_int_type(character, traits_type::eof()))
        {
            return traits_type::not_eof(character);
        }
        buffer_.append(traits_type::to_char_type(character));
        return character;
    }

private:
    InlineBuffer<INLINE_CAPACITY>& buffer_;
};

}

#endif
