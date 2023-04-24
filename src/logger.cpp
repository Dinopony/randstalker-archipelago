#include "logger.hpp"
#include <chrono>
#include <iostream>

void Logger::log(const std::string& msg, LogLevel level)
{
    Message new_message;
    new_message.level = level;
    new_message.text = msg;
    const auto now = std::chrono::system_clock::now();
    new_message.timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    if(level == LOG_WARNING || level == LOG_ERROR)
        std::cerr << msg << std::endl;
    else
        std::cout << msg << std::endl;

    _messages.emplace_back(new_message);
}

Logger& Logger::get()
{
    static Logger _singleton;
    return _singleton;
}


