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

public:
    explicit TrackableRegion(const nlohmann::json& json);

    [[nodiscard]] const std::string& name() const { return _name; }
    [[nodiscard]] uint16_t x() const { return _x; }
    [[nodiscard]] uint16_t y() const { return _y; }
    [[nodiscard]] uint16_t width() const { return _width; }
    [[nodiscard]] uint16_t height() const { return _height; }

    void sort_locations();
    [[nodiscard]] const std::vector<Location*>& locations() const { return _locations; }

    [[nodiscard]] bool is_hidden_for_goal(uint8_t goal_id) const { return _hidden_for_goals.count(goal_id); }
};
