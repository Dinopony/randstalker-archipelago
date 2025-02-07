#include "preset_builder.hpp"

static json build_game_settings_json(const json& slot_data)
{
    json game_settings = json::object();

    game_settings["archipelagoWorld"] = true;

    const std::array<std::string, 3> GOAL_NAMES = { "beat_gola", "reach_kazalt", "beat_dark_nole" };
    uint8_t goal_id = slot_data["goal"];
    game_settings["goal"] = GOAL_NAMES[goal_id];

    game_settings["jewelCount"] = slot_data["jewel_count"];
    game_settings["armorUpgrades"] = (slot_data["progressive_armors"] == 1);
    game_settings["startingLife"] = 0;
    game_settings["startingGold"] = 0;

    game_settings["startingItems"] = json::object();
    if(slot_data["use_record_book"] == 1)
        game_settings["startingItems"]["Record Book"] = 1;
    if(slot_data["use_spell_book"] == 1)
        game_settings["startingItems"]["Spell Book"] = 1;

    game_settings["fixArmletSkip"] = true;
    game_settings["removeTreeCuttingGlitchDrops"] = true;
    game_settings["consumableRecordBook"] = false;
    game_settings["consumableSpellBook"] = false;

    game_settings["removeGumiBoulder"] = (slot_data["remove_gumi_boulder"] == 1);
    game_settings["openGreenmazeShortcut"] = (slot_data.value("open_greenmaze_shortcut", 0) == 1);
    game_settings["allowWhistleUsageBehindTrees"] = (slot_data["allow_whistle_usage_behind_trees"] == 1);

    game_settings["removeTiborRequirement"] = ((slot_data["teleport_tree_requirements"] == 0)
                                            || (slot_data["teleport_tree_requirements"] == 2));
    game_settings["allTreesVisitedAtStart"] = (slot_data["teleport_tree_requirements"] < 2);

    game_settings["ekeekeAutoRevive"] = (slot_data["revive_using_ekeeke"] == 1);

    const std::array<int, 5> DIFFICULTY_RATES = {
            60,     // Peaceful = 60% HP & Damage
            80,     // Easy     = 80% HP & Damage
            100,    // Normal   = 100% HP & Damage
            140,    // Hard     = 140% HP & Damage
            200,    // Insane   = 200% HP & Damage
    };
    uint8_t difficulty = slot_data["combat_difficulty"];
    game_settings["enemiesDamageFactor"] = DIFFICULTY_RATES[difficulty];
    game_settings["enemiesHealthFactor"] = DIFFICULTY_RATES[difficulty];
    game_settings["enemiesArmorFactor"] = 100;
    game_settings["enemiesGoldsFactor"] = 100;
    game_settings["enemiesDropChanceFactor"] = 100;

    game_settings["healthGainedPerLifestock"] = 1;
    game_settings["fastMenuTransitions"] = true;

    return game_settings;
}

static json build_randomizer_settings_json(const json& slot_data)
{
    json rando_settings = json::object();

    rando_settings["allowSpoilerLog"] = false;

    rando_settings["shuffleTrees"] = (slot_data["shuffle_trees"] == 1);
    rando_settings["enemyJumpingInLogic"] = (slot_data["handle_enemy_jumping_in_logic"] == 1);
    rando_settings["damageBoostingInLogic"] = (slot_data["handle_damage_boosting_in_logic"] == 1);
    rando_settings["treeCuttingGlitchInLogic"] = (slot_data["handle_tree_cutting_glitch_in_logic"] == 1);
    rando_settings["ensureEkeEkeInShops"] = false;

    return rando_settings;
}

static json build_world_json(const json& slot_data, const json& locations_data, const std::string& player_name)
{
    json world = json::object();

    const json& prices = slot_data.at("location_prices");

    world["spawnLocation"] = slot_data["spawn_region"];
    world["darkRegion"] = slot_data.at("dark_region");
    world["itemSources"] = json::object();

    std::map<std::string, json> locations = locations_data;
    for(auto& [item_source_name, data] : locations)
    {
        // This is a hacky fix to extract the alias part from the name in cases where slot name fetched from server
        // looks like "Player Alias (Real Slot Name Causing A Really Long String)".
        // This was causing textbox overflows in some places, and you really don't want that to happen for the game
        // to remain stable. Also, aliases were conflicting with the "is it my item" detection to determine if an
        // item should be a local item or a remote AP item.
        std::string real_player_name = data["player"];
        auto pos = real_player_name.find(" (");
        if (pos != std::string::npos)
            real_player_name = real_player_name.substr(pos + 2, real_player_name.length() - 1 - (pos+2));

        // Ignore the fake "End" location which only serves as a win condition
        if(item_source_name == "End")
            continue;

        std::string item_name = data["item"];
        if(real_player_name == player_name)
        {
            if(item_name == "1 Gold")
                item_name += "s";
            else if(item_name == "Progressive Armor")
                item_name = "Steel Breast";
        }

        json output = { { "item", item_name } };
        if(prices.contains(item_source_name))
            output["price"] = prices.at(item_source_name);
        if(real_player_name != player_name)
            output["player"] = real_player_name;

        world["itemSources"][item_source_name] = output;
    }

    world["teleportTreePairs"] = json::array();
    std::vector<json> pairs = slot_data.at("teleport_tree_pairs");
    for(const json& pair : pairs)
        world["teleportTreePairs"].emplace_back(pair);

    world["hints"] = slot_data["hints"];

    return world;
}

json build_preset_json(const json& slot_data, const json& locations_data, const std::string& player_name)
{
    json preset = json::object();

    preset["gameSettings"] = build_game_settings_json(slot_data);
    preset["randomizerSettings"] = build_randomizer_settings_json(slot_data);
    preset["world"] = build_world_json(slot_data, locations_data, player_name);
    preset["world"]["seed"] = slot_data["seed"];

    return preset;
}
