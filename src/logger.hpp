#pragma once

#include <string>
#include <vector>
#include <mutex>

class Logger {
public:
    enum LogLevel {
        LOG_DEBUG = 0,
        LOG_MESSAGE = 1,
        LOG_INFO = 2,
        LOG_HINT = 3,
        LOG_WARNING = 4,
        LOG_ERROR = 5
    };

    struct Message {
        LogLevel level;
        std::string text;
        uint64_t timestamp;
    };

private:
    std::vector<Message> _messages;
    std::mutex _mutex;

public:
    void log(const std::string& msg, LogLevel level);

    [[nodiscard]] std::vector<Message> messages();

    static Logger& get();
    static void debug(const std::string& msg)    { Logger::get().log(msg, LOG_DEBUG); }
    static void message(const std::string& msg)  { Logger::get().log(msg, LOG_MESSAGE); }
    static void info(const std::string& msg)     { Logger::get().log(msg, LOG_INFO); }
    static void warning(const std::string& msg)  { Logger::get().log(msg, LOG_WARNING); }
    static void error(const std::string& msg)    { Logger::get().log(msg, LOG_ERROR); }

private:
    Logger() = default;
};
