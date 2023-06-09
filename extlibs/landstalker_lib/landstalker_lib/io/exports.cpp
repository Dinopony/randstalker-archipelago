#include "io.hpp"

#include "../model/entity_type.hpp"
#include "../model/map.hpp"
#include "../model/map_connection.hpp"
#include "../model/world.hpp"

void io::export_items_as_json(const World& world, const std::string& file_path)
{
    Json items_json;
    for(auto& [id, item] : world.items())
        items_json[std::to_string(id)] = item->to_json();
    dump_json_to_file(items_json, file_path);
}

void io::export_entity_types_as_json(const World& world, const std::string& file_path)
{
    Json entity_types_json = Json::object();
    for(auto&[id, entity_type] : world.entity_types())
        entity_types_json[std::to_string(id)] = entity_type->to_json();
    dump_json_to_file(entity_types_json, file_path);
}

void io::export_maps_as_json(const World& world, const std::string& file_path)
{
    Json maps_json = Json::object();
    for(auto& [map_id, map] : world.maps())
        maps_json[std::to_string(map_id)] = map->to_json(world);
    dump_json_to_file(maps_json, file_path);
}

void io::export_map_connections_as_json(const World& world, const std::string& file_path)
{
    Json map_connections_json = Json::array();
    for(const MapConnection& connection : world.map_connections())
        map_connections_json.emplace_back(connection.to_json());
    dump_json_to_file(map_connections_json, file_path);
}

void io::export_map_palettes_as_json(const World& world, const std::string& file_path)
{
    Json map_palettes_json = Json::array();
    for(MapPalette* palette : world.map_palettes())
        map_palettes_json.emplace_back(palette->to_json());
    dump_json_to_file(map_palettes_json, file_path);
}

void io::export_game_strings_as_json(const World& world, const std::string& file_path)
{
    Json strings_json;
    for(uint32_t i=0 ; i<world.game_strings().size() ; ++i)
    {
        std::stringstream hex_id;
        hex_id << "0x" << std::hex << i;
        strings_json[hex_id.str()] = world.game_strings()[i];
    }
    dump_json_to_file(strings_json, file_path);
}

