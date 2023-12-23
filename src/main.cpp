#include <cmath>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <random>
#include <regex>
#include <landstalker_lib/constants/item_codes.hpp>

#include "multiworld_interfaces/archipelago_interface.hpp"
#include "multiworld_interfaces/offline_play_interface.hpp"
#include "emulator_interfaces/retroarch_mem_interface.hpp"
#include "emulator_interfaces/bizhawk_mem_interface.hpp"
#include "game_state.hpp"
#include "user_interface.hpp"
#include "logger.hpp"
#include "randstalker_invoker.hpp"


#ifndef DEBUG
    #ifdef _MSC_VER
        #pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
    #endif
#endif

UserInterface ui;
GameState game_state;
MultiworldInterface* multiworld = nullptr;
EmulatorInterface* emulator = nullptr;
std::mutex session_mutex;

constexpr uint16_t ADDR_RECEIVED_ITEM = 0x0020;                 // 1 byte long
constexpr uint16_t ADDR_DEATHLINK_STATE = 0x0021;               // 1 byte long
constexpr uint16_t ADDR_SEED = 0x0022;                          // 4 bytes long
constexpr uint16_t ADDR_COMPLETION_BYTE = 0x0028;               // 1 byte long
constexpr uint16_t ADDR_IS_IN_GAME = 0x1200;
constexpr uint16_t ADDR_CURRENT_RECEIVED_ITEM_INDEX = 0x107E;
constexpr uint16_t ADDR_CURRENT_HEALTH = 0x543E;

constexpr uint8_t DEATHLINK_STATE_IDLE = 0;
constexpr uint8_t DEATHLINK_STATE_RECEIVED_DEATH = 1;
constexpr uint8_t DEATHLINK_STATE_WAIT_FOR_RESURRECT = 2;

constexpr uint8_t ITEM_PROGRESSIVE_ARMOR = 69; // 0x45
constexpr uint8_t ITEM_ARCHIPELAGO_KAZALT_JEWEL = 70; // 0x46

#define INTERNAL_PRESET_FILE_PATH "./_preset.json"
#define SOLVE_LOGIC_PRESET_FILE_PATH "./_solve_logic.json"

static uint32_t generate_random_seed()
{
    std::random_device seeder;
    std::mt19937 rng(seeder());
    std::uniform_int_distribution<> distribution(INT32_MIN, INT32_MAX);
    return static_cast<uint32_t>(distribution(rng));
}

// =============================================================================================
//      GLOBAL FUNCTIONS (Callable from UI)
// =============================================================================================

void update_map_tracker_logic()
{
    if(!ui.map_tracker_open())
        return;

    nlohmann::json logic_solve_preset;

    logic_solve_preset["gameSettings"]["goal"] = ui.tracker_config().goal_internal_string();
    logic_solve_preset["gameSettings"]["jewelCount"] = ui.tracker_config().jewel_count;
    logic_solve_preset["gameSettings"]["allTreesVisitedAtStart"] = ui.tracker_config().open_trees;
    logic_solve_preset["gameSettings"]["removeTiborRequirement"] = !(ui.tracker_config().tibor_required);
    logic_solve_preset["gameSettings"]["removeGumiBoulder"] = ui.tracker_config().remove_gumi_boulder;
    logic_solve_preset["gameSettings"]["openGreenmazeShortcut"] = ui.tracker_config().open_greenmaze_shortcut;
    logic_solve_preset["gameSettings"]["allowWhistleUsageBehindTrees"] = ui.tracker_config().allow_whistle_usage_behind_trees;

    for(TrackableItem* item : ui.trackable_items())
        if(game_state.owned_item_quantity(item->item_id()) > 0)
            if(item->name() != "Kazalt Jewel")
                logic_solve_preset["gameSettings"]["startingItems"][item->name()] = game_state.owned_item_quantity(item->item_id());

    std::string spawn_location = ui.tracker_config().spawn_location;
    for(char& c : spawn_location)
        c = (c == ' ') ? '_' : (char)tolower(c);
    logic_solve_preset["world"]["spawnLocation"] = spawn_location;

    if(ui.tracker_config().dark_dungeon != "???")
        logic_solve_preset["world"]["darkRegion"] = ui.tracker_config().dark_dungeon;

    // If trees are shuffled, don't pass the "shuffled trees" parameter that would have no effect by itself, but
    // pass the tree connections noted by the player inside the tracker
    if(ui.tracker_config().open_trees && ui.tracker_config().shuffled_trees)
    {
        logic_solve_preset["world"]["teleportTreePairs"] = nlohmann::json::array();
        std::set<std::string> already_processed_trees;
        for(auto& [tree_1, tree_2] : ui.tracker_config().teleport_tree_connections)
        {
            if(tree_1 == tree_2)
                continue;
            if(already_processed_trees.contains(tree_1) || already_processed_trees.contains(tree_2))
                continue;

            already_processed_trees.insert(tree_1);
            already_processed_trees.insert(tree_2);
            std::vector<std::string> tree_pair = { tree_1, tree_2 };
            logic_solve_preset["world"]["teleportTreePairs"].emplace_back(tree_pair);
        }
    }

    std::ofstream preset_file(SOLVE_LOGIC_PRESET_FILE_PATH);
    preset_file << logic_solve_preset.dump();
    preset_file.close();

    std::string command = "randstalker.exe";
    command += " --preset=\"" SOLVE_LOGIC_PRESET_FILE_PATH "\"";
    command += " --solvelogic";

    std::set<std::string> reachable_locations = invoke_randstalker_to_solve_logic(command);
    for(Location& loc : game_state.locations())
        loc.reachable(reachable_locations.contains(loc.name()));

#ifndef DEBUG
    std::filesystem::remove(std::filesystem::path(SOLVE_LOGIC_PRESET_FILE_PATH));
#endif
}

void initiate_solo_session()
{
    session_mutex.lock();
    game_state.reset();
    multiworld = new OfflinePlayInterface();
    session_mutex.unlock();
}

void connect_ap(std::string host, const std::string& slot_name, const std::string& password)
{
    if(host.empty())
    {
        Logger::error("Cannot connect with an empty URI");
        return;
    }

    if(multiworld)
    {
        Logger::error("Cannot connect when there is an active connection going on. Please disconnect first.");
        return;
    }

    session_mutex.lock();
    game_state.reset();

    Logger::info("Attempting to connect to Archipelago server at '" + host + "'...");

    if(host.find("ws://") != 0 && host.find("wss://") != 0)
    {
        // Protocol not given: try both and see which one wins
        ArchipelagoInterface* wss_multiworld = new ArchipelagoInterface("wss://" + host, slot_name, password);
        ArchipelagoInterface* ws_multiworld = new ArchipelagoInterface("ws://" + host, slot_name, password);
        multiworld = nullptr;

        constexpr uint32_t WAIT_MILLIS = 100;
        constexpr uint32_t TIMEOUT_MILLIS = 5000;
        for(uint32_t i=0 ; i<TIMEOUT_MILLIS ; i += WAIT_MILLIS)
        {
            wss_multiworld->poll();
            if(wss_multiworld->is_connected())
            {
                multiworld = wss_multiworld;
                delete ws_multiworld;
                break;
            }

            ws_multiworld->poll();
            if(ws_multiworld->is_connected())
            {
                multiworld = ws_multiworld;
                delete wss_multiworld;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MILLIS));
        }

        if(!multiworld)
        {
            Logger::error("Could not connect to Archipelago server");
            delete ws_multiworld;
            delete wss_multiworld;
        }
    }
    else
    {
        // Protocol given: use the parameters that were explicitly passed
        multiworld = new ArchipelagoInterface(host, slot_name, password);
    }

    session_mutex.unlock();
}

void disconnect_ap()
{
    Logger::info("Disconnecting from Archipelago server...");
    session_mutex.lock();
    delete multiworld;
    multiworld = nullptr;
    delete emulator;
    emulator = nullptr;
    game_state.reset();
    session_mutex.unlock();
}

void connect_emu()
{
    session_mutex.lock();
    try
    {
        emulator = new RetroarchMemInterface();
        Logger::info("Successfully connected to Retroarch.");
    }
    catch(EmulatorException& e)
    {
        emulator = nullptr;
        std::cout << e.message() << std::endl;
    }

    if(!emulator)
    {
        try
        {
            emulator = new BizhawkMemInterface();
            Logger::info("Successfully connected to Bizhawk.");
        }
        catch(EmulatorException& e)
        {
            emulator = nullptr;
            std::cout << e.message() << std::endl;
        }
    }

    if(!emulator)
    {
        Logger::error("Could not find a valid emulator process currently running the game to connect to.");
    }
    else if(!multiworld->is_offline_session() && emulator->read_game_long(ADDR_SEED) != game_state.expected_seed())
    {
        delete emulator;
        emulator = nullptr;
        Logger::error("Invalid seed. Please ensure the right ROM was loaded.");
    }

    session_mutex.unlock();
}

void poll_archipelago()
{
    if(!multiworld)
        return;

    multiworld->poll();

    if(multiworld->connection_failed())
    {
        delete multiworld;
        multiworld = nullptr;
        return;
    }

    if(!multiworld->is_connected())
        return;

    if(game_state.must_send_checked_locations())
    {
        // Send newly checked locations to server
        multiworld->send_checked_locations_to_server(game_state.checked_locations());

        game_state.must_send_checked_locations(false);
    }

    if(game_state.has_won())
        multiworld->notify_game_completed();

    if(game_state.must_send_death())
    {
        multiworld->notify_death();
        game_state.must_send_death(false);
    }
}

void poll_emulator()
{
    // If no save file is currently loaded, no need to do anything
    if(emulator->read_game_word(ADDR_IS_IN_GAME) == 0x00)
        return;

    // Test all location flags to see if player checked new locations since last poll
    for(Location& location : game_state.locations())
    {
        if(location.was_checked())
            continue;

        uint8_t flag_byte_value = emulator->read_game_byte(location.checked_flag_byte());
        uint8_t flag_bit_value = (flag_byte_value >> location.checked_flag_bit()) & 0x1;
        if(flag_bit_value != 0)
        {
            location.was_checked(true);
            game_state.must_send_checked_locations(true);
        }
    }

    // If there are received items that are not yet processed, send the next pending one to the player
    uint16_t current_item_index_in_game = emulator->read_game_word(ADDR_CURRENT_RECEIVED_ITEM_INDEX);
    if(game_state.current_item_index() > current_item_index_in_game)
    {
        if(emulator->read_game_byte(ADDR_RECEIVED_ITEM) == 0xFF)
        {
            uint8_t item_id = game_state.item_with_index(current_item_index_in_game);

            // If the item is a progressive armor, look at the armors owned by the player and give them the next tier.
            // Technically, we *could* send any kind of armor and the next tier would be received in-game, but the
            // item name inside the "Got <ITEM>" textbox when an item is received directly depends on this ID.
            if(item_id == ITEM_PROGRESSIVE_ARMOR)
            {
                uint32_t owned_armors = emulator->read_game_word(0x1044);
                if((owned_armors & 0x2000) == 0)
                    item_id = ITEM_STEEL_BREAST;
                else if((owned_armors & 0x0002) == 0)
                    item_id = ITEM_CHROME_BREAST;
                else if((owned_armors & 0x0020) == 0)
                    item_id = ITEM_SHELL_BREAST;
                else
                    item_id = ITEM_HYPER_BREAST;
            }
            else if(item_id == ITEM_ARCHIPELAGO_KAZALT_JEWEL)
            {
                // Kazalt Jewel over Archipelago is a fake item that needs to be converted into a Red Jewel on reception
                item_id = ITEM_RED_JEWEL;
            }

            emulator->write_game_byte(ADDR_RECEIVED_ITEM, item_id);
        }
    }

    // Check goal completion
    if(emulator->read_game_byte(ADDR_COMPLETION_BYTE) == 0x01)
    {
        game_state.has_won(true);
        emulator->write_game_byte(ADDR_COMPLETION_BYTE, 0x00);
    }

    // Update inventory bytes for the item tracker
    bool inventory_changed = false;
    constexpr uint32_t INVENTORY_START_ADDR = 0xFF1040;
    for(uint8_t i=0 ; i<0x20 ; ++i)
    {
        uint8_t byte_value = emulator->read_game_byte(INVENTORY_START_ADDR + i);
        if(game_state.update_inventory_byte(i, byte_value))
            inventory_changed = true;
    }

    // If any of the inventory values changed, update logic for the map tracker
    if(inventory_changed)
        update_map_tracker_logic();

    // Handle deathlink, both ways
    if(game_state.has_deathlink())
    {
        // If another player died and we received the death notification, schedule a death
        if(game_state.received_death() && emulator->read_game_byte(ADDR_DEATHLINK_STATE) == DEATHLINK_STATE_IDLE)
        {
            Logger::debug("Processing received death...");
            emulator->write_game_byte(ADDR_DEATHLINK_STATE, DEATHLINK_STATE_RECEIVED_DEATH);
            game_state.received_death(false);
        }

        // If player just died, send a death notification to other players
        if(emulator->read_game_word(ADDR_CURRENT_HEALTH) == 0x0000)
        {
            // Check that this death wasn't caused by a recent received death or already processed
            if(!game_state.must_send_death() && emulator->read_game_byte(ADDR_DEATHLINK_STATE) == DEATHLINK_STATE_IDLE)
            {
                Logger::debug("Player death detected");
                emulator->write_game_byte(ADDR_DEATHLINK_STATE, DEATHLINK_STATE_WAIT_FOR_RESURRECT);
                game_state.must_send_death(true);
            }
        }
        else if(emulator->read_game_byte(ADDR_DEATHLINK_STATE) == DEATHLINK_STATE_WAIT_FOR_RESURRECT)
        {
            // Player has life and is in a "post deathlink" state, clear it back to normal to make
            // dying from deathlink and sending deaths possible again
            emulator->write_game_byte(ADDR_DEATHLINK_STATE, DEATHLINK_STATE_IDLE);
        }
    }
}

static std::string get_output_rom_path(uint32_t seed, const std::string& player_name)
{
    std::string output_path = std::string(ui.output_rom_path());
    if(!output_path.ends_with("/"))
        output_path += "/";

    if(!player_name.empty())
        output_path += "AP_" + player_name + "_";
    else
        output_path += "SP_";

    output_path += std::to_string(seed) + ".md";
    return output_path;
}

/**
 * Check if the ROM with the expected output name was already built on a previous connection to the same server.
 * If that is the case, notify the player and "skip" the ROM building window.
 */
void check_rom_existence(uint32_t seed, const std::string& player_name)
{
    std::string output_path = get_output_rom_path(seed, player_name);
    Logger::info("Checking for ROM at path '" + output_path + "'...");

    if(std::filesystem::exists(std::filesystem::path(output_path)))
    {
        game_state.built_rom_path(output_path);
        Logger::info("ROM already found at \"" + output_path + "\", press the \"Rebuild ROM with other settings\" "
                                                               "button if you want to rebuild it anyway.");

        // Load the tracker data from a potential previous seating, since the ROM was already there
        ui.tracker_config().file_path = std::regex_replace(output_path, std::regex("\\.md"), ".json");
        ui.tracker_config().load_from_file();
    }
    else Logger::info("ROM not found!");
}

std::string build_rom()
{
    Logger::info("Building ROM...");

    session_mutex.lock();

    nlohmann::json preset_json;

    if(multiworld->is_offline_session())
    {
        if(ui.offline_generation_mode() == 0)
        {
            // If we are building an offline seed from a preset file, update GameState with the preset JSON contents
            std::ifstream preset_file("./presets/" + ui.selected_preset() + ".json");
            if(!preset_file.is_open())
            {
                Logger::error("Failed to open preset file at './presets/" + ui.selected_preset() + ".json'.");
                session_mutex.unlock();
                return "";
            }

            preset_file >> preset_json;
            preset_file.close();

            uint32_t seed = generate_random_seed();
            game_state.expected_seed(seed);
            preset_json["seed"] = seed;
        }
        else
        {
            // If we are build an offline seed from a permalink, parse the permalink settings first to extract
            // the few required settings for the trackers (goal, jewel count...)
            std::string command = "randstalker.exe";
            command += " --inputrom=\"" + std::string(ui.input_rom_path()) + "\"";
            command += " --permalink=\"" + ui.permalink() + "\"";
            command += " --outputlog=\"" INTERNAL_PRESET_FILE_PATH "\"";
            command += " --outputrom=\"\"";
            command += " --nostdin";

            bool success = invoke(command);
            if(!success)
            {
                Logger::error("Failed to parse permalink, please check it is correct.");
                session_mutex.unlock();
                return "";
            }

            std::ifstream preset_file(INTERNAL_PRESET_FILE_PATH);
            if(!preset_file.is_open())
            {
                Logger::error("Failed to open parsed permalink settings.");
                session_mutex.unlock();
                return "";
            }

            preset_file >> preset_json;
            preset_file.close();

            // Filter any unwanted / unneeded preset contents
            if(preset_json.contains("world"))
                preset_json.erase("world");
            if(preset_json.contains("playthrough"))
                preset_json.erase("playthrough");
            std::string hash = preset_json.at("hashSentence");
            preset_json.erase("hashSentence");

            // Store a fake seed number since we cannot know the real seed for sure (e.g. permalinks with forbidden
            // spoiler log output). This fake seed is built from the hash characters and is used to make a unique
            // output ROM filename.
            uint32_t hash_as_number = 0;
            uint32_t exponent = 1;
            for(uint8_t c : hash)
            {
                uint32_t c_as_number = static_cast<uint32_t>(c);
                hash_as_number += (c_as_number * exponent);
                exponent *= 2;
            }
            game_state.expected_seed(hash_as_number);
        }
    }
    else
    {
        ArchipelagoInterface* archipelago = reinterpret_cast<ArchipelagoInterface*>(multiworld);
        if(archipelago->locations_data().empty())
        {
            Logger::error("Client is still waiting for data from the server. Please wait before trying again.");
            session_mutex.unlock();
            return "";
        }

        preset_json = build_preset_json(archipelago->slot_data(), archipelago->locations_data(), archipelago->player_name());
    }

    std::string dump = preset_json.dump(4);

    std::string output_path = get_output_rom_path(game_state.expected_seed(), multiworld->player_name());

    // Autofill all tracker settings that can be deduced from the preset
    ui.tracker_config().build_from_preset(preset_json);
    ui.tracker_config().file_path = std::regex_replace(output_path, std::regex("\\.md"), ".json");

    // Remove shuffle trees if teleport tree pairs are explicitly defined
    if(preset_json.contains("world") && preset_json["world"].contains("teleportTreePairs"))
        preset_json["randomizerSettings"]["shuffleTrees"] = false;

    // Save the preset as a internal file that can be used by randstalker.exe
    std::ofstream preset_file(INTERNAL_PRESET_FILE_PATH);
    preset_file << preset_json.dump();
    preset_file.close();

    session_mutex.unlock();

    ui.save_personal_settings();

    std::string command = "randstalker.exe";
    command += " --inputrom=\"" + std::string(ui.input_rom_path()) + "\"";
    command += " --outputrom=\"" + output_path + "\"";
    command += " --preset=\"" INTERNAL_PRESET_FILE_PATH "\"";
    command += " --nostdin";

    bool success = invoke(command);

#ifndef DEBUG
    std::filesystem::remove(std::filesystem::path(INTERNAL_PRESET_FILE_PATH));
#endif

    if(success)
    {
        Logger::info("ROM built successfully at \"" + output_path + "\".");
        ui.tracker_config().save_to_file();
        return output_path;
    }
    else
    {
        Logger::error("ROM failed to build.");
        return "";
    }
}

void process_console_input(const std::string& input)
{
#ifdef DEBUG
    if(input == "!senddeath" && game_state.has_deathlink())
    {
        Logger::debug("Fake death queued for sending");
        game_state.must_send_death(true);
    }
    else if(input == "!receivedeath" && game_state.has_deathlink())
    {
        Logger::debug("Fake death registered as received");
        game_state.received_death(true);
    }
    else if(input == "!giveallitems" && emulator)
    {
        Logger::debug("Giving all items...");
        for(uint16_t addr = 0x1040 ; addr <= 0x105E ; ++addr)
            emulator->write_game_byte(addr, 0x22);
    }
    else if(input == "!infinitegold" && emulator)
    {
        Logger::debug("Giving lots of gold...");
        emulator->write_game_word(0x120E, 9999);
    }
    else if(input == "!collectallchecks" && emulator)
    {
        Logger::debug("Collecting all checks...");
        for(Location& loc : game_state.locations())
        {
            uint8_t flag_byte_value = emulator->read_game_byte(loc.checked_flag_byte());
            uint8_t or_mask = 0x1 << loc.checked_flag_bit();
            emulator->write_game_byte(loc.checked_flag_byte(), flag_byte_value | or_mask);
            loc.was_checked(true);
        }
    }
    else
#endif
    if(input == "/about")
    {
        Logger::info("About Randstalker Archipelago Client v" RELEASE);
        Logger::message("Development of Randstalker, this client and the whole Landstalker integration in Archipelago");
        Logger::message("-> Dinopony");
        Logger::message("");

        Logger::message("\"Where is it?\" checks screenshots");
        Logger::message("-> Lucy");
        Logger::message("-> Wiz");
        Logger::message("");

        Logger::message("Testing");
        Logger::message("-> Hawkrex");
        Logger::message("-> Lucy");
        Logger::message("-> Sagaz");
        Logger::message("-> Wiz");
        Logger::message("");

        Logger::message("Thanks for playing :)");
    }
    else if(multiworld)
    {
        multiworld->say(input);
    }
    else
    {
        Logger::error("Command could not be sent to Archipelago server, please connect first.");
    }
}


// =============================================================================================
//      ENTRY POINT
// =============================================================================================

int main()
{
    bool keep_working = true;

    Logger::info("===== Randstalker Archipelago Client v" RELEASE " =====");
    Logger::info("Use /about for full credits");
    Logger::info("Have fun randomizing!");

    // Network + game handling thread
    std::thread process_thread([&keep_working]()
    {
        while(keep_working)
        {
            session_mutex.lock();
            {
                poll_archipelago();

                if(emulator)
                {
                    try
                    {
                        poll_emulator();
                    }
                    catch(EmulatorException& ex)
                    {
                        Logger::error(ex.message());
                        delete emulator;
                        emulator = nullptr;
                    }
                }
            }
            session_mutex.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }

        ui.tracker_config().save_to_file();
    });

    // UI thread
    ui.open();

    // When UI is closed, tell the other thread to stop working
    keep_working = false;
    process_thread.join();
    return EXIT_SUCCESS;
}
