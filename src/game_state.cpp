#include "game_state.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include "data/locations.json.hxx"

using nlohmann::json;

GameState::GameState()
{
    _received_items.reserve(256);

    uint8_t cur_reward_id = 0;
    json input_json = json::parse(LOCATIONS_JSON);
    json output_json = json::array();
    for(json& location_data : input_json)
    {
        Location loc(location_data, cur_reward_id);
        _locations.emplace_back(loc);
        output_json.emplace_back(loc.to_json());
    }

    std::ofstream file("output_locations.json");
    file << output_json.dump(4);
    file.close();
}

uint8_t GameState::item_with_index(uint16_t received_item_index) const
{
    if(received_item_index < _received_items.size())
        return _received_items[received_item_index];

    std::cerr << "[WARNING] Attempting to fetch received item #" << received_item_index << " whereas it was never received." << std::endl;
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
        std::cerr << "[WARNING] Setting a received item without resizing received items array." << std::endl;

    _received_items[index] = item;
    std::cout << "Received " << (uint16_t)item << " from ??? [#" << index << "]" << std::endl;
}