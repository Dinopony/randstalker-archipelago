#include "trackable_region.hpp"
#include "client.hpp"

TrackableRegion::TrackableRegion(const nlohmann::json& json)
{
    if(json.contains("name"))
        _name = json.at("name");
    if(json.contains("x"))
        _x = json.at("x");
    if(json.contains("y"))
        _y = json.at("y");
    if(json.contains("width"))
        _width = json.at("width");
    if(json.contains("height"))
        _height = json.at("height");
    if(json.contains("locations"))
    {
        for(std::string loc_name : json.at("locations"))
        {
            const Location* loc = game_state.location(loc_name);
            if(loc)
                _locations.emplace_back(loc);
        }
    }
    if(json.contains("hidden_for_goals"))
    {
        for(uint8_t goal_id : json.at("hidden_for_goals"))
            _hidden_for_goals.insert(goal_id);
    }
}

uint32_t TrackableRegion::checked_locations_count() const
{
    uint32_t count = 0;
    for(const Location* loc : _locations)
        if(loc->was_checked())
            count += 1;
    return count;
}