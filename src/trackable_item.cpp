#include "trackable_item.hpp"

TrackableItem::TrackableItem(const nlohmann::json& json)
{
    constexpr float ICON_SIZE = 55.f;

    if(json.contains("name"))
        _name = json.at("name");
    if(json.contains("image"))
    {
        std::string image_filename = json.at("image");
        _texture.loadFromFile("images/" + image_filename);
        _texture.setSmooth(true);
    }
    if(json.contains("itemId"))
        _item_id = json.at("itemId");

    if(json.contains("x"))
        _x = json.at("x");
    _x *= ICON_SIZE;

    if(json.contains("y"))
        _y = json.at("y");
    _y *= ICON_SIZE;
}
