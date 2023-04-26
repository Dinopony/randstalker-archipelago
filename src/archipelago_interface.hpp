#pragma once

#include "preset_builder.hpp"
#include "randstalker_invoker.hpp"
#include <set>
#include <apclient.hpp>

using nlohmann::json;

class GameState;

class ArchipelagoInterface {
private:
    APClient* _client = nullptr;
    GameState* _game_state;
    bool _connected = false;
    bool _connection_failed = false;
    std::string _slot_name;
    std::string _password;

public:
    explicit ArchipelagoInterface(const std::string& uri, std::string slot_name, std::string password, GameState* game_state);
    virtual ~ArchipelagoInterface();

    void poll();
    void send_checked_locations_to_server(const std::set<uint16_t>& checked_locations);

    void say(const std::string& msg);
    bool is_connected() const;
    bool connection_failed() const { return _connection_failed; }
    void notify_game_completed();
    void notify_death();

private:
    void init_handlers();

    void on_socket_connected();
    void on_socket_disconnected();
    void on_room_info();
    void on_slot_connected(const json& slot_data);
    void on_slot_disconnected();
    void on_slot_refused(const std::list<std::string>& errors);
    void on_items_received(const std::list<APClient::NetworkItem>& items);
    void on_bounced(const json& cmd);
};