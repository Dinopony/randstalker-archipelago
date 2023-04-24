#pragma once

#include <apclient.hpp>
#include <apuuid.hpp>
#include <fstream>

#include "preset_builder.hpp"
#include "randstalker_invoker.hpp"

using nlohmann::json;

#define GAME_NAME "Landstalker"
#define DATAPACKAGE_CACHE_FILE "datapackage.json"
#define UUID_FILE "uuid"

class GameState;

class ArchipelagoInterface {
private:
    APClient* _client = nullptr;
    GameState* _game_state;
    bool _connected = false;
    bool ap_sync_queued = false;
    bool ap_connect_sent = false;
    std::string _slot_name;
    std::string _password;

public:
    explicit ArchipelagoInterface(const std::string& uri, std::string slot_name, std::string password, GameState* game_state);
    virtual ~ArchipelagoInterface();

    void poll();
    void send_checked_locations_to_server(const std::set<uint16_t>& checked_locations);

    void say(const std::string& msg)
    {
        if(_client->get_state() < APClient::State::SOCKET_CONNECTED)
        {
            std::cerr << "Not connected to server, can't send chat message." << std::endl;
            return;
        }
    }
    bool is_connected() const { return _client && _client->get_state() == APClient::State::SLOT_CONNECTED; }
    void notify_game_completed();

private:
    void init_handlers()
    {
        _client->set_socket_connected_handler([this](){ this->on_socket_connected(); });
        _client->set_socket_disconnected_handler([this](){ this->on_socket_disconnected(); });
        _client->set_room_info_handler([this](){ this->on_room_info(); });

        _client->set_slot_connected_handler([this](const json& j){ this->on_slot_connected(j); });
        _client->set_slot_disconnected_handler([this](){ this->on_slot_disconnected(); });
        _client->set_slot_refused_handler([this](const std::list<std::string>& errors){ this->on_slot_refused(errors); });
        _client->set_items_received_handler([this](const std::list<APClient::NetworkItem>& items) { this->on_items_received(items); });
        _client->set_data_package_changed_handler([this](const json& data) { _client->save_data_package(DATAPACKAGE_CACHE_FILE); });
        _client->set_bounced_handler([this](const json& cmd) { this->on_bounced(cmd); });

        _client->set_print_handler([](const std::string& msg) {
            std::cout << msg << std::endl;
        });
        _client->set_print_json_handler([this](const std::list<APClient::TextNode>& msg) {
            std::cout << _client->render_json(msg, APClient::RenderFormat::ANSI) << std::endl;
        });
    }

    void on_socket_connected();
    void on_socket_disconnected();
    void on_room_info();
    void on_slot_connected(const json& slot_data);

    void on_slot_disconnected()
    {
        std::cout << "Disconnected from slot." << std::endl;
        ap_connect_sent = false;
    }

    void on_slot_refused(const std::list<std::string>& errors)
    {
        ap_connect_sent = false;
        if (std::find(errors.begin(), errors.end(), "InvalidSlot") != errors.end())
        {
            std::string slot;
//            if(game)
//                slot = game->get_slot(); // TODO: Why does slot belong to Game?
            std::cerr << "Game slot '" << slot << "' is invalid. Did you connect to the wrong server?" << std::endl;
        }
        else
        {
            std::cerr << "Connection refused:";
            for (const auto& error: errors)
                std::cerr << error;
            std::cerr << std::endl;
        }
    }

    void on_items_received(const std::list<APClient::NetworkItem>& items);

    void on_bounced(const json& cmd)
    {
        // TODO: Deathlink handling
        /*
        bool isEqual(double a, double b)
        {
            return fabs(a - b) < std::numeric_limits<double>::epsilon() * fmax(fabs(a), fabs(b));
        }

        if (game->want_deathlink())
        {
            auto tagsIt = cmd.find("tags");
            auto dataIt = cmd.find("data");
            if (tagsIt != cmd.end() && tagsIt->is_array() && std::find(tagsIt->begin(), tagsIt->end(), "DeathLink") != tagsIt->end())
            {
                if (dataIt != cmd.end() && dataIt->is_object()) {
                    json data = *dataIt;
                    printf("Received deathlink...\n");
                    if (data["time"].is_number() && isEqual(data["time"].get<double>(), deathtime)) {
                        deathtime = -1;
                    } else if (game) {
                        game->send_death();
                        printf("Died by the hands of %s: %s\n",
                               data["source"].is_string() ? data["source"].get<std::string>().c_str() : "???",
                               data["cause"].is_string() ? data["cause"].get<std::string>().c_str() : "???");
                    }
                } else {
                    printf("Bad deathlink packet!\n");
                }
            }
        }
        */
    }
};