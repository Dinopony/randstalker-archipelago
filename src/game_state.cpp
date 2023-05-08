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

void GameState::reset()
{
    _preset_json = {};
    _received_items.clear();
    _must_send_checked_locations = false;
    _expected_seed = 0xFFFFFFFF;
    _has_won = false;
    _has_deathlink = false;
    _received_death = false;
    _must_send_death = false;

    for(Location& location : _locations)
        location.was_checked(false);
}

uint8_t GameState::item_with_index(uint16_t received_item_index) const
{
    if(received_item_index < _received_items.size())
        return _received_items[received_item_index];

    Logger::warning("Tried fetching item #" + std::to_string(received_item_index) + " whereas it was never received.");
    return 0xFF;
}

void GameState::set_received_item(uint16_t index, uint8_t item)
{
    if(_received_items.size() <= index)
        _received_items.resize(index+1, 0xFF);
    else
        Logger::warning("Setting a received item without resizing received items array.");

    _received_items[index] = item;
}

std::vector<int64_t> GameState::checked_locations() const
{
    std::vector<int64_t> ret;
    ret.reserve(_locations.size());

    for(const Location& loc : _locations)
        if(loc.was_checked())
            ret.emplace_back((int64_t)loc.id());

    return ret;
}
