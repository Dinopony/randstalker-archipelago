#include <iostream>
#include <cmath>
#include <chrono>
#include <thread>

#include "archipelago_interface.hpp"
#include "retroarch_interface.hpp"
#include "game_state.hpp"
#include "user_interface.hpp"
#include <landstalker_lib/constants/item_codes.hpp>

#if !defined WIN32 && !defined _WIN32
#include <poll.h>
#endif

// TODO: Improve AP server logic
//      - Lantern bug?
//      - Shuffled trees logic (and all dynamic logic)
//      - Prevent duplicates in shops if in rebuy mode

// TODO: "Ignored placement of Sword of Gaia after crossing path because there are no more instances of it inside the item pool."

// TODO: Handle hints
//      - Oracle Stone      (currently empty)
//      - Lithograph        (currently empty)
//      - Fortune Teller?   (seems to work..ish)
//      - Foxies?

// TODO: Handle deathlink

// TODO: Handle collection from server (make check flags match with game state at all times)
//      - The problem is that if we reload, local items won't be reobtainable anymore
//      - We need to enforce this only for checks containing non-local items

std::mutex session_mutex;

GameState game_state;
ArchipelagoInterface* archipelago = nullptr;
RetroarchInterface* emulator = nullptr;

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
        std::cerr << "Cannot connect with an empty URI" << std::endl;
        return;
    }

    if(archipelago)
    {
        std::cerr << "Cannot connect when there is an active connection going on. Please use /disconnect first." << std::endl;
        return;
    }

    if(!host.empty() && host.find("ws://") != 0 && host.find("wss://") != 0)
        host = "ws://" + host;

    std::cout << "Attempting to connect to Archipelago server at '" << host << "'..." << std::endl;

    archipelago = new ArchipelagoInterface(host, slot_name, password, &game_state);
}

void disconnect_ap()
{
    delete archipelago;
    archipelago = nullptr;
}

void connect_emu()
{
    try
    {
        if(!emulator)
            emulator = new RetroarchInterface();
    }
    catch(EmulatorException& ex)
    {
        std::cerr << "[ERROR] Connection to emulator failed: " << ex.message() << std::endl;
    }
}

void disconnect_emu()
{
    delete emulator;
    emulator = nullptr;
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
    }

    // Test all location flags to see if player checked new locations since last poll
    const std::vector<Location>& locations = game_state.locations();
    for(const Location& location : locations)
    {
        if(location.was_checked(*emulator))
        {
            if(game_state.set_location_checked_by_player(location.id()))
            {
                std::cout << "Location '" << location.name() << "' is checked!" << std::endl;
            }
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
    if(archipelago->is_connected() && emulator->read_game_byte(ADDR_EQUIPPED_SWORD_EFFECT) == 0x05)
    {
        // Right after beating Gola, the game uses a unique "coin fall" equipped sword effect that is used
        // to detect that we are in the endgame cutscene
        archipelago->notify_game_completed();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}


// =============================================================================================
//      ENTRY POINT
// =============================================================================================

int main(int argc, char** argv)
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
                    std::cerr << "[ERROR] " << ex.message() << std::endl;
                    delete emulator;
                    emulator = nullptr;
                }

                if(archipelago)
                    archipelago->poll();
            }
            session_mutex.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
