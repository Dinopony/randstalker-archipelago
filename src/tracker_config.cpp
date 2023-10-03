#include "tracker_config.hpp"
#include "trackable_region.hpp"
#include "trackable_item.hpp"
#include <landstalker_lib/constants/item_codes.hpp>
#include <fstream>
#include <sstream>

void TrackerConfig::init_teleport_trees(const std::vector<TrackableRegion*>& regions)
{
    for(TrackableRegion* region : regions)
    {
        if(region->teleport_tree_name().empty())
            continue;
        if(teleport_tree_connections.count(region->teleport_tree_name()) > 0)
            continue;

        // Only add missing teleport trees, and add them connected to themselves by default
        teleport_tree_connections[region->teleport_tree_name()] = region->teleport_tree_name();
    }
}

[[nodiscard]] bool TrackerConfig::item_exists_in_game(TrackableItem* item) const
{
    uint8_t item_id = item->item_id();

    // Depending on the jewel count, some jewels don't exist
    if(item->name() == "Kazalt Jewel")
    {
        if(jewel_count < 6)
            return false;
        if(jewel_count < item->quantity())
            return false;
    }
    else if(item_id == ITEM_RED_JEWEL && (jewel_count < 1 || jewel_count > 5))
        return false;
    else if(item_id == ITEM_PURPLE_JEWEL && (jewel_count < 2 || jewel_count > 5))
        return false;
    else if(item_id == ITEM_GREEN_JEWEL && (jewel_count < 3 || jewel_count > 5))
        return false;
    else if(item_id == ITEM_BLUE_JEWEL && (jewel_count < 4 || jewel_count > 5))
        return false;
    else if(item_id == ITEM_YELLOW_JEWEL && (jewel_count < 5 || jewel_count > 5))
        return false;

    // No Gola items in "reach_kazalt" goal
    if(item_id == ITEM_GOLA_NAIL && goal == GOAL_REACH_KAZALT)
        return false;
    if(item_id == ITEM_GOLA_FANG && goal == GOAL_REACH_KAZALT)
        return false;
    if(item_id == ITEM_GOLA_HORN && goal == GOAL_REACH_KAZALT)
        return false;

    return true;
}

[[nodiscard]] const char* TrackerConfig::goal_internal_string() const
{
    if(goal == GOAL_BEAT_GOLA)
        return "beat_gola";
    else if(goal == GOAL_REACH_KAZALT)
        return "reach_kazalt";
    else if(goal == GOAL_BEAT_DARK_NOLE)
        return "beat_dark_nole";

    // Unreachable, in theory
    return "???";
}

void TrackerConfig::toggle_location_ignored(uint16_t loc_id)
{
    if(ignored_locations.contains(loc_id))
        ignored_locations.erase(loc_id);
    else
        ignored_locations.insert(loc_id);
}

void TrackerConfig::build_from_preset(const nlohmann::json& preset_json)
{
    if(preset_json.contains("gameSettings"))
    {
        if(preset_json["gameSettings"].contains("goal"))
        {
            autofilled_goal = true;
            std::string goal_str = preset_json["gameSettings"]["goal"];

            if(goal_str == "beat_dark_nole")
                goal = GOAL_BEAT_DARK_NOLE;
            else if(goal_str == "reach_kazalt")
                goal = GOAL_REACH_KAZALT;
            else
                goal = GOAL_BEAT_GOLA;
        }

        if(preset_json["gameSettings"].contains("jewelCount"))
        {
            autofilled_jewel_count = true;
            jewel_count = preset_json["gameSettings"]["jewelCount"];
        }

        if(preset_json["gameSettings"].contains("allTreesVisitedAtStart"))
        {
            autofilled_open_trees = true;
            open_trees = preset_json["gameSettings"]["allTreesVisitedAtStart"];
        }

        if(preset_json["gameSettings"].contains("removeTiborRequirement"))
        {
            autofilled_tibor_required = true;
            tibor_required = !(bool) (preset_json["gameSettings"]["removeTiborRequirement"]);
        }

        if(preset_json["gameSettings"].contains("armorUpgrades"))
        {
            progressive_armors = preset_json["gameSettings"]["armorUpgrades"];
        }
    }

    if(preset_json.contains("randomizerSettings"))
    {
        if(preset_json["randomizerSettings"].contains("shuffleTrees"))
        {
            autofilled_shuffled_trees = true;
            shuffled_trees = preset_json["randomizerSettings"]["shuffleTrees"];
        }
    }
}

void TrackerConfig::save_to_file() const
{
    if(file_path.empty())
        return;

    nlohmann::json contents = nlohmann::json::object();

    contents["goal"] = goal;
    contents["autofilled_goal"] = autofilled_goal;
    contents["jewel_count"] = jewel_count;
    contents["autofilled_jewel_count"] = autofilled_jewel_count;
    contents["shuffled_trees"] = shuffled_trees;
    contents["autofilled_shuffled_trees"] = autofilled_shuffled_trees;
    contents["open_trees"] = open_trees;
    contents["autofilled_open_trees"] = autofilled_open_trees;
    contents["tibor_required"] = tibor_required;
    contents["autofilled_tibor_required"] = autofilled_tibor_required;
    contents["spawn_location"] = spawn_location;
    contents["autofilled_spawn_location"] = autofilled_spawn_location;
    contents["dark_dungeon"] = dark_dungeon;
    contents["autofilled_dark_dungeon"] = autofilled_dark_dungeon;
    contents["progressive_armors"] = progressive_armors;
    contents["teleport_tree_connections"] = teleport_tree_connections;
    contents["ignored_locations"] = ignored_locations;

    std::ofstream file(file_path);
    file << contents.dump(4);
    file.close();
}

void TrackerConfig::load_from_file()
{
    if(file_path.empty())
        return;
    std::ifstream file(file_path);
    if(!file)
        return;

    try
    {
        std::stringstream buffer;
        buffer << file.rdbuf();
        nlohmann::json contents = nlohmann::json::parse(buffer.str());
        file.close();

        goal = contents["goal"];
        autofilled_goal = contents["autofilled_goal"];
        jewel_count = contents["jewel_count"];
        autofilled_jewel_count = contents["autofilled_jewel_count"];
        shuffled_trees = contents["shuffled_trees"];
        autofilled_shuffled_trees = contents["autofilled_shuffled_trees"];
        open_trees = contents["open_trees"];
        autofilled_open_trees = contents["autofilled_open_trees"];
        tibor_required = contents["tibor_required"];
        autofilled_tibor_required = contents["autofilled_tibor_required"];
        spawn_location = contents["spawn_location"];
        autofilled_spawn_location = contents["autofilled_spawn_location"];
        dark_dungeon = contents["dark_dungeon"];
        autofilled_dark_dungeon = contents["autofilled_dark_dungeon"];
        progressive_armors = contents["progressive_armors"];
        contents["teleport_tree_connections"].get_to(teleport_tree_connections);
        contents["ignored_locations"].get_to(ignored_locations);
    }
    catch(nlohmann::json::exception&) {}
}