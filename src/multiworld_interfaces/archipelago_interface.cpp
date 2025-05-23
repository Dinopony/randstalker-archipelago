#include "archipelago_interface.hpp"

#include <utility>
#include "apclient.hpp"
#include "apuuid.hpp"
#include <fstream>

#include "../client.hpp"
#include "../game_state.hpp"
#include "../logger.hpp"

#define GAME_NAME "Landstalker - The Treasures of King Nole"
#define DATAPACKAGE_CACHE_FILE "datapackage.json"
#define UUID_FILE "uuid"

constexpr uint16_t ITEM_BASE_ID = 4000;

ArchipelagoInterface::ArchipelagoInterface(const std::string& uri, std::string slot_name, std::string password) :
    _slot_name  (std::move(slot_name)),
    _password   (std::move(password))
{
    std::string uuid = ap_get_uuid(UUID_FILE);
    Logger::debug("UUID is " + uuid);

    _client = new APClient(uuid, GAME_NAME, uri);

    this->init_handlers();
}

ArchipelagoInterface::~ArchipelagoInterface()
{
    delete _client;
}

void ArchipelagoInterface::init_handlers()
{
    _client->set_socket_connected_handler([this](){ this->on_socket_connected(); });
    _client->set_socket_disconnected_handler([this](){ this->on_socket_disconnected(); });
    _client->set_room_info_handler([this](){ this->on_room_info(); });

    _client->set_slot_connected_handler([this](const json& j){ this->on_slot_connected(j); });
    _client->set_slot_disconnected_handler([this](){ this->on_slot_disconnected(); });
    _client->set_slot_refused_handler([this](const std::list<std::string>& errors){ this->on_slot_refused(errors); });

    _client->set_items_received_handler([this](const std::list<APClient::NetworkItem>& items) {
        for (const auto& i : items)
            this->on_item_received(i.index, i.item, i.player, i.location);
    });

    _client->set_location_info_handler([this](const std::list<APClient::NetworkItem>& items) {
        _locations_data = json::object();
        for (const auto& i : items)
            this->on_item_scouted(i.index, i.item, i.player, i.location);
    });

    _client->set_bounced_handler([this](const json& cmd) { this->on_bounced(cmd); });

    _client->set_print_handler([](const std::string& msg) { Logger::message(msg); });
    _client->set_print_json_handler([this](const std::list<APClient::TextNode>& msg) {
        Logger::message(_client->render_json(msg, APClient::RenderFormat::TEXT));
    });
}

void ArchipelagoInterface::say(const std::string& msg)
{
    if(msg.empty())
        return;
    if(!_client || _client->get_state() < APClient::State::SOCKET_CONNECTED)
        return;

    _client->Say(msg);
}

bool ArchipelagoInterface::is_connected() const
{
    return _client && _client->get_state() == APClient::State::SLOT_CONNECTED;
}

void ArchipelagoInterface::poll()
{
    if(!_client)
        return;

    _client->poll();
}

void ArchipelagoInterface::send_checked_locations_to_server(const std::vector<int64_t>& checked_locations)
{
    if(!_client || _client->get_state() != APClient::State::SLOT_CONNECTED)
    {
        Logger::warning("Attempting to send checked locations to server, but there is no connection.");
        return;
    }

    _client->LocationChecks(std::list<int64_t>(checked_locations.begin(), checked_locations.end()));
}

void ArchipelagoInterface::notify_game_completed()
{
    if(!_client || _client->get_state() != APClient::State::SLOT_CONNECTED)
    {
        Logger::warning("Attempting to send goal completed, but there is no connection.");
        return;
    }

    _client->StatusUpdate(APClient::ClientStatus::GOAL);
}

void ArchipelagoInterface::notify_death()
{
    if(!_client || _client->get_state() != APClient::State::SLOT_CONNECTED)
    {
        Logger::warning("Attempting to send deathlink, but there is no connection.");
        return;
    }

    double death_time = _client->get_server_time();
    json data{
            {"time", death_time},
            {"cause", "Wanted to consume an EkeEke."},
            {"source", _slot_name},
    };
    _client->Bounce(data, {}, {}, {"DeathLink"});
    Logger::debug("Sending death...");
}

void ArchipelagoInterface::on_socket_connected()
{
    _connected = true;
    Logger::info("Established connection to Archipelago server.");
}

void ArchipelagoInterface::on_socket_disconnected()
{
    _connection_failed = true;
    Logger::error("Disconnected from Archipelago server.");
}

void ArchipelagoInterface::on_room_info()
{
    if(!_client)
        return;

    _client->ConnectSlot(_slot_name, _password, 5, {}, {0, 6, 0});
}

void ArchipelagoInterface::on_slot_connected(const json& slot_data)
{
    if(!_client)
        return;

    Logger::info("Connected to slot.");
    _slot_data = slot_data;

    game_state.has_deathlink(slot_data["death_link"] == 1);
    if (game_state.has_deathlink())
    {
        Logger::debug("Updating connection with DeathLink tag");
        _client->ConnectUpdate(false, 0, true, { "DeathLink" });
    }

    // Update the expected seed to know which filename to look for during ROM existence check
    game_state.expected_seed(_slot_data["seed"]);
    check_rom_existence(game_state.expected_seed(), _slot_name);

    bool goal_reach_kazalt = (_slot_data["goal"] == 1);
    const std::vector<int64_t> ENDGAME_IDS = {
        4019, 4020, 4021, 4108, 4109, 4110, 4111, 4112, 4113, 4114, 4115, 4116, 4117, 4118, 4119, 4120, 4121,
        4122, 4123, 4124, 4125, 4126, 4127, 4128, 4129, 4130, 4131, 4132, 4133, 4261, 4263, 4265, 4267, 4271,
        4283, 4284, 4285, 4331, 4332, 4333, 4334, 4335
    };

    // Fetch all of the locations' data to know what to put in item sources when building the ROM
    std::list<int64_t> all_location_ids;
    for(const Location& loc : game_state.locations())
    {
        bool is_endgame_location = std::find(ENDGAME_IDS.begin(), ENDGAME_IDS.end(), loc.id()) != ENDGAME_IDS.end();
        if(!goal_reach_kazalt || !is_endgame_location)
            all_location_ids.emplace_back(loc.id());
    }

    _client->LocationScouts(all_location_ids);
}

void ArchipelagoInterface::on_slot_disconnected()
{
    std::cout << "Disconnected from slot." << std::endl;
}

void ArchipelagoInterface::on_slot_refused(const std::list<std::string>& errors)
{
    _connection_failed = true;
    if (std::find(errors.begin(), errors.end(), "InvalidSlot") != errors.end())
    {
        Logger::error("Game slot '" + _slot_name + "' is invalid. Did you connect to the wrong server?");
    }
    else
    {
        Logger::error("Connection refused:");
        for (const auto& error: errors)
            Logger::error(error);
    }
}

void ArchipelagoInterface::on_item_received(int index, int64_t item, int player, int64_t location)
{
    if(!_client)
        return;

    std::string item_name = _client->get_item_name(item, GAME_NAME);
    std::string player_name = _client->get_player_alias(player);
    std::string location_game = _client->get_player_game(player);
    std::string location_name = _client->get_location_name(location, location_game);

    Logger::debug("Received " + item_name + " from " + player_name + " (" + location_name + ")");
    game_state.set_received_item(index, item - ITEM_BASE_ID);
}

void ArchipelagoInterface::on_item_scouted(int index, int64_t item, int player, int64_t location)
{
    if(!_client)
        return;

    std::string location_name = _client->get_location_name(location, GAME_NAME);

    std::string player_name = _client->get_player_alias(player);
    std::string item_game = _client->get_player_game(player);
    std::string item_name = _client->get_item_name(item, item_game);

    _locations_data[location_name] = {
            { "item", item_name },
            { "player", player_name }
    };
}

void ArchipelagoInterface::on_bounced(const json& packet)
{
    if(!game_state.has_deathlink())
        return;

    auto tagsIt = packet.find("tags");
    auto dataIt = packet.find("data");
    if (tagsIt != packet.end() && tagsIt->is_array() && std::find(tagsIt->begin(), tagsIt->end(), "DeathLink") != tagsIt->end())
    {
        if (dataIt != packet.end() && dataIt->is_object())
        {
            json data = *dataIt;
            std::string player_name = data["source"].is_string() ? data["source"].get<std::string>().c_str() : "???";
            if(player_name == _slot_name && player_name != "???")
                return;

            if(data["cause"].is_string())
            {
                std::string cause = data["cause"];
                Logger::message("Died by the hands of " + player_name + ": " + cause);
            }
            else
            {
                Logger::message("Died by the hands of " + player_name + ".");
            }

            game_state.received_death(true);
        }
        else
        {
            Logger::error("Received bad deathlink packet.");
        }
    }
}