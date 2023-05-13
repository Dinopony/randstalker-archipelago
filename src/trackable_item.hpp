#pragma once

#include <string>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <utility>

class TrackableItem
{
private:
    std::string _name;
    uint8_t _item_id;
    float _x;
    float _y;

    sf::Texture _texture;

public:
    TrackableItem(std::string name, const std::string& image_name, uint8_t item_id, sf::Vector2f position) :
        _name       (std::move(name)),
        _item_id    (item_id),
        _x          (position.x),
        _y          (position.y)
    {
        _texture.loadFromFile("./images/" + image_name);
        _texture.setSmooth(true);
    }

    const std::string& name() const { return _name; }
    uint8_t item_id() const { return _item_id; }
    float x() const { return _x; }
    float y() const { return _y; }

    uint64_t get_texture_id() const { return _texture.getNativeHandle(); }
};