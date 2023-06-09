#include "item.hpp"

#include "../constants/offsets.hpp"

Json Item::to_json() const
{
    Json json;
    json["name"] = _name;
    json["maxQuantity"] = _max_quantity;
    json["startingQuantity"] = _starting_quantity;
    json["goldValue"] = _gold_value;
    json["verbOnUse"] = _verb_on_use;
    json["preUseAddress"] = _pre_use_address;
    json["postUseAddress"] = _post_use_address;
    return json;
}

Item* Item::from_json(uint8_t id, const Json& json)
{
    const std::string& name = json.at("name");
    uint8_t max_quantity = json.value("maxQuantity", 1);
    uint8_t starting_quantity = json.value("startingQuantity", 0);
    uint16_t gold_value = json.value("goldValue", 0);
    uint8_t verb_on_use = json.value("verbOnUse", 0);

    return new Item(id, name, max_quantity, starting_quantity, gold_value, verb_on_use);
}

void Item::apply_json(const Json& json)
{
    if(json.contains("name"))
        _name = json.at("name");
    if(json.contains("maxQuantity"))
        _max_quantity = json.at("maxQuantity");
    if(json.contains("startingQuantity"))
        _starting_quantity = json.at("startingQuantity");
    if(json.contains("goldValue"))
        _gold_value = json.at("goldValue");
    if(json.contains("verbOnUse"))
        _verb_on_use = json.at("verbOnUse");
}
