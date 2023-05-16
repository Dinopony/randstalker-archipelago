#pragma once

#include <string>
#include <utility>
#include <set>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <nlohmann/json.hpp>

class TrackableItem
{
private:
    std::string _name;
    uint8_t _item_id;
    float _x;
    float _y;
    sf::Texture _texture;

public:
    explicit TrackableItem(const nlohmann::json& json);

    const std::string& name() const { return _name; }
    uint8_t item_id() const { return _item_id; }
    float x() const { return _x; }
    float y() const { return _y; }

    uint64_t get_texture_id() const { return _texture.getNativeHandle(); }
};
