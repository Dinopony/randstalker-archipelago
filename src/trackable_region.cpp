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
            Location* loc = game_state.location(loc_name);
            if(loc)
                _locations.emplace_back(loc);
        }
    }
    if(json.contains("hiddenForGoals"))
    {
        for(uint8_t goal_id : json.at("hiddenForGoals"))
            _hidden_for_goals.insert(goal_id);
    }
    if(json.contains("darkDungeonName"))
        _dark_dungeon_name = json.at("darkDungeonName");
    if(json.contains("spawnLocationName"))
        _spawn_location_name = json.at("spawnLocationName");
    if(json.contains("teleportTreeName"))
        _teleport_tree_name = json.at("teleportTreeName");
}

void TrackableRegion::sort_locations(const std::set<uint16_t>& ignored_locations)
{
    auto get_location_value = [ignored_locations](const Location* loc) -> uint8_t
    {
        if(loc->was_checked())
            return 3;
        else if(ignored_locations.contains(loc->id()))
            return 2;
        else if(!loc->reachable())
            return 1;
        return 0;
    };

    auto sorting_func = [get_location_value](const Location* a, const Location* b) -> bool
    {
        int a_value = get_location_value(a);
        int b_value = get_location_value(b);

        if(a_value != b_value)
            return a_value < b_value;
        return a->name() < b->name();
    };

    std::sort(_locations.begin(), _locations.end(), sorting_func);
}
