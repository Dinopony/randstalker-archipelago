#pragma once

#include <vector>
#include <string>

class EmulatorException : public std::exception {
private:
    std::string _msg;
public:
    explicit EmulatorException(std::string msg) : std::exception(), _msg(std::move(msg)) {}
    [[nodiscard]] const std::string& message() const { return _msg; }
};

class EmulatorInterface
{
public:
    EmulatorInterface() = default;
    virtual ~EmulatorInterface() = default;

    [[nodiscard]] virtual uint8_t read_game_byte(uint16_t address) const = 0;
    [[nodiscard]] virtual uint16_t read_game_word(uint16_t address) const = 0;
    [[nodiscard]] virtual uint32_t read_game_long(uint16_t address) const = 0;

    virtual void write_game_byte(uint16_t address, uint8_t value) = 0;
    virtual void write_game_word(uint16_t address, uint16_t value) = 0;
    virtual void write_game_long(uint16_t address, uint32_t value) = 0;
};
