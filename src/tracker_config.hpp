#pragma once

#include <string>

#define GOAL_BEAT_GOLA 0
#define GOAL_REACH_KAZALT 1
#define GOAL_BEAT_DARK_NOLE 2

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

    std::map<std::string, std::string> teleport_tree_connections;

    TrackerConfig(const std::vector<TrackableRegion*>& regions = {})
    {
        for(TrackableRegion* region : regions)
            if(!region->teleport_tree_name().empty())
                teleport_tree_connections[region->teleport_tree_name()] = region->teleport_tree_name();
    }

    [[nodiscard]] bool item_exists_in_game(uint8_t item_id) const
    {
        // Depending on the jewel count, some jewels don't exist
        if(item_id == ITEM_RED_JEWEL && jewel_count < 1)
            return false;
        if(item_id == ITEM_PURPLE_JEWEL && jewel_count < 2)
            return false;
        if(item_id == ITEM_GREEN_JEWEL && jewel_count < 3)
            return false;
        if(item_id == ITEM_BLUE_JEWEL && jewel_count < 4)
            return false;
        if(item_id == ITEM_YELLOW_JEWEL && jewel_count < 5)
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

    [[nodiscard]] const char* goal_display_string() const
    {
        if(goal == GOAL_BEAT_GOLA)
            return "Beat Gola";
        else if(goal == GOAL_REACH_KAZALT)
            return "Reach Kazalt";
        else if(goal == GOAL_BEAT_DARK_NOLE)
            return "Beat Dark Nole";

        // Unreachable, in theory
        return "Unknown";
    }

    [[nodiscard]] const char* goal_internal_string() const
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
};