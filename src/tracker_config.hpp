#pragma once

#include <string>
#include <map>
#include <vector>
#include <set>
#include "nlohmann/json.hpp"

#define GOAL_BEAT_GOLA 0
#define GOAL_REACH_KAZALT 1
#define GOAL_BEAT_DARK_NOLE 2

class TrackableRegion;
class TrackableItem;

struct TrackerConfig
{
    uint8_t goal = GOAL_BEAT_GOLA;
    bool autofilled_goal = false;

    uint8_t jewel_count = 2;
    bool autofilled_jewel_count = false;

    bool shuffled_trees = false;
    bool autofilled_shuffled_trees = false;

    bool open_trees = false;
    bool autofilled_open_trees = false;

    bool tibor_required = true;
    bool autofilled_tibor_required = false;

    std::string spawn_location = "Massan";
    bool autofilled_spawn_location = false;

    std::string dark_dungeon = "???";
    bool autofilled_dark_dungeon = false;

    bool remove_gumi_boulder = false;
    bool autofilled_remove_gumi_boulder = false;

    bool open_greenmaze_shortcut = false;
    bool autofilled_open_greenmaze_shortcut = false;

    bool allow_whistle_usage_behind_trees = false;
    bool autofilled_allow_whistle_usage_behind_trees = false;

    bool damage_boosting_in_logic = false;
    bool autofilled_damage_boosting_in_logic = false;

    bool enemy_jumping_in_logic = false;
    bool autofilled_enemy_jumping_in_logic = false;

    bool tree_cutting_glitch_in_logic = false;
    bool autofilled_tree_cutting_glitch_in_logic = false;

    bool progressive_armors = true;

    std::map<std::string, std::string> teleport_tree_connections;
    std::set<uint16_t> ignored_locations;

    std::string file_path;

    TrackerConfig() = default;

    void init_teleport_trees(const std::vector<TrackableRegion*>& regions);
    [[nodiscard]] bool item_exists_in_game(TrackableItem* item_id) const;
    [[nodiscard]] const char* goal_internal_string() const;

    void toggle_location_ignored(uint16_t loc_id);

    void build_from_preset(const nlohmann::json& preset_json);
    void save_to_file() const;
    void load_from_file();
};