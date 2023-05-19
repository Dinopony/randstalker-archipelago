#pragma once

#include <vector>
#include <string>
#include "multiworld_interface.hpp"

class OfflinePlayInterface : public MultiworldInterface
{
public:
    OfflinePlayInterface() = default;
    ~OfflinePlayInterface() override = default;

    void poll() override {}
    void send_checked_locations_to_server(const std::vector<int64_t>& checked_locations) override {}
    void say(const std::string& msg) override {}

    [[nodiscard]] bool is_connected() const override { return true; }
    [[nodiscard]] bool is_offline_session() const override { return true; }
    [[nodiscard]] bool connection_failed() const override { return false; }
    [[nodiscard]] std::string player_name() const override { return "Player"; }

    void notify_game_completed() override {}
    void notify_death() override {}
};
