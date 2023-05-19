#pragma once

#include "multiworld_interface.hpp"
#include "../preset_builder.hpp"
#include <set>

using nlohmann::json;

class APClient;

class ArchipelagoInterface : public MultiworldInterface {
private:
    APClient* _client = nullptr;
    bool _connected = false;
    bool _connection_failed = false;
    std::string _slot_name;
    std::string _password;

public:
    explicit ArchipelagoInterface(const std::string& uri, std::string slot_name, std::string password);
    ~ArchipelagoInterface() override;

    void poll() override;
    void send_checked_locations_to_server(const std::vector<int64_t>& checked_locations) override;
    void say(const std::string& msg) override;
    void notify_game_completed() override;
    void notify_death() override;

    [[nodiscard]] bool is_connected() const override;
    [[nodiscard]] bool is_offline_session() const override { return false; }
    [[nodiscard]] bool connection_failed() const override { return _connection_failed; }
    [[nodiscard]] std::string player_name() const override { return _slot_name; }

private:
    void init_handlers();

    void on_socket_connected();
    void on_socket_disconnected();
    void on_room_info();
    void on_slot_connected(const json& slot_data);
    void on_slot_disconnected();
    void on_slot_refused(const std::list<std::string>& errors);
    void on_item_received(int index, int64_t item, int player, int64_t location);

    void on_bounced(const json& cmd);
};