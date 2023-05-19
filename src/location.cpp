#include "location.hpp"

#include <iostream>

constexpr uint16_t BASE_LOCATION_ID = 4000;
constexpr uint16_t BASE_GROUND_LOCATION_ID = BASE_LOCATION_ID + 256;
constexpr uint16_t BASE_SHOP_LOCATION_ID = BASE_GROUND_LOCATION_ID + 30;
constexpr uint16_t BASE_REWARD_LOCATION_ID = BASE_SHOP_LOCATION_ID + 50;

Location::Location(nlohmann::json& location_data)
{
    std::string debug_dump = location_data.dump(4);
    _name = location_data["name"];

    const std::string& type = location_data["type"];
    if(type == "chest")
    {
        uint8_t chest_id = location_data["chestId"];
        _checked_flag_byte = 0x1080 + (chest_id / 8);
        _checked_flag_bit = chest_id % 8;
        _id = BASE_LOCATION_ID + chest_id;
    }
    else if(type == "ground")
    {
        uint8_t ground_id = location_data["groundItemId"];
        _checked_flag_byte = 0x1060 + (ground_id / 8);
        _checked_flag_bit = ground_id % 8;
        _id = BASE_GROUND_LOCATION_ID + ground_id;
    }
    else if(type == "shop")
    {
        uint8_t shop_item_id = location_data["shopItemId"];
        _checked_flag_byte = 0x1064 + (shop_item_id / 8);
        _checked_flag_bit = shop_item_id % 8;
        _id = BASE_SHOP_LOCATION_ID + shop_item_id;
    }
    else if(type == "reward")
    {
        uint8_t reward_id = location_data["rewardId"];
        const std::string& byte_str = location_data["flag"]["byte"];
        _checked_flag_byte = std::stoul(byte_str.substr(2), nullptr, 16);
        _checked_flag_bit = location_data["flag"]["bit"];
        _id = BASE_REWARD_LOCATION_ID + reward_id;
    }
}

void Location::reset()
{
    _was_checked = false;
    _reachable = false;
    _ignored = false;
}
