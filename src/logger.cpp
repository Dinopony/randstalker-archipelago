#include "logger.hpp"
#include <chrono>
#include <iostream>

void Logger::log(const std::string& msg, LogLevel level)
{
#ifndef DEBUG
    if(level == LOG_DEBUG)
        return;
#endif
    _mutex.lock();
    Message new_message;
    new_message.level = level;
    new_message.text = msg;
    const auto now = std::chrono::system_clock::now();
    new_message.timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    if(level == LOG_MESSAGE && msg.starts_with("[Hint]:"))
        new_message.level = LOG_HINT;

    if(level == LOG_WARNING || level == LOG_ERROR)
        std::cerr << msg << std::endl;
    else
        std::cout << msg << std::endl;

    _messages.emplace_back(new_message);
    _mutex.unlock();
}

std::vector<Logger::Message> Logger::messages()
{
    _mutex.lock();
    std::vector<Message> messages_vector = _messages;
    _mutex.unlock();
    return messages_vector;
}

Logger& Logger::get()
{
    static Logger _singleton;
    return _singleton;
}


