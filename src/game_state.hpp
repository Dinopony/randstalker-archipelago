#pragma once

#include <vector>
#include <set>
#include <iostream>
#include "location.hpp"

class GameState {
private:
    std::vector<Location> _locations;
    std::set<uint16_t> _checked_locations;

    std::vector<uint8_t> _received_items;
    bool _server_must_know_checked_locations = false;
    uint32_t _expected_seed = 0xFFFFFFFF;
    bool _has_won = false;

    bool _has_deathlink = false;
    bool _received_death = false;
    bool _must_send_death = false;
public:
    GameState();

    [[nodiscard]] uint16_t current_item_index() const { return static_cast<uint16_t>(_received_items.size()); }
    [[nodiscard]] uint8_t item_with_index(uint16_t received_item_index) const;
    void set_received_item(uint16_t index, uint8_t item);

    [[nodiscard]] const std::vector<Location>& locations() const { return _locations; }
    bool set_location_checked_by_player(uint16_t location_index);

    [[nodiscard]] bool server_must_know_checked_locations() const { return _server_must_know_checked_locations; }
    void clear_server_must_know_checked_locations() { _server_must_know_checked_locations = false; }
    [[nodiscard]] const std::set<uint16_t>& checked_locations() const { return _checked_locations; }

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
};