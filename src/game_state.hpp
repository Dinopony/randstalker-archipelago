#pragma once

#include <utility>
#include <vector>
#include <set>
#include <iostream>
#include "location.hpp"

class GameState {
private:
    nlohmann::json _preset_json = {};
    std::string _built_rom_path;

    std::vector<Location> _locations;

    std::vector<uint8_t> _received_items;
    bool _must_send_checked_locations = false;
    uint32_t _expected_seed = 0xFFFFFFFF;
    bool _has_won = false;

    std::array<uint8_t, 0x20> _inventory_bytes;

    bool _has_deathlink = false;
    bool _received_death = false;
    bool _must_send_death = false;

    /// 0 = beat_gola, 1 = reach_kazalt, 2 = beat_dark_nole
    uint8_t _goal_id = -1;
    std::string _goal_string;

public:
    GameState();
    void reset();

    [[nodiscard]] uint16_t current_item_index() const { return static_cast<uint16_t>(_received_items.size()); }
    [[nodiscard]] uint8_t item_with_index(uint16_t received_item_index) const;
    void set_received_item(uint16_t index, uint8_t item);

    [[nodiscard]] const std::vector<Location>& locations() const { return _locations; }
    [[nodiscard]] std::vector<Location>& locations() { return _locations; }
    [[nodiscard]] const Location* location(const std::string& name) const;

    [[nodiscard]] bool must_send_checked_locations() const { return _must_send_checked_locations; }
    void must_send_checked_locations(bool val) { _must_send_checked_locations = val; }
    [[nodiscard]] std::vector<int64_t> checked_locations() const;

    [[nodiscard]] uint32_t expected_seed() const { return _expected_seed; }
    void expected_seed(uint32_t seed) { _expected_seed = seed; }

    [[nodiscard]] bool has_won() const { return _has_won; }
    void has_won(bool val) { _has_won = val; }

    [[nodiscard]] bool has_deathlink() const { return _has_deathlink; }
    void has_deathlink(bool val) { _has_deathlink = val; }

    [[nodiscard]] bool received_death() const { return _received_death; }
    void received_death(bool val) { _received_death = val; }

    [[nodiscard]] bool must_send_death() const { return _must_send_death; }
    void must_send_death(bool val) { _must_send_death = val; }

    [[nodiscard]] const nlohmann::json& preset_json() const { return _preset_json; }
    void preset_json(nlohmann::json json);

    [[nodiscard]] uint8_t goal_id() const { return _goal_id; }
    [[nodiscard]] const std::string& goal_string() const { return _goal_string; }

    [[nodiscard]] bool has_built_rom() const { return !_built_rom_path.empty(); }
    [[nodiscard]] const std::string& built_rom_path() const { return _built_rom_path; }
    void built_rom_path(const std::string& val) { _built_rom_path = val; }

    void update_inventory_byte(uint8_t byte_id, uint8_t value) { _inventory_bytes[byte_id] = value; }
    [[nodiscard]] uint8_t owned_item_quantity(uint8_t item_id) const;
};
