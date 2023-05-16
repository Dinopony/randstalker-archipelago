#pragma once

#include <string>
#include <vector>
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
    std::vector<const Location*> _locations;

public:
    explicit TrackableRegion(nlohmann::json json);

    [[nodiscard]] const std::string& name() const { return _name; }
    [[nodiscard]] uint16_t x() const { return _x; }
    [[nodiscard]] uint16_t y() const { return _y; }
    [[nodiscard]] uint16_t width() const { return _width; }
    [[nodiscard]] uint16_t height() const { return _height; }

    [[nodiscard]] const std::vector<const Location*>& locations() const { return _locations; }
    [[nodiscard]] uint32_t checked_locations_count() const;
};
