#pragma once

#include <string>
#include <vector>
#include <set>
#include <nlohmann/json.hpp>

class Location;

class TrackableRegion
{
private:
    std::string _name;
    uint16_t _x = 0;
    uint16_t _y = 0;
    uint16_t _width = 1;
    uint16_t _height = 1;
    std::vector<Location*> _locations;
    std::set<uint8_t> _hidden_for_goals;
    std::string _dark_dungeon_name = "_";
    std::string _spawn_location_name = "_";
    std::string _teleport_tree_name;

public:
    explicit TrackableRegion(const nlohmann::json& json);

    [[nodiscard]] const std::string& name() const { return _name; }
    [[nodiscard]] uint16_t x() const { return _x; }
    [[nodiscard]] uint16_t y() const { return _y; }
    [[nodiscard]] uint16_t width() const { return _width; }
    [[nodiscard]] uint16_t height() const { return _height; }
    [[nodiscard]] const std::string& dark_dungeon_name() const { return _dark_dungeon_name; }
    [[nodiscard]] const std::string& spawn_location_name() const { return _spawn_location_name; }
    [[nodiscard]] const std::string& teleport_tree_name() const { return _teleport_tree_name; }

    void sort_locations();
    [[nodiscard]] const std::vector<Location*>& locations() const { return _locations; }

    [[nodiscard]] bool is_hidden_for_goal(uint8_t goal_id) const { return _hidden_for_goals.count(goal_id); }
};
