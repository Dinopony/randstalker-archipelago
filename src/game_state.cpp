#include "game_state.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include "data/item_source.json.hxx"
#include "logger.hpp"

using nlohmann::json;

GameState::GameState()
{
    _received_items.reserve(256);

    json input_json = json::parse(ITEM_SOURCES_JSON);
    for(json& location_data : input_json)
        _locations.emplace_back(Location(location_data));
}

uint8_t GameState::item_with_index(uint16_t received_item_index) const
{
    if(received_item_index < _received_items.size())
        return _received_items[received_item_index];

    Logger::warning("Tried fetching item #" + std::to_string(received_item_index) + " whereas it was never received.");
    return 0xFF;
}

bool GameState::set_location_checked_by_player(uint16_t location_index)
{
    if(!_checked_locations.count(location_index))
    {
        _checked_locations.insert(location_index);
        _server_must_know_checked_locations = true;
        return true;
    }

    return false;
}

void GameState::set_received_item(uint16_t index, uint8_t item)
{
    if(_received_items.size() <= index)
        _received_items.resize(index+1, 0xFF);
    else
        Logger::warning("Setting a received item without resizing received items array.");

    _received_items[index] = item;
}