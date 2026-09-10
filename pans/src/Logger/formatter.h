#ifndef _PANS_SRC_LOGGER_FORMATTER_H_
#define _PANS_SRC_LOGGER_FORMATTER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "buffer.h"
#include "buffer_config.h"
#include "log_record.h"


namespace pans::detail {

using FormattedRecordBuffer = InlineBuffer<FORMATTED_RECORD_INLINE_CAPACITY>;

class Formatter final
{
public:
    class FormatItem 
    {
    public:
        virtual ~FormatItem() = default;
        virtual void format(const LogRecordView& record, FormattedRecordBuffer& output) const = 0;
    };

    explicit Formatter(std::string_view pattern);
    // 把一条日志 record，按照 Formatter 保存的格式规则，格式化后写进 output 
    void format(const LogRecordView& record, FormattedRecordBuffer& output) const;

    [[nodiscard]] const std::string& getPattern() const noexcept { return pattern_;}

private:
    int parse(); 
    void addLiteral(std::string& literal);

private:
    std::string pattern_;
    std::vector<std::unique_ptr<FormatItem>> items_;
};
 
}

#endif