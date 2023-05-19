#include <cmath>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <random>
#include <landstalker_lib/constants/item_codes.hpp>

#include "multiworld_interfaces/archipelago_interface.hpp"
#include "multiworld_interfaces/offline_play_interface.hpp"
#include "emulator_interfaces/retroarch_mem_interface.hpp"
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

    nlohmann::json logic_solve_preset = game_state.preset_json();
    if(logic_solve_preset.contains("world"))
    {
        if(logic_solve_preset["world"].contains("itemSources"))
            logic_solve_preset["world"].erase("itemSources");
        if(logic_solve_preset["world"].contains("hints"))
            logic_solve_preset["world"].erase("hints");
    }

    for(TrackableItem* item : ui.trackable_items())
        if(game_state.owned_item_quantity(item->item_id()) > 0)
            logic_solve_preset["gameSettings"]["startingItems"][item->name()] = 1;

    std::ofstream preset_file(SOLVE_LOGIC_PRESET_FILE_PATH);
    preset_file << logic_solve_preset.dump();
    preset_file.close();

    std::string command = "randstalker.exe";
    command += " --preset=\"" SOLVE_LOGIC_PRESET_FILE_PATH "\"";
    command += " --solvelogic";

    std::set<std::string> reachable_locations = invoke_randstalker_to_solve_logic(command);
    for(Location& loc : game_state.locations())
        loc.reachable(reachable_locations.contains(loc.name()));

    std::filesystem::path(SOLVE_LOGIC_PRESET_FILE_PATH).remove_filename();
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

    if(!host.empty() && host.find("ws://") != 0 && host.find("wss://") != 0)
        host = "ws://" + host;

    session_mutex.lock();
    Logger::info("Attempting to connect to Archipelago server at '" + host + "'...");
    game_state.reset();
    multiworld = new ArchipelagoInterface(host, slot_name, password);
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

        if(!multiworld->is_offline_session() && emulator->read_game_long(ADDR_SEED) != game_state.expected_seed())
        {
            delete emulator;
            emulator = nullptr;
            throw EmulatorException("Invalid seed. Please ensure the right ROM was loaded.");
        }

        Logger::info("Successfully connected to Retroarch.");
    }
    catch(EmulatorException& e)
    {
        Logger::error(e.message());
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

static std::string get_output_rom_path()
{
    std::string output_path = std::string(ui.output_rom_path());
    if(!output_path.ends_with("/"))
        output_path += "/";

    if(multiworld && !multiworld->is_offline_session())
        output_path += "AP_" + multiworld->player_name() + "_";
    else
        output_path += "SP_";
    output_path += std::to_string(game_state.expected_seed()) + ".md";

    return output_path;
}

void check_rom_existence()
{
    std::string output_path = get_output_rom_path();
    if(std::filesystem::exists(std::filesystem::path(output_path)))
    {
        game_state.built_rom_path(output_path);
        Logger::info("ROM already found at \"" + output_path + "\", press the \"Rebuild ROM with other settings\" "
                                                               "button if you want to rebuild it anyway.");
    }
}

std::string build_rom()
{
    Logger::info("Building ROM...");

    session_mutex.lock();

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

            nlohmann::json preset_json;
            preset_file >> preset_json;
            preset_file.close();

            game_state.expected_seed(generate_random_seed());
            preset_json["seed"] = game_state.expected_seed();
            game_state.preset_json(preset_json);
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

            nlohmann::json preset_json;
            preset_file >> preset_json;
            preset_file.close();

            // Filter any unwanted / unneeded preset contents
            if(preset_json.contains("world"))
                preset_json.erase("world");
            if(preset_json.contains("playthrough"))
                preset_json.erase("playthrough");
            std::string hash = preset_json.at("hashSentence");
            preset_json.erase("hashSentence");

            // Store this preset as the one that will be used later on to solve logic for the map tracker
            game_state.preset_json(preset_json);

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

    // Save the preset as a internal file that can be used by randstalker.exe
    std::ofstream preset_file(INTERNAL_PRESET_FILE_PATH);
    preset_file << game_state.preset_json().dump();
    preset_file.close();

    std::string output_path = get_output_rom_path();

    session_mutex.unlock();

    ui.save_personal_settings();

    std::string command = "randstalker.exe";
    command += " --inputrom=\"" + std::string(ui.input_rom_path()) + "\"";
    command += " --outputrom=\"" + output_path + "\"";
    command += " --preset=\"" INTERNAL_PRESET_FILE_PATH "\"";
    command += " --nostdin";

    bool success = invoke(command);

#ifndef DEBUG
    std::filesystem::path(INTERNAL_PRESET_FILE_PATH).remove_filename();
#endif

    if(success)
    {
        Logger::info("ROM built successfully at \"" + output_path + "\".");
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
    });

    // UI thread
    ui.open();

    // When UI is closed, tell the other thread to stop working
    keep_working = false;
    process_thread.join();
    return EXIT_SUCCESS;
}
