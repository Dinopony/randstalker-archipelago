#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

class Location {
private:
    uint16_t _id = 0xFFFF;
    std::string _name;
    uint16_t _checked_flag_byte = 0x0000;
    uint8_t _checked_flag_bit = 0x00;
    bool _was_checked = false;

public:
    Location() = default;
    explicit Location(nlohmann::json& location_data);

    [[nodiscard]] uint16_t id() const { return _id; }
    [[nodiscard]] const std::string& name() const { return _name; }
    [[nodiscard]] uint16_t checked_flag_byte() const { return _checked_flag_byte; }
    [[nodiscard]] uint8_t checked_flag_bit() const { return _checked_flag_bit; }

    [[nodiscard]] bool was_checked() const { return _was_checked; }
    void was_checked(bool value) { _was_checked = value; }
};
