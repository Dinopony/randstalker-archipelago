#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

class RetroarchInterface;

class Location {
private:
    uint16_t _id = 0xFFFF;
    std::string _name;
    uint16_t _checked_flag_byte = 0x0000;
    uint8_t _checked_flag_bit = 0x00;

public:
    Location() = default;
    explicit Location(nlohmann::json& location_data, uint8_t& cur_reward_id);

    [[nodiscard]] uint16_t id() const { return _id; }
    [[nodiscard]] const std::string& name() const { return _name; }
    [[nodiscard]] bool was_checked(const RetroarchInterface& emulator) const;

    [[nodiscard]] nlohmann::json to_json() const;
};