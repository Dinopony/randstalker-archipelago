#include "game_state.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <landstalker_lib/constants/item_codes.hpp>
#include "data/item_source.json.hxx"
#include "logger.hpp"

using nlohmann::json;

GameState::GameState()
{
    _received_items.reserve(256);

    json input_json = json::parse(ITEM_SOURCES_JSON);
    for(json& location_data : input_json)
        _locations.emplace_back(Location(location_data));
    std::sort(_locations.begin(), _locations.end(), [](Location& loc1, Location& loc2){
        return loc1.name() < loc2.name();
    });
}

void GameState::reset()
{
    _built_rom_path = "";

    _received_items.clear();
    _must_send_checked_locations = false;
    _expected_seed = 0xFFFFFFFF;
    _has_won = false;
    _has_deathlink = false;
    _received_death = false;
    _must_send_death = false;

    for(Location& location : _locations)
        location.reset();
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

Location* GameState::location(const std::string& name)
{
    for(Location& loc : _locations)
        if(loc.name() == name)
            return &loc;
    return nullptr;
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

bool GameState::update_inventory_byte(uint8_t byte_id, uint8_t value)
{
    if(_inventory_bytes[byte_id] == value)
        return false;

    _inventory_bytes[byte_id] = value;
    return true;
}

uint8_t GameState::owned_item_quantity(uint8_t item_id) const
{
    uint8_t byte = (item_id / 2);
    uint8_t upper_half_byte = (item_id % 2 == 1);

    uint8_t byte_value = _inventory_bytes.at(byte);

    uint8_t quantity = (upper_half_byte) ? (byte_value >> 4) : (byte_value & 0x0F);

    // Quantity = 1 means "owned in the past but currently owns zero". In that case, we return 1 anyway as if we still
    // owned the item since we had it at some point, which is valid from a tracking standpoint.
    if(quantity > 1)
        quantity -= 1;

    return quantity;
}