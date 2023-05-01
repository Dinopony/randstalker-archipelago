#include <cmath>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <landstalker_lib/constants/item_codes.hpp>

#include "archipelago_interface.hpp"
#include "retroarch_interface.hpp"
#include "game_state.hpp"
#include "user_interface.hpp"
#include "logger.hpp"
#include "randstalker_invoker.hpp"

// TODO: Add a setting to enforce one EkeEke in shops?
// TODO: Add lifestock requirements to logic

// TODO: Handle collection from server (make check flags match with game state at all times)
//      - The problem is that if we reload, local items won't be reobtainable anymore
//      - We need to enforce this only for checks containing non-local items

constexpr uint8_t ITEM_PROGRESSIVE_ARMOR = 69; // 0x45

UserInterface ui;
GameState game_state;
ArchipelagoInterface* archipelago = nullptr;
RetroarchInterface* emulator = nullptr;
std::mutex session_mutex;

constexpr uint16_t ADDR_RECEIVED_ITEM = 0x20;
constexpr uint16_t ADDR_DEATHLINK_STATE = 0x21;
constexpr uint16_t ADDR_IS_IN_GAME = 0x1200;
constexpr uint16_t ADDR_SEED = 0x22;
constexpr uint16_t ADDR_CURRENT_RECEIVED_ITEM_INDEX = 0x107E;
constexpr uint16_t ADDR_EQUIPPED_SWORD_EFFECT = 0x114E;
constexpr uint16_t ADDR_CURRENT_HEALTH = 0x543E;

constexpr uint8_t DEATHLINK_STATE_IDLE = 0;
constexpr uint8_t DEATHLINK_STATE_RECEIVED_DEATH = 1;
constexpr uint8_t DEATHLINK_STATE_WAIT_FOR_RESURRECT = 2;

// =============================================================================================
//      GLOBAL FUNCTIONS (Callable from UI)
// =============================================================================================

void connect_ap(std::string host, const std::string& slot_name, const std::string& password)
{
    if(host.empty())
    {
        Logger::error("Cannot connect with an empty URI");
        return;
    }

    if(archipelago)
    {
        Logger::error("Cannot connect when there is an active connection going on. Please disconnect first.");
        return;
    }

    if(!host.empty() && host.find("ws://") != 0 && host.find("wss://") != 0)
        host = "ws://" + host;

    session_mutex.lock();
    Logger::info("Attempting to connect to Archipelago server at '" + host + "'...");
    archipelago = new ArchipelagoInterface(host, slot_name, password, &game_state);
    session_mutex.unlock();
}

void disconnect_ap()
{
    session_mutex.lock();
    delete archipelago;
    archipelago = nullptr;
    session_mutex.unlock();
}

void connect_emu()
{
    session_mutex.lock();
    try
    {
        if(!emulator)
            emulator = new RetroarchInterface();
    }
    catch(EmulatorException& ex)
    {
        Logger::error("Connection to emulator failed: " + ex.message());
    }
    session_mutex.unlock();
}

void disconnect_emu()
{
    session_mutex.lock();
    delete emulator;
    emulator = nullptr;
    session_mutex.unlock();
}

void poll_archipelago()
{
    if(!archipelago)
        return;

    archipelago->poll();

    if(archipelago->connection_failed())
    {
        delete archipelago;
        archipelago = nullptr;
        return;
    }

    if(!archipelago->is_connected())
        return;

    if(game_state.must_send_checked_locations())
    {
        archipelago->send_checked_locations_to_server(game_state.checked_locations());
        game_state.must_send_checked_locations(false);
    }

    if(game_state.has_won())
        archipelago->notify_game_completed();

    if(game_state.must_send_death())
    {
        archipelago->notify_death();
        game_state.must_send_death(false);
    }
}

void poll_emulator()
{
    if(!emulator)
        return;

    // If no save file is currently loaded, no need to do anything
    if(emulator->read_game_word(ADDR_IS_IN_GAME) == 0x00)
        return;

    if(emulator->read_game_long(ADDR_SEED) != game_state.expected_seed())
    {
        if(game_state.expected_seed() != 0xFFFFFFFF)
        {
            // If seed is 0xFFFFFFFF, it means we did not connect to Archipelago server yet.
            // We only output an error and delete the current emulator interface if we were expecting a specific ROM
            // during gameplay and another one was provided.
            throw EmulatorException("Invalid seed. Please ensure the right ROM was loaded.");
        }
        return;
    }

    // Test all location flags to see if player checked new locations since last poll
    const std::vector<Location>& locations = game_state.locations();
    for(const Location& location : locations)
    {
        if(location.was_checked(*emulator))
        {
            bool location_was_new = game_state.set_location_checked_by_player(location.id());
            if(location_was_new)
                Logger::debug("You checked location " + location.name() + ".");
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
    if(emulator->read_game_byte(ADDR_EQUIPPED_SWORD_EFFECT) == 0x05)
    {
        // Right after beating Gola, the game uses a unique "coin fall" equipped sword effect that is used
        // to detect that we are in the endgame cutscene
        game_state.has_won(true);
    }

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

void build_rom(bool replace_if_exists)
{
    std::string output_path = std::string(ui.output_rom_path());
    if(!output_path.ends_with("/"))
        output_path += "/";
    output_path += std::to_string(game_state.expected_seed()) + ".md";

    if(!replace_if_exists && std::filesystem::exists(std::filesystem::path(output_path)))
    {
        Logger::info("ROM already found at \"" + output_path + "\", press the \"Rebuild ROM\" button if you want "
                                                               "to rebuild it anyway.");
        return;
    }

    Logger::info("Building ROM...");

    std::ofstream preset_file("./presets/_ap_preset.json");
    preset_file << game_state.preset_json().dump(4);
    preset_file.close();

    ui.save_personal_settings();

    std::string command = "randstalker.exe";
    command += " --inputrom=\"" + std::string(ui.input_rom_path()) + "\"";
    command += " --outputrom=\"" + output_path + "\"";
    command += " --preset=_ap_preset";
    command += " --nostdin";

    if(invoke(command))
        Logger::info("ROM built successfully at \"" + output_path + "\".");
    else
        Logger::error("ROM failed to build.");

#ifndef DEBUG
    std::filesystem::path("./presets/_ap_preset.json").remove_filename();
#endif
}

void process_console_input(const std::string& input)
{
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
        for(const Location& loc : game_state.locations())
            loc.mark_as_checked(*emulator);
    }
    else if(archipelago)
    {
        archipelago->say(input);
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
    // Network + game handling thread
    bool keep_working = true;
    std::thread process_thread([&keep_working]() {
        while(keep_working)
        {
            session_mutex.lock();
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

                poll_archipelago();
            }
            session_mutex.unlock();

            uint32_t millis = (game_state.has_won()) ? 500 : 150;
            std::this_thread::sleep_for(std::chrono::milliseconds(millis));
        }
    });

    // UI thread
    ui.open();

    // When UI is closed, tell the other thread to stop working
    keep_working = false;
    process_thread.join();
    return EXIT_SUCCESS;
}
