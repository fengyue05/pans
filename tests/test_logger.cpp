#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

#include <pans/Logger/Log.h>

int main()
{
    const std::filesystem::path output_path = std::filesystem::temp_directory_path() / "test_logger.log";
    std::ofstream(output_path, std::ios::binary | std::ios::trunc);

    auto logger = std::make_shared<pans::Logger>("test");
    logger->addAppender(pans::MakeStdoutAppender());
    logger->addAppender(pans::MakeFileAppender(output_path.string()));

    PANS_LOG_DEBUG(logger) << "debug message";
    PANS_LOG_INFO(logger) << "info message";
    PANS_LOG_WARN(logger) << "warning message";
    PANS_LOG_ERROR(logger) << "error message";
    PANS_LOG_FATAL(logger) << "fatal message";
    std::cout << "----------------------------------------------------" << std::endl;
    PANS_LOG_FMT_DEBUG(logger, "debug message");
    PANS_LOG_FMT_INFO(logger, "info message");
    PANS_LOG_FMT_WARN(logger, "warn message");
    PANS_LOG_FMT_ERROR(logger, "error message");
    PANS_LOG_FMT_FATAL(logger, "fatal message");

    char c = 'a';
    int i = 42;
    const char msg1[] = "hello";
    const std::string msg2 = "world";
    const std::string_view msg3 = "pans 磐石";

    auto g_logger = logger;
    std::cout << "----------------------------------------------------" << std::endl;
    LOG_INFO << "c=" << c << " i=" << i << " msg1: " << msg1 << " msg2: " << msg2 << " msg3: " << msg3;
    PANS_LOG_FMT_INFO(logger, "c=%c i=%d msg1: %s msg2: %s msg3: %s", c, i, msg1, msg2.c_str(), msg3.data());

    logger->sync();
    std::cout << "\nThe same records were written to: " << output_path << '\n';
    return 0;
}
