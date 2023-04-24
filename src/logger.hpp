#pragma once

#include <string>
#include <vector>

class Logger {
public:
    enum LogLevel {
        LOG_DEBUG = 0,
        LOG_INFO = 1,
        LOG_WARNING = 2,
        LOG_ERROR = 3
    };

    struct Message {
        LogLevel level;
        std::string text;
        uint64_t timestamp;
    };

private:
    std::vector<Message> _messages;

public:
    void log(const std::string& msg, LogLevel level);

    [[nodiscard]] const std::vector<Message>& messages() const { return _messages; }

    static Logger& get();
    static void debug(const std::string& msg)    { Logger::get().log(msg, LOG_DEBUG); }
    static void info(const std::string& msg)     { Logger::get().log(msg, LOG_INFO); }
    static void warning(const std::string& msg)  { Logger::get().log(msg, LOG_WARNING); }
    static void error(const std::string& msg)    { Logger::get().log(msg, LOG_ERROR); }

private:
    Logger() = default;
};
