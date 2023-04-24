#include "archipelago_interface.hpp"

#include <utility>

#include "game_state.hpp"

constexpr uint16_t ITEM_BASE_ID = 4000;

ArchipelagoInterface::ArchipelagoInterface(const std::string& uri, std::string slot_name, std::string password, GameState* game_state) :
    _game_state (game_state),
    _slot_name  (std::move(slot_name)),
    _password   (std::move(password))
{
    std::string uuid = ap_get_uuid(UUID_FILE);
    std::cout << "UUID is " << uuid << std::endl;

    _client = new APClient(uuid, GAME_NAME, uri);
//        try {
//            _client->set_data_package_from_file(DATAPACKAGE_CACHE_FILE);
//        } catch (std::exception&) { /* ignore */ }

    this->init_handlers();
}

ArchipelagoInterface::~ArchipelagoInterface()
{
    std::cout << "Disconnecting..." << std::endl;
    delete _client;
}

void ArchipelagoInterface::poll()
{
    if(!_client)
        return;

    _client->poll();

    if(_game_state->server_must_know_checked_locations() && _client->get_state() == APClient::State::SLOT_CONNECTED)
    {
        this->send_checked_locations_to_server(_game_state->checked_locations());
        _game_state->clear_server_must_know_checked_locations();
    }

    if(_game_state->has_won())
        this->notify_game_completed();
}

void ArchipelagoInterface::send_checked_locations_to_server(const std::set<uint16_t>& checked_locations)
{
    if(!_client || _client->get_state() != APClient::State::SLOT_CONNECTED)
    {
        std::cerr << "[ERROR] Attempting to send checked locations to server, but there is no connection." << std::endl;
        return;
    }

    std::cout << "[SEND] New checked locations = [";
    std::list<int64_t> checked_locations_typed;
    for(uint16_t loc_id : checked_locations)
    {
        std::cout << loc_id << " ";
        checked_locations_typed.emplace_back(static_cast<int64_t>(loc_id));
    }
    std::cout << "]" << std::endl;

    _client->LocationChecks(checked_locations_typed);
}

void ArchipelagoInterface::notify_game_completed()
{
    if(!_client || _client->get_state() != APClient::State::SLOT_CONNECTED)
    {
        std::cerr << "[ERROR] Attempting to send goal completed, but there is no connection." << std::endl;
        return;
    }

    _client->StatusUpdate(APClient::ClientStatus::GOAL);
}



void ArchipelagoInterface::on_socket_connected()
{
    // if the socket (re)connects we actually don't know the server's state. clear game's cache to not desync
    // TODO: in future set game's location cache from AP's checked_locations instead
    //        if (game) game->clear_cache();

    _connected = true;
    std::cout << "Connected!" << std::endl;
}

void ArchipelagoInterface::on_socket_disconnected()
{
    _connected = false;
    std::cout << "Disconnected from server." << std::endl;
}

void ArchipelagoInterface::on_room_info()
{
//        // compare seeds and error out if it's the wrong one, and then (try to) connect with games's slot
//        if (!game || game->get_seed().empty() || game->get_slot().empty())
//            printf("Waiting for game ...\n");
//        else if (strncmp(game->get_seed().c_str(), ap->get_seed().c_str(), GAME::MAX_SEED_LENGTH) != 0)
//            bad_seed(ap->get_seed(), game->get_seed());
//        else {
//            std::list<std::string> tags;
//            if (game->want_deathlink()) tags.push_back("DeathLink");
//            ap->ConnectSlot(game->get_slot(), password, game->get_items_handling(), tags, VERSION_TUPLE);
//            ap_connect_sent = true; // TODO: move to APClient::State ?
//        }
    _client->ConnectSlot(_slot_name, _password, 1, {}, {0, 3, 8});
}

void ArchipelagoInterface::on_slot_connected(const json& slot_data)
{
    std::cout << "Connected to slot : " << slot_data.dump(4) << std::endl;

    json preset = build_preset_json(slot_data, _slot_name);
    std::ofstream outfile("./presets/_ap_preset.json");
    outfile << preset.dump(4);
    outfile.close();

    _game_state->expected_seed(preset["seed"]);

    std::cout << "Invoking randstalker using preset..." << std::endl;

    char command[] = "randstalker.exe --outputrom=./seeds/ --preset=_ap_preset --nopause";
    invoke(command);
}

void ArchipelagoInterface::on_items_received(const std::list<APClient::NetworkItem>& items)
{
    // TODO: Handle datapackage?
    // if (!_client->is_data_package_valid())
    // {
    //     // NOTE: this should not happen since we ask for data package before connecting
    //     if (!ap_sync_queued) _client->Sync();
    //     ap_sync_queued = true;
    //     return;
    // }

    for (const auto& item: items)
    {
        std::string item_name = _client->get_item_name(item.item);
        std::string sender = _client->get_player_alias(item.player);
        std::string location = _client->get_location_name(item.location);

        std::cout << "[PACKET] Received " << item_name << " from " << sender << " (" << location << ") [#" << item.index << "]" << std::endl;
        _game_state->set_received_item(item.index, item.item - ITEM_BASE_ID);
    }
}