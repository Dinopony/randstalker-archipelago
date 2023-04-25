#include <cmath>
#include <chrono>
#include <thread>

#include "archipelago_interface.hpp"
#include "retroarch_interface.hpp"
#include "game_state.hpp"
#include "user_interface.hpp"
#include "logger.hpp"

// TODO: Add a setting to enforce one EkeEke in shops?
// TODO: "Ignored placement of Sword of Gaia after crossing path because there are no more instances of it inside the item pool."

// TODO: Handle hints
//      - Oracle Stone      (currently empty)
//      - Lithograph        (currently empty)
//      - Fortune Teller?   (seems to work..ish)
//      - Foxies?

// TODO: Shorten some item source names

// TODO: Handle deathlink

// TODO: Handle collection from server (make check flags match with game state at all times)
//      - The problem is that if we reload, local items won't be reobtainable anymore
//      - We need to enforce this only for checks containing non-local items

GameState game_state;
ArchipelagoInterface* archipelago = nullptr;
RetroarchInterface* emulator = nullptr;
std::mutex session_mutex;

constexpr uint16_t ADDR_RECEIVED_ITEM = 0x20;
constexpr uint16_t ADDR_IS_IN_GAME = 0x1200;
constexpr uint16_t ADDR_SEED = 0x22;
constexpr uint16_t ADDR_CURRENT_RECEIVED_ITEM_INDEX = 0x107E;
constexpr uint16_t ADDR_EQUIPPED_SWORD_EFFECT = 0x114E;


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

    if(game_state.server_must_know_checked_locations())
    {
        archipelago->send_checked_locations_to_server(game_state.checked_locations());
        game_state.clear_server_must_know_checked_locations();
    }

    if(game_state.has_won())
        archipelago->notify_game_completed();
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
            emulator->write_game_byte(ADDR_RECEIVED_ITEM, game_state.item_with_index(current_item_index_in_game));
        }
    }

    // Check goal completion
    if(emulator->read_game_byte(ADDR_EQUIPPED_SWORD_EFFECT) == 0x05)
    {
        // Right after beating Gola, the game uses a unique "coin fall" equipped sword effect that is used
        // to detect that we are in the endgame cutscene
        game_state.has_won(true);
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
    UserInterface ui;
    ui.open();

    // When UI is closed, tell the other thread to stop working
    keep_working = false;
    process_thread.join();
    return EXIT_SUCCESS;
}
