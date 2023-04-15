#include <iostream>
#include <cmath>
#include <limits>

#include "archipelago_interface.hpp"
#include "retroarch_interface.hpp"
#include "game_state.hpp"
#include <landstalker_lib/constants/item_codes.hpp>

#include <chrono>
#include <thread>

#if !defined WIN32 && !defined _WIN32
#include <poll.h>
#endif

// TODO: Handle the following on ASM side:
//      - Set flag 0xFF106A:0 when Gola is beaten to mark endgame
// TODO: Handle game completion on client

// TODO: Improve ground item handling
//      - Add a unique ID (similar to chest ID) to each ground item
//      - Make a flag field being set every time a ground item is taken
//          + Add an option to make ground items takeable only once using this flag (allowing any ground item)

// TODO: Implement imGui UI

// TODO: Handle collection from server (make check flags match with game state at all times)
//      - The problem is that if we reload, local items won't be reobtainable anymore
//      - We need to enforce this only for checks containing non-local items

// TODO: Handle deathlink
// TODO: Ensure resilience to disconnects (keeping GameState intact in the meantime)

std::mutex session_mutex;

GameState game_state;
ArchipelagoInterface* archipelago = nullptr;
RetroarchInterface* emulator = nullptr;

constexpr uint16_t ADDR_RECEIVED_ITEM = 0x20;
constexpr uint16_t ADDR_IS_IN_GAME = 0x1200;
constexpr uint16_t ADDR_SEED = 0x22;
constexpr uint16_t ADDR_CURRENT_RECEIVED_ITEM_INDEX = 0x107E;

void poll_emulator()
{
    // TODO: Enforce this only being called every 1 sec?
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

        // TODO: Collect in-game locations marked as collected inside GameState
    }

    // If there are received items that are not yet processed, send the next pending one to the player
    uint16_t current_item_index_in_game = emulator->read_game_word(ADDR_CURRENT_RECEIVED_ITEM_INDEX);
    if(game_state.current_item_index() > current_item_index_in_game)
    {
        // TODO: See how local items are handled when checking a location.
        //       If server sends local items to client, some filtering needs to be done based on the player who
        //       sent the item (ignore if self).
        if(emulator->read_game_byte(ADDR_RECEIVED_ITEM) == 0xFF)
        {
            emulator->write_game_byte(ADDR_RECEIVED_ITEM, game_state.item_with_index(current_item_index_in_game));
            emulator->write_game_word(ADDR_CURRENT_RECEIVED_ITEM_INDEX, current_item_index_in_game + 1);
        }
    }
}

void connect_ap(std::string uri)
{
    if(uri.empty())
    {
        std::cerr << "Cannot connect with an empty URI" << std::endl;
        return;
    }

    if(archipelago)
    {
        std::cerr << "Cannot connect when there is an active connection going on. Please use /disconnect first." << std::endl;
        return;
    }

    if(!uri.empty() && uri.find("ws://") != 0 && uri.find("wss://") != 0)
        uri = "ws://" + uri;

    std::cout << "Attempting to connect to Archipelago server at '" << uri << "'..." << std::endl;

    archipelago = new ArchipelagoInterface(uri, &game_state);
}

void disconnect_ap()
{
    delete archipelago;
    archipelago = nullptr;
}

void try_retroarch_read()
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

void on_command(const std::string& command)
{
    if (command == "/help") {
        printf("Available commands:\n"
               "  /connect [addr[:port]] - connect to AP server\n"
               "  /disconnect - disconnect from AP server\n");
               // TODO: "  /force-send - send missing items to game, ignoring locks\n"
               // TODO: "  /force-resend - resend all items to game\n"
               // TODO: "  /sync - resync items/locations with AP server\n");
    } else if (command.starts_with("/connect")) {
        connect_ap(command.substr(9));
    } else if (command == "/disconnect") {
        disconnect_ap();
    } else if (command == "/md") {
        try_retroarch_read();
    } else if (command.starts_with("/give")) {
        try
        {
            uint16_t index = game_state.current_item_index();
            game_state.set_received_item(index, std::stoul(command.substr(6)));
        } catch(std::exception&) {
            std::cerr << "Invalid item given" << std::endl;
        }
    } else if (command.find('/') == 0) {
        printf("Unknown command: %s\n", command.c_str());
    } else if (archipelago) {
        archipelago->say(command);
    }
}

int main(int argc, char** argv)
{
    printf("use /connect [<host>] to connect to an AP server\n");

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

    // Console (main) thread
    std::string input;
    do
    {
        std::getline(std::cin, input);
        session_mutex.lock();
        {
            on_command(input);
        }
        session_mutex.unlock();
    }
    while(input != "/exit");

    keep_working = false;
    process_thread.join();

    return 0;
}
